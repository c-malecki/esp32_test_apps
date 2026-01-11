#include "esp_err.h"
#include "soc/gpio_num.h"
#include <driver/gpio.h>
#include <esp32/rom/ets_sys.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DATA_PIN GPIO_NUM_25
#define TIMER_INTERVAL 2
#define DATA_BITS 40
#define DATA_BYTES (DATA_BITS / 8)

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&mux)

static const char *TAG = "TEMP_HUMID";

/*
  code is mostly me copying/tweaking the dht.c from
  https://github.com/esp-idf-lib/dht/blob/main/dht.c to get a better
  understanding of what is happening

  from ./datasheets/DHT11-Temperature_Humidity.pdf

  Stage 1. set Data Single-bus voltage level from high to low for at least 18ms
  Stage 2. pull up voltage and wait 20-40us (microseconds) for DHT’s response
  Stage 3. DHT sends low for 80us as reponse signal
  Stage 4. DHT sends high for 80us to prep sending data

  begin sending data

  every bit starts with 50us low then length of following high determines 1 or 0
  duration < 50us = 0
  duration > 50us = 1

  total of 40 bits sequentially
  byte array length 5
  bytes 1 and 3 are humidity %
  bytes 2 and 4 are temperature C
  bytes 2 and 4 zero fill
  byte 5 is checksum

  byte_5 == (byte_1 + byte_2 + byte_3 + byte_4) & 0xFF
*/

static esp_err_t await_pin_state(
  gpio_num_t pin, uint32_t timeout, int expected_state, uint32_t *duration) {
  gpio_set_direction(pin, GPIO_MODE_INPUT);

  for (uint32_t i = 0; i < timeout; i += TIMER_INTERVAL) {
    ets_delay_us(TIMER_INTERVAL);
    if (gpio_get_level(pin) == expected_state) {
      if (duration) {
        *duration = i;
        return ESP_OK;
      }
    }
  }

  return ESP_ERR_TIMEOUT;
};

static inline esp_err_t transmit_data(
  gpio_num_t pin, uint8_t data[DATA_BYTES]) {
  esp_err_t err;

  uint32_t low_duration;
  uint32_t high_duration;

  // 1. set voltage from high to low for 20ms
  gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(pin, 0);
  ets_delay_us(20000);

  // 2. pull up voltage and wait 40us
  gpio_set_level(pin, 1);
  ets_delay_us(40);
  err = await_pin_state(pin, 40, 0, NULL);
  if (err != ESP_OK) {
    PORT_EXIT_CRITICAL();
    ESP_LOGE(TAG, "transmit_data: Stage 2 error: %d", err);
    return err;
  }

  // 3. DHT sends low for 80us as reponse signal
  err = await_pin_state(pin, 80, 1, NULL);
  if (err != ESP_OK) {
    PORT_EXIT_CRITICAL();
    ESP_LOGE(TAG, "transmit_data: Stage 3 error: %d", err);
    return err;
  }

  // 4. DHT sends high for 80us to prep sending data
  err = await_pin_state(pin, 80, 0, NULL);
  if (err != ESP_OK) {
    PORT_EXIT_CRITICAL();
    ESP_LOGE(TAG, "transmit_data: Stage 4 error: %d", err);
    return err;
  }

  // read in bits
  for (int i = 0; i < DATA_BITS; i++) {
    err = await_pin_state(pin, 65, 1, &low_duration);
    if (err != ESP_OK) {
      PORT_EXIT_CRITICAL();
      ESP_LOGE(TAG, "transmit_data: low bit timeout");
      return err;
    }

    err = await_pin_state(pin, 75, 0, &high_duration);
    if (err != ESP_OK) {
      PORT_EXIT_CRITICAL();
      ESP_LOGE(TAG, "transmit_data: high bit timeout");
      return err;
    }

    uint8_t b = i / 8;
    uint8_t m = i % 8;
    // if m == 0, initialize element in byte array because it's the start of the
    // next byte otherwise, b will be the index of the byte array we want to use

    // 8 / 8 = 1 (b == 1)
    // 8 % 8 = 0 (m == 0)

    // 10 / 8 = 1 (b == 1)
    // 10 % 8 = 0 (m == 2)
    if (!m) {
      data[b] = 0;
    }

    data[b] |= (high_duration > low_duration) << (7 - m);
  }

  return ESP_OK;
};

static inline int16_t convert_data(uint8_t most_sig_bit) {
  int16_t data = most_sig_bit * 10;
  return data;
}

esp_err_t read_data(gpio_num_t pin, int16_t *humidity, int16_t *temperature) {
  if (!humidity && !temperature) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t data[DATA_BYTES] = {0};

  gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(pin, 1);

  PORT_ENTER_CRITICAL();
  esp_err_t res = transmit_data(pin, data);
  if (res == ESP_OK) {
    PORT_EXIT_CRITICAL();
  }

  gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(pin, 1);

  if (res != ESP_OK) {
    return res;
  }

  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    ESP_LOGE(TAG, "checksum failed");
    return ESP_ERR_INVALID_CRC;
  }

  if (humidity) {
    *humidity = convert_data(data[0]);
  }
  if (temperature) {
    *temperature = convert_data(data[2]);
  }

  ESP_LOGD(TAG, "Data: humidity=%d, temperature=%d", *humidity, *temperature);

  return ESP_OK;
};

esp_err_t read_float_data(gpio_num_t pin, float *humidity, float *temperature) {
  if (!humidity && !temperature) {
    return ESP_ERR_INVALID_ARG;
  }

  int16_t i_humidity, i_temp;

  esp_err_t res =
    read_data(pin, humidity ? &i_humidity : NULL, temperature ? &i_temp : NULL);
  if (res != ESP_OK) {
    return res;
  }

  if (humidity) {
    *humidity = i_humidity / 10.0;
  }
  if (temperature) {
    *temperature = i_temp / 10.0;
  }

  return ESP_OK;
};

void dht11_task(void *param) {
  float temperature, humidity;

  gpio_set_pull_mode(DATA_PIN, GPIO_PULLUP_ONLY);
  vTaskDelay(pdMS_TO_TICKS(2000));

  while (1) {
    if (read_float_data(DATA_PIN, &humidity, &temperature) == ESP_OK)
      printf("Humidity: %.1f%% Temperature: %.1fC\n", humidity, temperature);
    else
      printf("Could not read data from sensor\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
};

void app_main(void) {
  xTaskCreate(dht11_task, "dht11_task", 2048, NULL, 5, NULL);
  // xTaskCreate(
  //   dht11_task, "dht11_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}