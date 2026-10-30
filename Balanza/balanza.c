#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware.h"      // PIN_ADC, CANAL_ADC
#include "balanza.h"

/* ---------------------------------------------------------------------
 *  Muestreo (lo escribe la interrupción del timer, lo lee el main)
 * ------------------------------------------------------------------- */
static repeating_timer_t  s_timer;
static uint32_t           s_acum;            // solo la ISR
static uint16_t           s_cnt;             // solo la ISR
static volatile uint32_t  s_ventana_suma;    // suma de las últimas 80 muestras
static volatile uint32_t  s_ventana_id;      // se incrementa en cada ventana nueva

/* ---------------------------------------------------------------------
 *  Estado de la balanza
 * ------------------------------------------------------------------- */
static uint32_t s_ultimo_id;
static float    s_factor_g_por_v = BALANZA_FACTOR_G_POR_V;
static float    s_tara_cuentas;
static float    s_cuentas;
static float    s_gramos;
static int      s_peso_pantalla;
static uint16_t s_estable_cnt;
static bool     s_ok;

// Dispensado
static bool     s_dispensando;
static float    s_umbral_cierre;
static uint8_t  s_conf;
static bool     s_objetivo_ok;

/* ---------------------------------------------------------------------
 *  Interrupción del timer: 1 lectura cada 250 us (debe ser corta)
 * ------------------------------------------------------------------- */
static bool muestreo_cb(repeating_timer_t *rt)
{
    (void)rt;

    s_acum += adc_read();
    if (++s_cnt >= BALANZA_MUESTRAS_VENTANA) {
        s_ventana_suma = s_acum;
        s_ventana_id++;
        s_acum = 0;
        s_cnt  = 0;
    }
    return true;    // seguir repitiendo
}

/* ---------------------------------------------------------------------
 *  Auxiliares
 * ------------------------------------------------------------------- */
static inline float cuentas_a_voltios(float cuentas)
{
    return cuentas * (BALANZA_VREF_V / BALANZA_ADC_MAX);
}

// Espera la próxima ventana completa (<= 20 ms) y devuelve su promedio en cuentas.
static float esperar_ventana(void)
{
    uint32_t id = s_ventana_id;
    while (s_ventana_id == id) {
        tight_loop_contents();
    }
    uint32_t suma = s_ventana_suma;
    s_ultimo_id = s_ventana_id;     // esta ventana ya fue consumida
    return (float)suma / BALANZA_MUESTRAS_VENTANA;
}

// Promedia n ventanas. Devuelve true si (máx - mín) <= max_disp.
static bool promediar(int n, float max_disp, float *prom)
{
    float suma = 0.0f, mn = 1.0e9f, mx = -1.0e9f;

    for (int i = 0; i < n; i++) {
        float c = esperar_ventana();
        suma += c;
        if (c < mn) mn = c;
        if (c > mx) mx = c;
    }
    *prom = suma / (float)n;
    return (mx - mn) <= max_disp;
}

/* ---------------------------------------------------------------------
 *  API
 * ------------------------------------------------------------------- */
void balanza_init(void)
{
    adc_init();
    adc_gpio_init(PIN_ADC);
    adc_select_input(CANAL_ADC);

    s_acum = 0;
    s_cnt  = 0;

    // Delay negativo: período fijo medido entre inicios de callback (ventana exacta de 20 ms)
    add_repeating_timer_us(-BALANZA_PERIODO_MUESTREO_US, muestreo_cb, NULL, &s_timer);
}

bool balanza_tarar(void)
{
    absolute_time_t limite = make_timeout_time_ms(BALANZA_TARA_TIMEOUT_MS);
    float prom = 0.0f;
    bool  estable;

    do {
        estable = promediar(BALANZA_TARA_VENTANAS, BALANZA_TARA_DISPERSION_CTAS, &prom);
    } while (!estable && !time_reached(limite));

    s_tara_cuentas  = prom;
    s_cuentas       = prom;
    s_gramos        = 0.0f;
    s_peso_pantalla = 0;
    s_estable_cnt   = 0;
    s_dispensando   = false;
    s_objetivo_ok   = false;
    s_conf          = 0;
    s_ok = (prom > BALANZA_CUENTAS_MIN) && (prom < BALANZA_CUENTAS_MAX);

    return estable & s_ok;
}

