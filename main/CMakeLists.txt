idf_component_register(SRCS "main.c" "wifi_manager.c" "motion_detector.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event nvs_flash driver json)
