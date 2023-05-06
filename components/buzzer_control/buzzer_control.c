#include "buzzer_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#define LOCAL_LOG_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "driver/dac_continuous.h"
#include <math.h>
#include "esp_timer.h"

#define TIMER_ALARM_COUNT 10
#define MAX_KEYFRAME_COUNT 32
#define BUZZER_DAC_POS_CHAN DAC_CHANNEL_1
#define BUZZER_DAC_NEG_CHAN DAC_CHANNEL_2
#define TASK_N_QUIT (1ULL << 1)
#define TASK_N_RESET (1ULL << 2)

#define ERROR_CHECK_RETURN(action) {esp_err_t ret = action; if(ret != ESP_OK) { return ret; }} 

static const char *TAG = "BUZZER_CONTROL";

static buzzer_pattern_t *current_pattern = NULL;
static buzzer_keyframe_t *current_keyframe = NULL;
static TaskHandle_t buzzer_task_handle;
static int current_keyframe_idx = 0;
static int64_t next_frame_time_us = 0;
static uint32_t play_progress = 0;
static uint32_t play_pos = 0;
static uint32_t half_period_ticks = 0;
static uint8_t sine_approx[2048];
static uint8_t saw_approx[2048];
static uint8_t square_approx[2048];
static uint8_t *current_waveform = NULL;

static dac_continuous_handle_t dac_handle;

// static IRAM_ATTR bool timer_isr(void *args)
// {
//     uint8_t dac_level = 127;
//     if (current_waveform != NULL && half_period_ticks > 0)
//     {
//         play_pos = (play_pos + 1) % (half_period_ticks * 2);
//         play_progress = (play_pos * 0xff) / (half_period_ticks * 2);
//         dac_level = current_waveform[play_progress];
//     }

//     if (dac_level != last_dac_level)
//     {
//         dac_level = (dac_level - last_dac_level) / 2 + last_dac_level;
//         ESP_ERROR_CHECK(dac_output_voltage(BUZZER_DAC_POS_CHAN, dac_level));
//         ESP_ERROR_CHECK(dac_output_voltage(BUZZER_DAC_NEG_CHAN, (uint8_t)(255 - dac_level)));
//         last_dac_level = dac_level;
//     }

//     return pdFALSE;
// }

// static void buzzer_set_frequency(uint32_t frequency)
// {
//     half_period_ticks = (TIMER_RES / TIMER_ALARM_COUNT) / (2 * frequency);
// }

static void buzzer_start_play(buzzer_waveform_t waveform)
{
    switch (waveform)
    {
    case BUZZER_WAV_SAW:
        current_waveform = saw_approx;
        break;
    case BUZZER_WAV_SIN:
        current_waveform = sine_approx;
        break;
    case BUZZER_WAV_SQUARE:
        current_waveform = square_approx;
        break;
    default:
        ESP_LOGE(TAG, "Unsupported waveform!");
        current_waveform = NULL;
        break;
    }
}

static void buzzer_stop_play()
{
    current_waveform = NULL;
}

static bool increment_pattern_frame()
{
    if (current_keyframe == NULL)
    {
        return false;
    }

    if (esp_timer_get_time() >= next_frame_time_us)
    {
        current_keyframe_idx++;
        if (current_keyframe_idx == current_pattern->frame_count)
        {
            current_keyframe_idx = 0;

            if (!current_pattern->loop)
            {
                current_keyframe = NULL;
                return true;
            }
        }

        current_keyframe = &current_pattern->key_frames[current_keyframe_idx];

        return true;
    }

    return false;
}

