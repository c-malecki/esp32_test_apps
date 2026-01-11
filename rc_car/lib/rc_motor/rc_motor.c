#include "rc_motor.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define MOTOR_AIN1_P26 GPIO_NUM_26
#define MOTOR_AIN2_P25 GPIO_NUM_25

#define MOTOR_BIN1_P4 GPIO_NUM_4
#define MOTOR_BIN2_P16 GPIO_NUM_16

#define MOTOR_STBY_P14 GPIO_NUM_14

static const char *TAG = "RC_MOTOR";

void motor_init(void) {
  ledc_timer_config_t timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                               .duty_resolution = LEDC_TIMER_8_BIT,
                               .timer_num = LEDC_TIMER_0,
                               .freq_hz = 1000,
                               .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&timer);

  ledc_channel_config_t ch1 = {.gpio_num = MOTOR_AIN1_P26,
                               .speed_mode = LEDC_LOW_SPEED_MODE,
                               .channel = LEDC_CHANNEL_0,
                               .timer_sel = LEDC_TIMER_0,
                               .duty = 0,
                               .hpoint = 0};
  ledc_channel_config(&ch1);

  ledc_channel_config_t ch2 = {.gpio_num = MOTOR_AIN2_P25,
                               .speed_mode = LEDC_LOW_SPEED_MODE,
                               .channel = LEDC_CHANNEL_1,
                               .timer_sel = LEDC_TIMER_0,
                               .duty = 0,
                               .hpoint = 0};
  ledc_channel_config(&ch2);

  ledc_channel_config_t ch3 = {.gpio_num = MOTOR_BIN1_P4,
                               .speed_mode = LEDC_LOW_SPEED_MODE,
                               .channel = LEDC_CHANNEL_2,
                               .timer_sel = LEDC_TIMER_0,
                               .duty = 0,
                               .hpoint = 0};
  ledc_channel_config(&ch3);

  ledc_channel_config_t ch4 = {.gpio_num = MOTOR_BIN2_P16,
                               .speed_mode = LEDC_LOW_SPEED_MODE,
                               .channel = LEDC_CHANNEL_3,
                               .timer_sel = LEDC_TIMER_0,
                               .duty = 0,
                               .hpoint = 0};
  ledc_channel_config(&ch4);

  gpio_set_direction(MOTOR_STBY_P14, GPIO_MODE_OUTPUT);
  gpio_set_level(MOTOR_STBY_P14, 1);
}

void motor_set_speed(int speed) { // -255 to 255
  if (speed > 0) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, speed);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, speed);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
  } else {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, -speed);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, -speed);
  }
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);

  ESP_LOGI(TAG, "Motor control write: %d", speed);
}

void motor_brake(void) { gpio_set_level(MOTOR_STBY_P14, 0); }

void motor_resume(void) { gpio_set_level(MOTOR_STBY_P14, 1); }

void motor_stop(void) { motor_set_speed(0); }

// void init_pins() {
//   gpio_reset_pin(MOTOR_AIN1_P26);
//   gpio_reset_pin(MOTOR_AIN2_P25);

//   gpio_reset_pin(MOTOR_BIN1_P4);
//   gpio_reset_pin(MOTOR_BIN2_P16);

//   gpio_reset_pin(MOTOR_STBY_P14);

//   gpio_set_direction(MOTOR_AIN1_P26, GPIO_MODE_OUTPUT);
//   gpio_set_direction(MOTOR_AIN2_P25, GPIO_MODE_OUTPUT);

//   gpio_set_direction(MOTOR_BIN1_P4, GPIO_MODE_OUTPUT);
//   gpio_set_direction(MOTOR_BIN2_P16, GPIO_MODE_OUTPUT);

//   gpio_set_direction(MOTOR_STBY_P14, GPIO_MODE_OUTPUT);

// gpio_set_level(MOTOR_STBY_P14, 1);
// }