#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hydro_sensor.h"
#include "buzzer_control.h"
#include "buzzer_music.h"
#include "c3_led_blink.h"

static const char* TAG = "LEAK_DETECTOR";
static hydro_level_t sensor_level;
static buzzer_pattern_t** patterns;

static buzzer_pattern_t* start_pattern;

static void init_buzzer_patterns() {
    patterns = (buzzer_pattern_t**)calloc(4, sizeof(buzzer_pattern_t*));

    patterns[HYDRO_LEVEL_OK] = NULL;
    ESP_ERROR_CHECK(parse_music_str("o4l2cr2c", &patterns[HYDRO_LEVEL_LOW]));
    ESP_ERROR_CHECK(parse_music_str("o5l2co4f#", &patterns[HYDRO_LEVEL_MED]));
    ESP_ERROR_CHECK(parse_music_str("l4o6cf#o7co6f#c", &patterns[HYDRO_LEVEL_HIGH]));

    for(int i=1; i<=HYDRO_LEVEL_HIGH; i++) {
        patterns[i]->waveform = BUZZER_WAV_SAW;
        patterns[i]->loop = false;
    }

    ESP_ERROR_CHECK(parse_music_str("o5l4cego6c", &start_pattern));
    start_pattern->waveform = BUZZER_WAV_SQUARE;
    start_pattern->loop = false;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_ERROR_CHECK_WITHOUT_ABORT(buzzer_control_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(c3_led_blink_init());

    init_buzzer_patterns();

    ESP_ERROR_CHECK(init_hydro_sensor());
    ESP_ERROR_CHECK(buzzer_control_play_pattern(start_pattern));

    ESP_ERROR_CHECK(c3_blink_color(255, 0, 0, 400));
    vTaskDelay(400 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(c3_blink_color(0, 255, 0, 400));
    vTaskDelay(400 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(c3_blink_color(0, 0, 255, 400));
    vTaskDelay(400 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(c3_stop_blink());

    while(true) {
        sensor_level = read_hydro_sensor();
        if(sensor_level == HYDRO_LEVEL_ERR) {
            ESP_LOGI(TAG, "failed to read sensor");
        } else {
            ESP_LOGD(TAG, "sensor level: %d", sensor_level);

            buzzer_pattern_t* pattern = patterns[sensor_level];
            if(pattern != NULL) {
                buzzer_control_play_pattern(pattern);
            }

            if(sensor_level > HYDRO_LEVEL_OK) {
                c3_blink_color(255, 0, 0, 400);
            } else {
                c3_stop_blink();
            }
        }

        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }

    fflush(stdout);
    esp_restart();
}
