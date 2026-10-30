#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware.h"  
#include "hc_sr04.h"

/* ---------------------------------------------------------------------
 *  Estado interno de la medición
 *
 *  REPOSO -> (periodo cumplido, pulso en TRIG) -> ESPERANDO_ECO
 *         -> (flanco de subida, ISR)           -> MIDIENDO
 *         -> (flanco de bajada, ISR)           -> LISTA
 *         -> (el main procesa la lectura)      -> REPOSO
 *  Si ECHO no sube o no baja a tiempo, hcsr04_actualizar() pasa a LISTA
 *  con DISTANCIA_LEJOS.
 * ------------------------------------------------------------------- */
typedef enum {
    HC_REPOSO,
    HC_ESPERANDO_ECO,
    HC_MIDIENDO,
    HC_LISTA
} hc_estado_t;

// Compartidas entre ISR y main
static volatile hc_estado_t s_estado = HC_REPOSO;
static volatile uint32_t    s_t_trigger;
static volatile uint32_t    s_t_subida;
static volatile uint32_t    s_mm_cruda = DISTANCIA_LEJOS;

// Solo del main
static uint32_t s_t_ultimo_inicio;
static uint32_t s_ultima_mm = DISTANCIA_LEJOS;
static uint8_t  s_cnt_cerca;
static uint8_t  s_cnt_libre;
static bool     s_hay_objeto;
static bool     s_evento;

/* ---------------------------------------------------------------------
 *  Conversión: mm = dt_us * v(mm/us) / 2  con v = 0,343 mm/us
 * ------------------------------------------------------------------- */
static inline uint32_t us_a_mm(uint32_t dt_us)
{
    uint32_t mm = (dt_us * VELOCIDAD_SONIDO_M_S) / 2000UL;

    if (mm < DISTANCIA_MIN_VALIDA || mm > DISTANCIA_MAX_VALIDA) {
        return DISTANCIA_LEJOS;
    }
    return mm;
}

/* ---------------------------------------------------------------------
 *  ISR de ECHO (handler "raw": no pisa el callback de los botones)
 * ------------------------------------------------------------------- */
static void echo_irq_handler(void)
{
    uint32_t eventos = gpio_get_irq_event_mask(ECHO_HCSR04) &
                       (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    if (!eventos) {
        return;                         // la interrupción era de otro pin
    }
    gpio_acknowledge_irq(ECHO_HCSR04, eventos);

    uint32_t ahora = time_us_32();

    if ((eventos & GPIO_IRQ_EDGE_RISE) && s_estado == HC_ESPERANDO_ECO) {
        s_t_subida = ahora;
        s_estado   = HC_MIDIENDO;
    }
    if ((eventos & GPIO_IRQ_EDGE_FALL) && s_estado == HC_MIDIENDO) {
        s_mm_cruda = us_a_mm(ahora - s_t_subida);
        s_estado   = HC_LISTA;
    }
}

/* ---------------------------------------------------------------------
 *  Auxiliares (contexto main)
 * ------------------------------------------------------------------- */
static void iniciar_medicion(void)
{
    s_t_ultimo_inicio = time_us_32();

    // Único tramo "bloqueante": 10 us del pulso de disparo
    gpio_put(TRIG_HCSR04, 1);
    busy_wait_us_32(HCSR04_TRIG_US);
    gpio_put(TRIG_HCSR04, 0);

    s_t_trigger = time_us_32();
    s_estado    = HC_ESPERANDO_ECO;
}

static void verificar_timeouts(void)
{
    // La hora se lee dentro de la sección crítica: si no, la ISR podría
    // escribir s_t_subida después de leer "ahora" y la resta daría un valor enorme.
    uint32_t irq   = save_and_disable_interrupts();
    uint32_t ahora = time_us_32();

    if (s_estado == HC_ESPERANDO_ECO &&
        (ahora - s_t_trigger) > HCSR04_TIMEOUT_INICIO_US) {
        s_mm_cruda = DISTANCIA_LEJOS;       // el sensor no respondió
        s_estado   = HC_LISTA;
    } else if (s_estado == HC_MIDIENDO &&
               (ahora - s_t_subida) > HCSR04_TIMEOUT_ECO_US) {
        s_mm_cruda = DISTANCIA_LEJOS;       // eco demasiado largo: sin objeto
        s_estado   = HC_LISTA;
    }
    restore_interrupts(irq);
}

static void procesar_lectura(uint32_t mm)
{
    s_ultima_mm = mm;

    bool cerca = (mm != DISTANCIA_LEJOS) && (mm <= DISTANCIA_OBJETIVO);
    bool libre = (mm == DISTANCIA_LEJOS) || (mm > DISTANCIA_OBJETIVO + HCSR04_HISTERESIS_MM);

    if (cerca) {
        s_cnt_libre = 0;
        if (s_cnt_cerca < HCSR04_LECTURAS_CONFIRMAR) s_cnt_cerca++;
        if (s_cnt_cerca >= HCSR04_LECTURAS_CONFIRMAR && !s_hay_objeto) {
            s_hay_objeto = true;
            s_evento     = true;
        }
    } else if (libre) {
        s_cnt_cerca = 0;
        if (s_cnt_libre < HCSR04_LECTURAS_CONFIRMAR) s_cnt_libre++;
        if (s_cnt_libre >= HCSR04_LECTURAS_CONFIRMAR) {
            s_hay_objeto = false;
        }
    }
    // Entre OBJETIVO y OBJETIVO + histéresis: zona de histéresis, no cambia nada
}

/* ---------------------------------------------------------------------
 *  API
 * ------------------------------------------------------------------- */
void hcsr04_init(void)
{
    gpio_init(TRIG_HCSR04);
    gpio_set_dir(TRIG_HCSR04, GPIO_OUT);
    gpio_put(TRIG_HCSR04, 0);

    gpio_init(ECHO_HCSR04);
    gpio_set_dir(ECHO_HCSR04, GPIO_IN);
    gpio_pull_down(ECHO_HCSR04);

    s_t_ultimo_inicio = time_us_32();
    s_estado = HC_REPOSO;

    gpio_acknowledge_irq(ECHO_HCSR04, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    gpio_add_raw_irq_handler(ECHO_HCSR04, echo_irq_handler);
    gpio_set_irq_enabled(ECHO_HCSR04, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void hcsr04_actualizar(void)
{
    switch (s_estado) {
    case HC_REPOSO:
        if ((time_us_32() - s_t_ultimo_inicio) >= (HCSR04_PERIODO_MS * 1000u)) {
            iniciar_medicion();
        }
        break;

    case HC_ESPERANDO_ECO:
    case HC_MIDIENDO:
        verificar_timeouts();
        break;

    case HC_LISTA:
        procesar_lectura(s_mm_cruda);
        s_estado = HC_REPOSO;
        break;
    }
}

uint32_t hcsr04_distancia_mm(void)
{
    return s_ultima_mm;
}


bool hcsr04_objeto_detectado(void)
{
    if (s_evento) {
        s_evento = false;
        return true;
    }
    return false;
}
