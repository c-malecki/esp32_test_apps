#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>

void ledc_init(void);
void set_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif