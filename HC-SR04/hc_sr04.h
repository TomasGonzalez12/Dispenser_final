#ifndef HC_SR04_H
#define HC_SR04_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 *  HC-SR04 no bloqueante
 *
 *  - hcsr04_actualizar() se llama en cada vuelta del bucle principal.
 *    Dispara una medición cada HCSR04_PERIODO_MS y no espera al eco.
 *  - Los flancos de ECHO se miden por interrupción (timestamps en us).
 *  - Los timeouts se vigilan desde hcsr04_actualizar().
 *  Pines: ECHO_HCSR04 y TRIG_HCSR04 (hardware.h).
 * ===================================================================== */

// Mediciones (todas en mm)
#define DISTANCIA_OBJETIVO        (150)      // se detecta objeto si distancia <= este valor
#define DISTANCIA_MIN_VALIDA      (20)       // zona ciega del HC-SR04 (~2 cm)
#define DISTANCIA_MAX_VALIDA      (1000)
#define DISTANCIA_LEJOS           (0xFFFF)   // "sin lectura válida" / fuera de rango

// HC-SR04
#define HCSR04_TRIG_US            (10)       // ancho del pulso de disparo
#define HCSR04_TIMEOUT_INICIO_US  (5000)     // máx. espera a que ECHO suba
#define HCSR04_TIMEOUT_ECO_US     (30000)    // máx. duración de ECHO (~5 m); más = sin objeto
#define HCSR04_PERIODO_MS         (60)       // separación mínima entre mediciones
#define VELOCIDAD_SONIDO_M_S      (343)      // a ~20 °C

// Detección (anti-rebote por lecturas consecutivas + histéresis)
#define HCSR04_LECTURAS_CONFIRMAR (3)        // 3 x 60 ms = 180 ms
#define HCSR04_HISTERESIS_MM      (20)       // para dar por "ido": > OBJETIVO + histéresis

/* ---------------------------------------------------------------------
 *  API
 * ------------------------------------------------------------------- */

// Configura TRIG/ECHO e instala la interrupción de ECHO.
void     hcsr04_init(void);

// Llamar en cada vuelta del bucle principal. No bloquea.
void     hcsr04_actualizar(void);

// Última distancia válida medida en mm, o DISTANCIA_LEJOS.
uint32_t hcsr04_distancia_mm(void);

// Nivel: hay un objeto dentro de DISTANCIA_OBJETIVO (ya confirmado).
bool     hcsr04_hay_objeto(void);

// Evento: devuelve true UNA vez cuando aparece un objeto (se consume al leerlo).
bool     hcsr04_objeto_detectado(void);

#endif
