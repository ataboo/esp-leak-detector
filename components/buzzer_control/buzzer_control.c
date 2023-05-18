#include "buzzer_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#define LOCAL_LOG_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "driver/gptimer.h"
#include <math.h>
#include "esp_timer.h"
#include "driver/gpio.h"

#define TIMER_ALARM_COUNT 20
#define MAX_KEYFRAME_COUNT 32
#define TASK_N_QUIT (1ULL << 1)
#define TASK_N_RESET (1ULL << 2)

#define BUZZER_POS_PIN GPIO_NUM_3

#define OUTPUT_PINS (1ULL<<BUZZER_POS_PIN)

#define ERROR_CHECK_RETURN(action) {esp_err_t ret = action; if(ret != ESP_OK) { return ret; }} 

#define TIMER_RES 1000000

static const char *TAG = "BUZZER_CONTROL";

static buzzer_pattern_t *current_pattern = NULL;
static buzzer_keyframe_t *current_keyframe = NULL;
static TaskHandle_t buzzer_task_handle;
static int current_keyframe_idx = 0;
static int64_t next_frame_time_us = 0;
static uint32_t play_progress = 0;
static uint32_t play_pos = 0;
static uint32_t half_period_ticks = 0;
static bool last_dac_level = 0;
static bool square_bool[255];

static bool *current_waveform = NULL;

static IRAM_ATTR bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    bool dac_level = 0;
    if (current_waveform != NULL && half_period_ticks > 0)
    {
        play_pos = (play_pos + 1) % (half_period_ticks * 2);
        play_progress = (play_pos * 0xff) / (half_period_ticks * 2);
        dac_level = current_waveform[play_progress];
    }

    if (dac_level != last_dac_level)
    {
        gpio_set_level(BUZZER_POS_PIN, (int)dac_level);
        last_dac_level = dac_level;
    }

    return pdFALSE;
}

static void buzzer_set_frequency(uint32_t frequency)
{
    half_period_ticks = (TIMER_RES / TIMER_ALARM_COUNT) / (2 * frequency);
}

static void buzzer_start_play(buzzer_waveform_t waveform)
{
    current_waveform = square_bool;
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
                    buzzer_set_frequency(current_keyframe->frequency);
                    buzzer_start_play(current_pattern->waveform);
                }
            }
            changed_keyframe = false;
        }
    }

    vTaskDelete(NULL);
}

static void gen_approx_wavs()
{
    for (int i = 0; i < 255; i++)
    {
        square_bool[i] = i > 127 ? 0 : 1;
    }
}

esp_err_t buzzer_control_init()
{
    gen_approx_wavs();

    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = OUTPUT_PINS,
    };

    ERROR_CHECK_RETURN(gpio_config(&gpio_cfg));

    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RES,
    };

    gptimer_handle_t timer_handle = NULL;
    ERROR_CHECK_RETURN(gptimer_new_timer(&config, &timer_handle));

    gptimer_event_callbacks_t timer_cbs = {
        .on_alarm = timer_isr,
    };
    ERROR_CHECK_RETURN(gptimer_register_event_callbacks(timer_handle, &timer_cbs, NULL));
    ERROR_CHECK_RETURN(gptimer_enable(timer_handle));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = TIMER_ALARM_COUNT,
        .flags = {
            .auto_reload_on_alarm = 1
        },
    };

    ERROR_CHECK_RETURN(gptimer_set_alarm_action(timer_handle, &alarm_config));
    ERROR_CHECK_RETURN(gptimer_start(timer_handle));

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
