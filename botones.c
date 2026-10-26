#include "pico/stdlib.h"
#include "hardware.h"
#include "botones.h"

pulsadores_t puls_subir;
pulsadores_t puls_bajar;
pulsadores_t puls_seleccion;

void pulsador_init(pulsadores_t *puls, uint8_t puls_gpio, bool estado)
{
    puls -> gpio_pin = puls_gpio;
    puls -> estado   = estado;

    gpio_init (puls -> gpio_pin);
    gpio_set_dir (puls -> gpio_pin, GPIO_IN);
    gpio_pull_up (puls -> gpio_pin);
    gpio_set_input_hysteresis_enabled (puls -> gpio_pin, true);
}


//Antirrebote botones

int64_t debounce_alarm_cb(alarm_id_t id, void *user_data)
{
    // Recuperamos el puntero al objeto pulsador que disparó la alarma
    pulsadores_t *puls = (pulsadores_t *)user_data;

    // Si pasado el delay el pin sigue en LOW (presionado por el pull-up), confirmamos la pulsación
    if (!gpio_get(puls->gpio_pin)) {
        puls->estado = true; 
        // O puedes activar una bandera general, cambiar el estado de su LED, etc.
    }

    // Limpiamos y re-habilitamos la interrupción para este pin específico
    gpio_acknowledge_irq(puls->gpio_pin, GPIO_IRQ_EDGE_FALL);
    gpio_set_irq_enabled(puls->gpio_pin, GPIO_IRQ_EDGE_FALL, true);

    return 0;   // No se repite la alarma
}

// Callback general de interrupción GPIO
void PULS_SELECCIONallback(uint gpio, uint32_t event_mask)
{
    if (!(event_mask & GPIO_IRQ_EDGE_FALL)) return;
  
    pulsadores_t *puls = NULL;

    // Identificamos cuál de los 3 botones disparó la interrupción
    if (gpio == puls_subir.gpio_pin) {
        puls = &puls_subir;
    } else if (gpio == puls_bajar.gpio_pin) {
        puls = &puls_bajar;
    } else if (gpio == puls_seleccion.gpio_pin) {
        puls = &puls_seleccion;
    } else {
        return; // Si fue otro pin, ignoramos
    }

    // Desactivamos temporalmente la interrupción de este pin para evitar rebotes múltiples
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);

    // Lanzamos la alarma pasando el objeto del botón como user_data
    add_alarm_in_ms(DELAY_PULS, debounce_alarm_cb, puls, true);
}