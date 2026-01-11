#ifndef RC_MOTOR_H
#define RC_MOTOR_H

#include <stdint.h>

void motor_init(void);
void motor_set_speed(int speed);
void motor_brake(void);
void motor_resume(void);
void motor_stop(void);

#endif