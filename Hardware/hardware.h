#ifndef HARDWARE_H
#define HARDWARE_H

#include "picoOled.h"

// --- CONFIGURACIÓN DE PINES ---
//botones
#define PULS_SUBIR_PIN      (10)
#define PULS_BAJAR_PIN      (11)
#define PULS_SELECCION_PIN  (12)
#define PULS_TARAR_PIN      (13)
//sensor hcsr04
#define ECHO_HCSR04         (14)
#define TRIG_HCSR04         (15)
//i2c Oled
#define PIN_SDA             (16)
#define PIN_SCL             (17)
//servo
#define SIG_SERVO_PIN       (20)
//adc
#define PIN_ADC             (26)


//OTROS
#define I2C_PORT            (i2c0)
#define OLED_ADDR           (0x3C)

#define CANAL_ADC           (0)  // GPIO 26 corresponde al canal 0

//Variable OLED
extern t_OledParams oled;

//Funciones
void init_config();

#endif