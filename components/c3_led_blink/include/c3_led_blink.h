#pragma once

#include "esp_err.h"

esp_err_t c3_led_blink_init();

esp_err_t c3_set_color(uint8_t r, uint8_t g, uint8_t b);

esp_err_t c3_blink_color(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms);

esp_err_t c3_stop_blink();
