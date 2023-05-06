#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    HYDRO_LEVEL_ERR = -1,
    HYDRO_LEVEL_OK = 0,
    HYDRO_LEVEL_LOW,
    HYDRO_LEVEL_MED,
    HYDRO_LEVEL_HIGH,
} hydro_level_t;

esp_err_t init_hydro_sensor();

hydro_level_t read_hydro_sensor();
