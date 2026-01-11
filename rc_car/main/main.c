#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "rc_ble.h"
#include "rc_motor.h"
#include <stdio.h>

static const char *TAG = "RC_CAR";

void app_main(void) {
  esp_err_t esp_err = nvs_flash_init();
  if (esp_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      esp_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err = nvs_flash_init();
  }
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", esp_err);
    return;
  }

  motor_init();

  esp_err = ble_svr_init();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init nimble %d ", esp_err);
    return;
  }

  nimble_port_freertos_init(ble_host_task);
}
