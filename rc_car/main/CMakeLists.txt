idf_component_register(SRCS "main.c" "../lib/rc_ble/rc_ble.c" "../lib/rc_motor/rc_motor.c"
                    INCLUDE_DIRS "." "../lib/rc_ble" "../lib/rc_motor"
                    REQUIRES nvs_flash bt esp_driver_ledc esp_driver_gpio)