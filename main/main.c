/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "wifi_time.h"
#include "time.h"
#include "esp_timer.h"
#include "hydro_sensor.h"
#include "buzzer_control.h"
#include "buzzer_music.h"

static const char* TAG = "LEAK_DETECTOR";

// static time_t now = 0;
// static struct tm timeinfo;
static time_t last_clock_update_micros;

static hydro_level_t sensor_level;

static void update_internal_clock()
{
    esp_err_t ret = blocking_update_time();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to update time");
    }
    else
    {
        ESP_LOGD(TAG, "successfully updated clock");
        last_clock_update_micros = esp_timer_get_time();
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    
    // wifi_time_init();
    // update_internal_clock();
    // time(&now);
    // localtime_r(&now, &timeinfo);
    // ESP_LOGI(TAG, "Updating for time: %.2d:%.2d", timeinfo.tm_hour, timeinfo.tm_min);

    ESP_ERROR_CHECK_WITHOUT_ABORT(buzzer_control_init());

    ESP_ERROR_CHECK(init_hydro_sensor());

    buzzer_pattern_t* music;
    parse_music_str("o5l2cr2c", &music);
    music->waveform = BUZZER_WAV_SQUARE;
    music->loop = false;

    ESP_ERROR_CHECK(buzzer_control_play_pattern(music));

    while(true) {
        sensor_level = read_hydro_sensor();
        if(sensor_level == HYDRO_LEVEL_ERR) {
            ESP_LOGI(TAG, "failed to read sensor");
        } else {
            ESP_LOGD(TAG, "sensor level: %d", sensor_level);

            switch(sensor_level) {
                case HYDRO_LEVEL_OK:
                    break;
                case HYDRO_LEVEL_LOW:
                    break;
                case HYDRO_LEVEL_MED:
                    break;
                case HYDRO_LEVEL_HIGH:
                    break;
                default:
                    ESP_LOGE(TAG, "level not supported");
            }
        }

        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
