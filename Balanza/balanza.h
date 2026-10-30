#ifndef BALANZA_H
#define BALANZA_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 *  BALANZA: celda de carga 2 kg + AD623 (G ~ 304, Ref = 1,25 V) -> ADC0
 *
 *  Todo el procesamiento de la señal vive en balanza.c. El resto del
 *  proyecto solo usa las funciones de la sección "API".
 * ===================================================================== */

/* ---------- Conversión ADC / calibración ---------- */
#define BALANZA_VREF_V               (3.3f)
#define BALANZA_ADC_MAX              (4095.0f)

// Gramos por voltio a la salida del AD623.
// Es un valor inicial: recalibrar con balanza_calibrar() y pegar acá el resultado.
#define BALANZA_FACTOR_G_POR_V       (5000.0f)

/* ---------- Muestreo ----------
 * Un timer toma una muestra cada 250 us y publica el promedio de 80 muestras
 * = ventana de 20 ms = exactamente 1 ciclo de red de 50 Hz, por lo que el
 * promedio anula el zumbido de 50 Hz y sus armónicos. */
#define BALANZA_PERIODO_MUESTREO_US  (250)
#define BALANZA_MUESTRAS_VENTANA     (80)

/* ---------- Filtro (EMA adaptativo, una actualización cada 20 ms) ---------- */
#define BALANZA_ALFA_LENTO           (0.10f)   // régimen estable: filtra más
#define BALANZA_ALFA_RAPIDO          (0.50f)   // cambio grande: sigue rápido
#define BALANZA_UMBRAL_RAPIDO_G      (8.0f)    // error a partir del cual usa alfa rápido

/* ---------- Valor para mostrar en pantalla ---------- */
#define BALANZA_ZONA_MUERTA_G        (2.0f)    // |peso| menor a esto se muestra 0
#define BALANZA_HISTERESIS_PANT_G    (2)       // cambio mínimo para refrescar el valor

/* ---------- Estabilidad ---------- */
#define BALANZA_UMBRAL_ESTABLE_G     (3.0f)
#define BALANZA_VENTANAS_ESTABLE     (25)      // 25 x 20 ms = 0,5 s

/* ---------- Tara y calibración ---------- */
#define BALANZA_TARA_VENTANAS        (50)      // 50 x 20 ms = 1 s de promedio
#define BALANZA_TARA_DISPERSION_CTAS (4.0f)    // máx-mín admitido entre ventanas (cuentas)
#define BALANZA_TARA_TIMEOUT_MS      (5000)

/* ---------- Dispensado ---------- */
#define BALANZA_ANTICIPO_G           (5.0f)    // gramos "en el aire" al cerrar: AJUSTAR
#define BALANZA_CONFIRMACIONES       (3)       // ventanas seguidas sobre el umbral (60 ms)
#define BALANZA_MARGEN_REAPERTURA_G  (10.0f)   // bajo el objetivo - margen => hay que reponer

/* ---------- Validación de la señal ---------- */
#define BALANZA_CUENTAS_MIN          (150.0f)  // por debajo: cable cortado / sensor en falla
#define BALANZA_CUENTAS_MAX          (4000.0f) // por encima: sobrecarga / saturación

/* ---------- Límites para elegir el peso objetivo (botones) ---------- */
#define BALANZA_OBJETIVO_MIN_G       (10)
#define BALANZA_OBJETIVO_MAX_G       (1000)    // la celda de 2 kg también soporta el plato
#define BALANZA_OBJETIVO_PASO_G      (10)

/* =====================================================================
 *  API
 * ===================================================================== */

// Inicializa el ADC (GPIO26) y arranca el muestreo por timer.
void  balanza_init(void);

// Tara con el plato vacío. Bloqueante (~1 s, máx. BALANZA_TARA_TIMEOUT_MS).
// Devuelve false si la señal no se estabilizó (igual queda tarada con lo último medido).
bool  balanza_tarar(void);

// Llamar en cada vuelta del bucle principal. No bloquea.
// Devuelve true cuando procesó una muestra nueva (cada 20 ms).
bool  balanza_actualizar(void);

float balanza_peso_g(void);                // peso filtrado en gramos
int   balanza_peso_pantalla_g(void);       // entero estable, para la OLED
bool  balanza_estable(void);               // lectura quieta durante >= 0,5 s
bool  balanza_ok(void);                    // false = señal fuera de rango (falla)
float balanza_voltaje(void);               // voltaje en el ADC (depuración)

// Control del dispensado
void  balanza_iniciar_dispensado(float objetivo_g);  // al abrir la compuerta
bool  balanza_objetivo_alcanzado(void);              // true => cerrar la compuerta
void  balanza_detener_dispensado(void);              // al cerrar
bool  balanza_necesita_alimento(float objetivo_g);   // decide si corresponde abrir

#endif
