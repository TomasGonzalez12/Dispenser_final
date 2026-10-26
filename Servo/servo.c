#include "pico/stdlib.h"
#include "stdint.h"
#include "hardware/pwm.h"
#include "servo.h"

uint slice;  //del PWM
uint channel; //PWM

void angulo_servo(uint8_t angulo)
{
    uint pulso_us = SERVO_MIN_US + (angulo * (SERVO_MAX_US - SERVO_MIN_US) / 180);

    uint16_t level = (pulso_us * (SERVO_TOP + 1)) / SERVO_PERIODO_US;

    pwm_set_chan_level(slice, channel, level);
}
