idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES hydro_sensor buzzer_control c3_led_blink)
