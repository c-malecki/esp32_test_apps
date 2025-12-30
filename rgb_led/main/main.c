#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "rgb_led.h"
#include "wifi.h"

static const char *TAG = "RGB_LED";

#define DEVICE_NAME "RGB_LED"

static void ble_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void app_main(void) {
  esp_err_t esp_err = nvs_flash_init();
  if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err = nvs_flash_init();
  }
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init error; code: %d ", esp_err);
    return;
  }

  ledc_init();

  esp_err = init_wifi_prov();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "init_wifi_prov; error code: %d ", esp_err);
    return;
  }

  set_rgb(255, 0, 0);

  nimble_port_freertos_init(ble_host_task);
}
