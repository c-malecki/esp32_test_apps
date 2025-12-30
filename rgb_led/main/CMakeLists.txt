idf_component_register(SRCS "main.c" "../lib/rgb_led.c" "../lib/wifi.c" "../lib/http_server.c"
                       INCLUDE_DIRS "." "../lib"
                       REQUIRES esp_driver_ledc esp_driver_gpio bt nvs_flash esp_wifi esp_http_server)
