#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <stdio.h>

#define MOTOR_AIN1_P26 GPIO_NUM_26
#define MOTOR_AIN2_P25 GPIO_NUM_25
#define MOTOR_STBY_P14 GPIO_NUM_14
#define MOTOR_BIN1_P4 GPIO_NUM_4
#define MOTOR_BIN2_P16 GPIO_NUM_16

static int prev_speed = 0;
static int cur_speed = 127;

void motor_pwm_init(void) {
  ledc_timer_config_t timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                               .duty_resolution = LEDC_TIMER_8_BIT,
                               .timer_num = LEDC_TIMER_0,
                               .freq_hz = 1000,
                               .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&timer);

  ledc_channel_config_t channel1 = {.gpio_num = MOTOR_AIN1_P26,
                                    .speed_mode = LEDC_LOW_SPEED_MODE,
                                    .channel = LEDC_CHANNEL_0,
                                    .timer_sel = LEDC_TIMER_0,
                                    .duty = 0,
                                    .hpoint = 0};
  ledc_channel_config(&channel1);

  ledc_channel_config_t channel2 = {.gpio_num = MOTOR_AIN2_P25,
                                    .speed_mode = LEDC_LOW_SPEED_MODE,
                                    .channel = LEDC_CHANNEL_1,
                                    .timer_sel = LEDC_TIMER_0,
                                    .duty = 0,
                                    .hpoint = 0};
  ledc_channel_config(&channel2);
}

void motor_set(int speed) {
  // -255 to 255
  if (speed > 0) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, speed);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
  } else {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, -speed);
  }
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void motor_task(void *param) {
  while (1) {
    motor_set(cur_speed);
    vTaskDelay(pdMS_TO_TICKS(3000));

    prev_speed = cur_speed;
    if (prev_speed < 250) {
      cur_speed = cur_speed + 25;
    } else {
      cur_speed = 127;
    }
    ESP_LOGI("MOTOR", "cur_speed: %d; prev_speed: %d", cur_speed, prev_speed);
  }
}

void test_task(void *param) {
  while (1) {
    gpio_set_level(MOTOR_STBY_P14, 1);
    gpio_set_level(MOTOR_AIN1_P26, 1);
    gpio_set_level(MOTOR_AIN2_P25, 0);
    ESP_LOGI("MOTOR", "should be on");
    vTaskDelay(pdMS_TO_TICKS(3000));

    gpio_set_level(MOTOR_STBY_P14, 0);
    gpio_set_level(MOTOR_AIN1_P26, 0);
    gpio_set_level(MOTOR_AIN2_P25, 0);
    ESP_LOGI("MOTOR", "should be off");
  }
}

static void init_pins() {
  gpio_reset_pin(MOTOR_AIN1_P26);
  gpio_reset_pin(MOTOR_AIN2_P25);

  gpio_reset_pin(MOTOR_BIN1_P4);
  gpio_reset_pin(MOTOR_BIN2_P16);

  gpio_reset_pin(MOTOR_STBY_P14);

  ESP_LOGI("MOTOR", "setting GPIOs");

  gpio_set_direction(MOTOR_AIN1_P26, GPIO_MODE_OUTPUT);
  gpio_set_direction(MOTOR_AIN2_P25, GPIO_MODE_OUTPUT);

  gpio_set_direction(MOTOR_BIN1_P4, GPIO_MODE_OUTPUT);
  gpio_set_direction(MOTOR_BIN2_P16, GPIO_MODE_OUTPUT);

  gpio_set_direction(MOTOR_STBY_P14, GPIO_MODE_OUTPUT);

  vTaskDelay(pdMS_TO_TICKS(1000));

  gpio_set_level(MOTOR_STBY_P14, 1);
}

void app_main(void) {
  // ESP_LOGI("MOTOR", "motor_pwm_init");
  // motor_pwm_init();
  init_pins();

  while (1) {
    gpio_set_level(MOTOR_AIN1_P26, 1);
    gpio_set_level(MOTOR_AIN2_P25, 0);

    gpio_set_level(MOTOR_BIN1_P4, 1);
    gpio_set_level(MOTOR_BIN2_P16, 0);

    ESP_LOGI("MOTOR", "FORWARD");

    vTaskDelay(pdMS_TO_TICKS(5000));

    // gpio_set_level(MOTOR_AIN1_P26, 0);
    // gpio_set_level(MOTOR_AIN2_P25, 0);
    // ESP_LOGI("MOTOR", "COAST");
    // vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // xTaskCreate(motor_task, "motor_task", 2048, NULL, 5, NULL);
}
