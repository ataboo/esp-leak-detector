#define LOCAL_LOG_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "c3_led_blink.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TASK_N_QUIT 1ULL<<0
#define TASK_N_RESET 1ULL<<1

#define LED_GPIO GPIO_NUM_8
#define STRIP_RES_HZ 10000000

#define ERROR_CHECK_RETURN(action) {esp_err_t ret = action; if(ret != ESP_OK) { return ret; }} 

static char* TAG = "C3_LED_BLINK";

static led_strip_handle_t led_strip;

static uint32_t blink_period_ms = 500;
static uint8_t blink_r;
static uint8_t blink_g;
static uint8_t blink_b;
static bool blink_on;
static TaskHandle_t blink_task_handle;

static void c3_blink_task(void *args)
{
    uint32_t notification = 0;

    while (true)
    {
        if (xTaskNotifyWait(1, 1, &notification, (blink_period_ms / 2) / portTICK_PERIOD_MS))
        {
            if (notification & TASK_N_QUIT)
            {
                ESP_LOGI(TAG, "Quitting blink loop.");
                led_strip_clear(led_strip);
                break;
            }
        }

        ESP_LOGI(TAG, "Blink on: %d", blink_on);
        if(blink_on) {
            c3_set_color(blink_r, blink_g, blink_b);
        } else {
            led_strip_clear(led_strip);
        }

        blink_on = !blink_on;
    }

    vTaskDelete(NULL);
}

esp_err_t c3_led_blink_init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = STRIP_RES_HZ
    };
    
    ERROR_CHECK_RETURN(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ERROR_CHECK_RETURN(led_strip_clear(led_strip));
    
    return ESP_OK;
}

esp_err_t c3_set_color(uint8_t r, uint8_t g, uint8_t b) {
    ERROR_CHECK_RETURN(led_strip_set_pixel(led_strip, 0, r, g, b));
    ERROR_CHECK_RETURN(led_strip_refresh(led_strip));

    return ESP_OK;
}

esp_err_t c3_blink_color(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms) {
    if(period_ms < 20 || period_ms > 10000) {
        return ESP_ERR_INVALID_ARG;
    }
    
    blink_r = r;
    blink_g = g;
    blink_b = b;
    blink_period_ms = period_ms;

    if(blink_task_handle == NULL) {
        blink_on = true;
        xTaskCreate(c3_blink_task, "Buzzer Task", 2048, NULL, 5, &blink_task_handle);
    }

    return ESP_OK;
}

esp_err_t c3_stop_blink() {
    if(blink_task_handle == NULL) {
        return ESP_FAIL;
    }

    xTaskNotify(blink_task_handle, 1, eSetBits);
    blink_task_handle = NULL;

    return ESP_OK;
}

