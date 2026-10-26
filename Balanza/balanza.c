#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "balanza.h"

// --- CONSTANTES ELÉCTRICAS Y CALIBRACIÓN ---
const float VREF_PICO = 3.3f;
const float CONVERSION_FACTOR = 3.3f / 4095.0f; // El ADC de la Pico en C es de 12 bits (0-4095)

const float PESO_OBJETO_PRUEBA_GRAMOS = 1000.0f;
const float VARIACION_VOLTAJE_MEDIDA = 0.200f;
const float FACTOR_GRAMOS_POR_VOLTIO = PESO_OBJETO_PRUEBA_GRAMOS / VARIACION_VOLTAJE_MEDIDA;

// --- VARIABLES FILTRO DIGITAL ---
float gramos_filtrados = 0.0f;
const float alfa = 0.05f;
int peso_fijo_pantalla = 0;
float voltaje_tara = 0.0f;

// --- FUNCIONES DE LECTURA ---
float leer_voltaje_promedio(int muestras)
{
    uint32_t suma_raw = 0;
    for (int i = 0; i < muestras; i++) {
        suma_raw += adc_read();
        sleep_us(100); // Pequeña pausa entre muestras para estabilidad
    }
    return ((float)suma_raw / muestras) * CONVERSION_FACTOR;
}