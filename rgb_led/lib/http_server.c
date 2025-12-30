#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "rgb_led.h"

static const char *TAG = "HTTP_SERVER";

static esp_err_t root_handler(httpd_req_t *req) {
  const char *html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body { font-family: Arial; text-align: center; margin: 50px; }"
    "input[type='range'] { width: 80%; margin: 10px; }"
    ".color-preview { width: 100px; height: 100px; margin: 20px auto; border: "
    "2px solid #ccc; }"
    "</style>"
    "</head>"
    "<body>"
    "<h1>RGB LED Control</h1>"
    "<div class='color-preview' id='preview'></div>"
    "<label>Red: <span id='r-val'>0</span></label><br>"
    "<input type='range' id='r' min='0' max='255' value='0'><br>"
    "<label>Green: <span id='g-val'>0</span></label><br>"
    "<input type='range' id='g' min='0' max='255' value='0'><br>"
    "<label>Blue: <span id='b-val'>0</span></label><br>"
    "<input type='range' id='b' min='0' max='255' value='0'><br>"
    "<button id='btn'>set color</button>"
    "<script>"
    "function setColor() {"
    "  const r = document.getElementById('r').value;"
    "  const g = document.getElementById('g').value;"
    "  const b = document.getElementById('b').value;"
    "  document.getElementById('r-val').textContent = r;"
    "  document.getElementById('g-val').textContent = g;"
    "  document.getElementById('b-val').textContent = b;"
    "  fetch('/api/color', {"
    "    method: 'POST',"
    "    headers: {'Content-Type': 'application/json'},"
    "    body: JSON.stringify({r: parseInt(r), g: parseInt(g), b: parseInt(b)})"
    "  });"
    "}"
    "function update() {"
    "  const r = document.getElementById('r').value;"
    "  const g = document.getElementById('g').value;"
    "  const b = document.getElementById('b').value;"
    "  document.getElementById('r-val').textContent = r;"
    "  document.getElementById('g-val').textContent = g;"
    "  document.getElementById('b-val').textContent = b;"
    "  document.getElementById('preview').style.backgroundColor = "
    "`rgb(${r},${g},${b})`;"
    "}"
    "document.getElementById('r').oninput = update;"
    "document.getElementById('g').oninput = update;"
    "document.getElementById('b').oninput = update;"
    "document.getElementById('btn').addEventListener('click', setColor);"
    "</script>"
    "</body>"
    "</html>";

  httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t color_handler(httpd_req_t *req) {
  char buf[100];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  cJSON *json = cJSON_Parse(buf);
  if (json == NULL) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  int r = cJSON_GetObjectItem(json, "r")->valueint;
  int g = cJSON_GetObjectItem(json, "g")->valueint;
  int b = cJSON_GetObjectItem(json, "b")->valueint;

  set_rgb(r, g, b);
  ESP_LOGI(TAG, "Set RGB: %d, %d, %d", r, g, b);

  cJSON_Delete(json);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

httpd_handle_t start_webserver(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t root = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = root_handler,
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t color = {
      .uri = "/api/color",
      .method = HTTP_POST,
      .handler = color_handler,
    };
    httpd_register_uri_handler(server, &color);

    ESP_LOGI(TAG, "Web server started");
  }

  return server;
}

// void stop_webserver(httpd_handle_t server) {
//   if (server) {
//     httpd_stop(server);
//   }
// }