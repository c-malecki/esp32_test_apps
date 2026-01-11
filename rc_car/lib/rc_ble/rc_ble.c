#include "rc_ble.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "rc_motor.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define DEVICE_NAME "RC_CAR"

static const char *TAG = "RC_BLE";
static void advertise(void);
static uint8_t own_addr_type;

static int motor_write(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint8_t raw = ctxt->om->om_data[0];
    int speed = ((int)raw - 127) * 2;
    if (speed > 255) {
      speed = 255;
    }
    if (speed < -255) {
      speed = -255;
    }
    ESP_LOGI(TAG, "Motor control write: %d", speed);
    motor_set_speed(speed);
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_chr_def rc_var_chars[] = {
    {
        .uuid = BLE_UUID16_DECLARE(RC_MOTOR_CHAR_UUID),
        .access_cb = motor_write,
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    {0}};

static const struct ble_gatt_svc_def rc_car_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(RC_CAR_SVC_UUID),
        .characteristics = rc_var_chars,
    },
    {0}};

static int on_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "connection %s; status=%d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);

    if (event->connect.status != 0) {
      advertise();
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
    advertise();
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
    advertise();
    return 0;
  }
  return 0;
};

static void advertise(void) {
  int nimble_err;

  struct ble_hs_adv_fields adv_fields;

  memset(&adv_fields, 0, sizeof adv_fields);

  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  adv_fields.tx_pwr_lvl_is_present = 1;
  adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  adv_fields.name = (uint8_t *)DEVICE_NAME;
  adv_fields.name_len = strlen(DEVICE_NAME);
  adv_fields.name_is_complete = 1;

  adv_fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(0x1811)};
  adv_fields.num_uuids16 = 1;
  adv_fields.uuids16_is_complete = 1;

  nimble_err = ble_gap_adv_set_fields(&adv_fields);
  if (nimble_err != 0) {
    MODLOG_DFLT(ERROR, "error setting advertisement data; nimble_err=%d\n",
                nimble_err);
    return;
  }

  struct ble_gap_adv_params adv_params;

  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  nimble_err = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                                 &adv_params, on_gap_event, NULL);
  if (nimble_err != 0) {
    MODLOG_DFLT(ERROR, "error enabling advertisement; nimble_err=%d\n",
                nimble_err);
    return;
  }
}

static void on_stack_sync(void) {
  int nimble_err;

  own_addr_type = BLE_OWN_ADDR_RANDOM;

  nimble_err = ble_hs_id_infer_auto(0, &own_addr_type);
  if (nimble_err != 0) {
    MODLOG_DFLT(ERROR, "error determining address type; nimble_err=%d\n",
                nimble_err);
    return;
  }

  uint8_t addr_val[6] = {0};
  nimble_err = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
  if (nimble_err != 0) {
    ESP_LOGI(TAG, "Failed to copy address; nimble_err=%d", nimble_err);
    return;
  }

  ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x", addr_val[5],
           addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);

  advertise();
}

static void on_stack_reset(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void ble_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

int ble_svr_init(void) {
  esp_err_t esp_err = nimble_port_init();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init nimble %d ", esp_err);
    return esp_err;
  }

  ble_hs_cfg.reset_cb = on_stack_reset;
  ble_hs_cfg.sync_cb = on_stack_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  int nimble_err = ble_gatts_count_cfg(rc_car_svcs);
  if (nimble_err != 0) {
    return nimble_err;
  }

  nimble_err = ble_gatts_add_svcs(rc_car_svcs);
  if (nimble_err != 0) {
    return nimble_err;
  }
  if (nimble_err != 0) {
    ESP_LOGE(TAG, "Failed to init GATT server: %d", nimble_err);
    return nimble_err;
  }

  return 0;
}