static void buzzer_play_task(void *args)
{
    uint32_t notification = 0;
    bool changed_keyframe = false;
    int64_t current_time_us;

    while (true)
    {
        if (xTaskNotifyWait(1, 1, &notification, 10 / portTICK_PERIOD_MS))
        {
            if (notification & TASK_N_QUIT)
            {
                ESP_LOGI(TAG, "Quitting buzzer loop.");
                break;
            }

            if (notification & TASK_N_RESET)
            {
                ESP_LOGI(TAG, "Resetting");
                current_keyframe_idx = 0;
                if (current_pattern != NULL)
                {
                    current_keyframe = &current_pattern->key_frames[0];
                }
                else
                {
                    current_keyframe = NULL;
                }

                next_frame_time_us = 0;
                changed_keyframe = true;
            }
        }
        else
        {
            if (current_pattern != NULL && increment_pattern_frame())
            {
                changed_keyframe = true;
            }
        }

        if (changed_keyframe)
        {
            buzzer_stop_play();
            vTaskDelay(10 / portTICK_PERIOD_MS);

            current_time_us = esp_timer_get_time();
            if (current_keyframe != NULL)
            {
                next_frame_time_us = current_time_us + current_keyframe->duration * 1000;

                if (current_keyframe->frequency > 0)
                {
                    // buzzer_set_frequency(current_keyframe->frequency);
                    // buzzer_start_play(current_pattern->waveform);
                }
            }
            changed_keyframe = false;
        }
    }

    vTaskDelete(NULL);
}

static void gen_approx_wavs()
{
    double half_max = 1024;

    for (int i = 0; i < 2048; i++)
    {
        sine_approx[i] = (uint8_t)round(sin(2 * M_PI * (i / 2048.0)) * half_max + half_max);
        saw_approx[i] = i;
        // square_approx[i] = i > 127 ? 255 : 0;
    }
}

esp_err_t buzzer_control_init()
{
    gen_approx_wavs();

    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_ALL,
        .desc_num = 2,
        .buf_size = 2048,
        .freq_hz = 48000,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_APLL,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    ERROR_CHECK_RETURN(dac_continuous_new_channels(&cont_cfg, &dac_handle));

    ERROR_CHECK_RETURN(dac_continuous_enable(dac_handle));

    while(1) {
        ERROR_CHECK_RETURN(dac_continuous_write(dac_handle, sine_approx, 2048, NULL, -1));
    }

    // ERROR_CHECK_RETURN(dac_output_enable(BUZZER_DAC_POS_CHAN));
    // ERROR_CHECK_RETURN(dac_output_enable(BUZZER_DAC_NEG_CHAN));

    // timer_config_t config = {
    //     .divider = TIMER_DIVIDER,
    //     .counter_dir = TIMER_COUNT_UP,
    //     .counter_en = TIMER_PAUSE,
    //     .alarm_en = TIMER_ALARM_EN,
    //     .auto_reload = TIMER_AUTORELOAD_EN,
    // };
    // timer_init(TIMER_GROUP_0, TIMER_0, &config);
    // timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    // timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_COUNT);
    // timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    // timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_isr, NULL, 0);
    // timer_start(TIMER_GROUP_0, TIMER_0);

    // gptimer_config_t config = {
    //     .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    //     .direction = GPTIMER_COUNT_UP,
    //     .resolution_hz = TIMER_RES,
    // };

    // gptimer_handle_t timer_handle = NULL;
    // ERROR_CHECK_RETURN(gptimer_new_timer(&config, &timer_handle));

    // gptimer_event_callbacks_t timer_cbs = {
    //     .on_alarm = timer_isr,
    // };
    // ERROR_CHECK_RETURN(gptimer_register_event_callbacks(timer_handle, &timer_cbs, NULL));
    // ERROR_CHECK_RETURN(gptimer_enable(timer_handle));

    // gptimer_alarm_config_t alarm_config = {
    //     .alarm_count = TIMER_ALARM_COUNT,
    //     .flags = {
    //         .auto_reload_on_alarm = 1
    //     },
    // };

    // ERROR_CHECK_RETURN(gptimer_set_alarm_action(timer_handle, &alarm_config));
    // ERROR_CHECK_RETURN(gptimer_start(timer_handle));

    xTaskCreate(buzzer_play_task, "Buzzer Task", 2048, NULL, 5, &buzzer_task_handle);

    return ESP_OK;
}

esp_err_t buzzer_control_play_pattern(buzzer_pattern_t *pattern)
{
    current_pattern = pattern;
    xTaskNotify(buzzer_task_handle, TASK_N_RESET, eSetBits);

    return ESP_OK;
}

void buzzer_control_deinit()
{
    xTaskNotify(buzzer_task_handle, 1, eSetBits);

    current_pattern = NULL;
}
