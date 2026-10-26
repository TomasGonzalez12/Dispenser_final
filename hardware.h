#ifndef HARDWARE_H
#define HARDWARE_H

#include "Oled/picoOled.h"

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

extern t_OledParams oled;

//VARIABLES 
// --- Constantes electricas y calibracion---
extern const float VREF_PICO;
extern const float CONVERSION_FACTOR;

extern const float PESO_OBJETO_PRUEBA_GRAMOS;
extern const float VARIACION_VOLTAJE_MEDIDA;
extern const float FACTOR_GRAMOS_POR_VOLTIO;

// --- Variables y filtro digital ---
extern float gramos_filtrados;
extern const float alfa;
extern int peso_fijo_pantalla;
extern float voltaje_tara;

//Funciones
void init_config();

#endif