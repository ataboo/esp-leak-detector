#define LOCAL_LOG_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hydro_sensor.h"

#define ADC_CHANNEL ADC_CHANNEL_0
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_11
#define ADC_BIT_WIDTH ADC_BITWIDTH_12
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1

#define ADC_MAX_VALUE (1UL<<ADC_BIT_WIDTH)

#define LOW_LEVEL_THRESHOLD (ADC_MAX_VALUE * 15 / 16)
#define MED_LEVEL_THRESHOLD (ADC_MAX_VALUE * 3 / 4)
#define HIGH_LEVEL_THRESHOLD (ADC_MAX_VALUE / 2)

static int adc_raw;
static adc_oneshot_unit_handle_t adc1_handle;

static const char *TAG = "HYDRO_SENSOR";

esp_err_t init_hydro_sensor() {
    if(adc1_handle != NULL) {
        ESP_LOGE(TAG, "hydrosensor already initialized");
        return ESP_FAIL;
    }

    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if(ret != ESP_OK) {
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BIT_WIDTH, 
        .atten = ADC_ATTEN
    };

    ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &chan_config);
    if(ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

hydro_level_t read_hydro_sensor() {

    esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw);
    if(ret != ESP_OK) {
        return HYDRO_LEVEL_ERR;
    }

    ESP_LOGI(TAG, "Raw read: %d", adc_raw);

    ESP_LOGI(TAG, "Low: %lu, Med: %lu, High: %lu", LOW_LEVEL_THRESHOLD, MED_LEVEL_THRESHOLD, HIGH_LEVEL_THRESHOLD);

    if(adc_raw > LOW_LEVEL_THRESHOLD) {
        ESP_LOGI(TAG, "Reading ok");
        return HYDRO_LEVEL_OK;
    }

    if(adc_raw > MED_LEVEL_THRESHOLD) {
        ESP_LOGI(TAG, "Reading low");
        return HYDRO_LEVEL_LOW;
    }

    if(adc_raw > HIGH_LEVEL_THRESHOLD) {
        return HYDRO_LEVEL_MED;
    }

    return HYDRO_LEVEL_HIGH;
}
