#include "driver/gpio.h"
#include "driver/ledc.h"

#define GPIO_R GPIO_NUM_23 // pin 37
#define GPIO_G GPIO_NUM_22 // pin 36
#define GPIO_B GPIO_NUM_21 // pin 33

#define CH_R LEDC_CHANNEL_0
#define CH_G LEDC_CHANNEL_1
#define CH_B LEDC_CHANNEL_2

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096

#define LEDC_CLK_SRC LEDC_AUTO_CLK
#define LEDC_FREQUENCY (4000) // Frequency in Hertz. Set frequency at 4 kHz

void ledc_init(void) {
  ledc_timer_config_t timer_cfg = {
    .speed_mode = LEDC_MODE,
    .duty_resolution = LEDC_DUTY_RES,
    .timer_num = LEDC_TIMER,
    .freq_hz = LEDC_FREQUENCY,
    .clk_cfg = LEDC_CLK_SRC,
  };
  ledc_timer_config(&timer_cfg);

  ledc_channel_config_t channel_cfgs[] = {
    {.channel = CH_R,
      .gpio_num = GPIO_R,
      .speed_mode = LEDC_MODE,
      .timer_sel = LEDC_TIMER,
      .duty = 0,
      .hpoint = 0},
    {.channel = CH_G,
      .gpio_num = GPIO_G,
      .speed_mode = LEDC_MODE,
      .timer_sel = LEDC_TIMER,
      .duty = 0,
      .hpoint = 0},
    {.channel = CH_B,
      .gpio_num = GPIO_B,
      .speed_mode = LEDC_MODE,
      .timer_sel = LEDC_TIMER,
      .duty = 0,
      .hpoint = 0},
  };

  for (int i = 0; i < 3; i++) {
    ledc_channel_config(&channel_cfgs[i]);
  }
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  ledc_set_duty(LEDC_MODE, CH_R, r);
  ledc_update_duty(LEDC_MODE, CH_R);

  ledc_set_duty(LEDC_MODE, CH_G, g);
  ledc_update_duty(LEDC_MODE, CH_G);

  ledc_set_duty(LEDC_MODE, CH_B, b);
  ledc_update_duty(LEDC_MODE, CH_B);
}