bool balanza_calibrar(float peso_patron_g)
{
    float prom = 0.0f;
    bool  estable = promediar(BALANZA_TARA_VENTANAS, BALANZA_TARA_DISPERSION_CTAS, &prom);
    float dv = cuentas_a_voltios(prom - s_tara_cuentas);

    if (!estable || peso_patron_g <= 0.0f || dv < 0.01f) {
        return false;
    }
    s_factor_g_por_v = peso_patron_g / dv;
    return true;
}

float balanza_get_factor(void)             { return s_factor_g_por_v; }
void  balanza_set_factor(float g_por_v)    { s_factor_g_por_v = g_por_v; }

bool balanza_actualizar(void)
{
    uint32_t id = s_ventana_id;
    if (id == s_ultimo_id) {
        return false;               // todavía no hay ventana nueva
    }
    s_ultimo_id = id;

    // 1) Cuentas promedio de la ventana y validación de rango
    s_cuentas = (float)s_ventana_suma / BALANZA_MUESTRAS_VENTANA;
    s_ok = (s_cuentas > BALANZA_CUENTAS_MIN) && (s_cuentas < BALANZA_CUENTAS_MAX);

    // 2) Cuentas -> voltios -> gramos (respecto de la tara)
    float gramos = cuentas_a_voltios(s_cuentas - s_tara_cuentas) * s_factor_g_por_v;

    // 3) EMA adaptativo: lento si está quieto, rápido si hay un cambio grande
    float error = gramos - s_gramos;
    float alfa  = (fabsf(error) > BALANZA_UMBRAL_RAPIDO_G) ? BALANZA_ALFA_RAPIDO
                                                          : BALANZA_ALFA_LENTO;
    s_gramos += alfa * error;

    // 4) Estabilidad
    if (fabsf(error) < BALANZA_UMBRAL_ESTABLE_G) {
        if (s_estable_cnt < UINT16_MAX) s_estable_cnt++;
    } else {
        s_estable_cnt = 0;
    }

    // 5) Valor para pantalla: zona muerta y histéresis (no afectan al filtro)
    int redondeado = (int)roundf(s_gramos);
    if (fabsf(s_gramos) < BALANZA_ZONA_MUERTA_G) {
        s_peso_pantalla = 0;
    } else if (abs(redondeado - s_peso_pantalla) >= BALANZA_HISTERESIS_PANT_G) {
        s_peso_pantalla = redondeado;
    }

    // 6) Objetivo alcanzado: varias ventanas seguidas sobre el umbral
    //    (evita cerrar por un pico de los granos al caer sobre la celda)
    if (s_dispensando) {
        if (s_gramos >= s_umbral_cierre) {
            if (s_conf < BALANZA_CONFIRMACIONES) s_conf++;
            if (s_conf >= BALANZA_CONFIRMACIONES) s_objetivo_ok = true;
        } else {
            s_conf = 0;
        }
    }
    return true;
}

float balanza_peso_g(void)           { return s_gramos; }
int   balanza_peso_pantalla_g(void)  { return s_peso_pantalla; }
bool  balanza_estable(void)          { return s_estable_cnt >= BALANZA_VENTANAS_ESTABLE; }
bool  balanza_ok(void)               { return s_ok; }
float balanza_voltaje(void)          { return cuentas_a_voltios(s_cuentas); }

void balanza_iniciar_dispensado(float objetivo_g)
{
    s_umbral_cierre = objetivo_g - BALANZA_ANTICIPO_G;
    s_conf          = 0;
    s_objetivo_ok   = false;
    s_dispensando   = true;
}

bool balanza_objetivo_alcanzado(void)
{
    return s_objetivo_ok;
}

void balanza_detener_dispensado(void)
{
    s_dispensando = false;
    s_objetivo_ok = false;
    s_conf        = 0;
}

bool balanza_necesita_alimento(float objetivo_g)
{
    return s_gramos < (objetivo_g - BALANZA_MARGEN_REAPERTURA_G);
}
