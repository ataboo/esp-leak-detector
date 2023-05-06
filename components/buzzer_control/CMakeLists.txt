idf_component_register(SRCS "buzzer_control.c" "buzzer_music.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_timer)