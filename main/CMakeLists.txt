idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_time hydro_sensor esp_timer buzzer_control)
