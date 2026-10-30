#ifndef BOTONES_H
#define BOTONES_H

#include "hardware/sync.h"

#define DELAY_PULS          (30)

//VARIABLES
typedef struct
{
    uint8_t         gpio_pin;
    volatile bool   estado;
}pulsadores_t;

extern pulsadores_t puls_subir;
extern pulsadores_t puls_bajar;
extern pulsadores_t puls_seleccion;
extern pulsadores_t puls_tarar;

//PROTOTIPO FUNCIONES
//Inicializacion pulsadores
void        pulsador_init (pulsadores_t *puls, uint8_t puls_gpio, bool estado);

//Antirrebote
int64_t     debounce_alarm_cb (alarm_id_t id, void *user_data);
void        puls_callback(uint gpio, uint32_t event_mask);
//Limpia bandera
bool pulsador_presionado(pulsadores_t *puls);

#endif