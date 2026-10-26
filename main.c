#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware.h"
#include "oled.h"
#include "botones.h"
#include "servo.h"
#include "balanza.h"


int main() {
    init_config();
    
    //Bucle Principal
    while (true) {
        float voltaje_actual = leer_voltaje_promedio(40);
        
        // Calculamos la diferencia real de voltaje
        float diferencia_voltaje_real = voltaje_actual - voltaje_tara;
        
        // Convertimos la señal analógica a gramos crudos (?)
        float gramos_instantaneos = diferencia_voltaje_real * FACTOR_GRAMOS_POR_VOLTIO;
        float gramos_actuales = 0.0f;

        // --- TRUCO DE SALTO AL VACÍO OPTIMIZADO ---
        if (gramos_instantaneos > -30.0f && gramos_instantaneos < 30.0f) {
            gramos_actuales = 0.0f;
            gramos_filtrados = 0.0f; // Limpia el residuo del filtro
        } else {
            gramos_actuales = gramos_instantaneos;
        }

        // --- FILTRO DIGITAL SUAVE (EMA) ---
        gramos_filtrados = (alfa * gramos_actuales) + ((1.0f - alfa) * gramos_filtrados);
        int peso_redondeado = (int)roundf(gramos_filtrados);

        // --- HISTÉRESIS INTELIGENTE PARA LA PANTALLA ---
        if (peso_redondeado == 0) {
            peso_fijo_pantalla = 0;
        } else if (abs(peso_redondeado - peso_fijo_pantalla) > 4) {
            peso_fijo_pantalla = peso_redondeado;
        }

        // Imprimir por consola serial
        printf("PESO NETO: %d g | Voltaje: %.3f V\n", peso_fijo_pantalla, voltaje_actual);

        sleep_ms(50);
    }
}
