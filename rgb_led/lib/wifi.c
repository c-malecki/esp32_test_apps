#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "host/ble_gap.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "http_server.h"
#include "nimble/nimble_port.h"
#include "nvs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

#define WIFI_SERVICE_UUID 0x1800
#define SSID_CHAR_UUID 0x2A00
#define PASS_CHAR_UUID 0x2A01
#define CONNECT_CHAR_UUID 0x2A02

static const char *TAG = "WIFI_GATT";

static httpd_handle_t server = NULL;

static uint8_t own_addr_type;
static char wifi_ssid[WIFI_SSID_MAX_LEN] = {0};
static char wifi_pass[WIFI_PASS_MAX_LEN] = {0};
static bool wifi_connected = false;

static void wifi_event_handler(
  void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "WiFi started, connecting...");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_connected = false;
    wifi_event_sta_disconnected_t *disconn =
      (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGE(TAG, "WiFi disconnected, reason: %d", disconn->reason);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    wifi_connected = true;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    printf("Got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    server = start_webserver();
  }
};

static void wifi_connect(void) {
  wifi_config_t wifi_config = {
    .sta =
      {
        .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK,
        .scan_method = WIFI_ALL_CHANNEL_SCAN,
        .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
      },
  };

  strncpy((char *)wifi_config.sta.ssid, wifi_ssid, WIFI_SSID_MAX_LEN);
  strncpy((char *)wifi_config.sta.password, wifi_pass, WIFI_PASS_MAX_LEN);

  ESP_LOGI(TAG,
    "Connecting - SSID:'%s' Pass:'%s'",
    wifi_config.sta.ssid,
    wifi_config.sta.password);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
};

static void save_wifi_creds(void) {
  nvs_handle_t handle;
  nvs_open("wifi", NVS_READWRITE, &handle);
  nvs_set_str(handle, "ssid", wifi_ssid);
  nvs_set_str(handle, "pass", wifi_pass);
  nvs_commit(handle);
  nvs_close(handle);
  ESP_LOGI(TAG, "WiFi credentials saved to NVS");
}

static bool load_wifi_creds(void) {
  nvs_handle_t handle;
  if (nvs_open("wifi", NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }

  size_t ssid_len = WIFI_SSID_MAX_LEN;
  size_t pass_len = WIFI_PASS_MAX_LEN;

  esp_err_t err1 = nvs_get_str(handle, "ssid", wifi_ssid, &ssid_len);
  esp_err_t err2 = nvs_get_str(handle, "pass", wifi_pass, &pass_len);

  nvs_close(handle);

  if (err1 == ESP_OK && err2 == ESP_OK) {
    ESP_LOGI(TAG, "Loaded WiFi credentials from NVS");
    return true;
  }
  return false;
}

static int ssid_write(uint16_t conn_handle,
  uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctxt,
  void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < WIFI_SSID_MAX_LEN) {
      os_mbuf_copydata(ctxt->om, 0, len, wifi_ssid);
      wifi_ssid[len] = '\0';
      printf("SSID received: %s\n", wifi_ssid);
    }
  }
  return 0;
};

static int pass_write(uint16_t conn_handle,
  uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctxt,
  void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < WIFI_PASS_MAX_LEN) {
      os_mbuf_copydata(ctxt->om, 0, len, wifi_pass);
      wifi_pass[len] = '\0';
      printf("Password received\n");
    }
  }
  return 0;
};

static int connect_write(uint16_t conn_handle,
  uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctxt,
  void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    printf("Connecting to WiFi...\n");
    save_wifi_creds();
    wifi_connect();
  }
  return 0;
};

static int status_read(uint16_t conn_handle,
  uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctxt,
  void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    uint8_t status = wifi_connected ? 1 : 0;
    os_mbuf_append(ctxt->om, &status, sizeof(status));
  }
  return 0;
};

static const struct ble_gatt_svc_def provision_services[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID16_DECLARE(WIFI_SERVICE_UUID),
    .characteristics =
      (struct ble_gatt_chr_def[]){{
                                    .uuid = BLE_UUID16_DECLARE(SSID_CHAR_UUID),
                                    .access_cb = ssid_write,
                                    .flags = BLE_GATT_CHR_F_WRITE,
                                  },
        {
          .uuid = BLE_UUID16_DECLARE(PASS_CHAR_UUID),
          .access_cb = pass_write,
          .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
          .uuid = BLE_UUID16_DECLARE(CONNECT_CHAR_UUID),
          .access_cb = connect_write,
          .flags = BLE_GATT_CHR_F_WRITE,
        },
        {
          .uuid = BLE_UUID16_DECLARE(0x2A03),
          .access_cb = status_read,
          .flags = BLE_GATT_CHR_F_READ,
        },
        {0}},
  },
  {0}};

static void on_stack_sync(void) {
  own_addr_type = BLE_OWN_ADDR_RANDOM;

  ble_hs_id_infer_auto(0, &own_addr_type);

  struct ble_hs_adv_fields fields = {0};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.name = (uint8_t *)"ESP32-RGB";
  fields.name_len = strlen("ESP32-RGB");
  fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params adv_params = {0};
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  ble_gap_adv_start(
    own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
}

int init_wifi_prov(void) {
  esp_err_t esp_err;

  esp_err = esp_netif_init();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init; error code: %d ", esp_err);
    return esp_err;
  }

  esp_err = esp_event_loop_create_default();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "esp_event_loop_create_default; error code: %d ", esp_err);
    return esp_err;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err = esp_wifi_init(&cfg);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_init; error code: %d ", esp_err);
    return esp_err;
  }

  esp_err = esp_event_handler_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "esp_event_handler_register; error code: %d ", esp_err);
    return esp_err;
  }

  esp_err = esp_event_handler_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "esp_event_handler_register; error code: %d ", esp_err);
    return esp_err;
  }

  if (load_wifi_creds()) {
    wifi_connect();
    // idf.py erase-flash to erase stored credentials
  }

  esp_err = nimble_port_init();
  if (esp_err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init; error code: %d ", esp_err);
    return esp_err;
  }

  ble_hs_cfg.sync_cb = on_stack_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  esp_err = ble_gatts_count_cfg(provision_services);
  if (esp_err != 0) {
    ESP_LOGE(TAG, "ble_gatts_count_cfg; error code: %d ", esp_err);
    return esp_err;
  }

  esp_err = ble_gatts_add_svcs(provision_services);
  if (esp_err != 0) {
    ESP_LOGE(TAG, "ble_gatts_add_svcs; error code: %d ", esp_err);
    return esp_err;
  }

  return 0;
}