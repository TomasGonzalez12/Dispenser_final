#include <stdio.h>
#include "pico/stdlib.h"
#include "balanza.h"
#include "botones.h"
#include "hardware.h"
#include "hc_sr04.h"
#include "picoOled.h"
#include "servo.h"

/* =====================================================================
 *  DISPENSER PARA MASCOTAS - MÁQUINA DE ESTADOS
 *
 *  ST_TARA         pide tarar (botón TARAR) con el plato vacío
 *  ST_SELECCION    elige el peso objetivo (SUBIR / BAJAR / SELECCION)
 *  ST_ESPERA       PUERTA_CERRADA: espera a la mascota (HC-SR04)
 *  ST_DISPENSANDO  PUERTA_ABIERTA: hasta que la balanza llega al objetivo
 *  ST_BLOQUEO      PUERTA_BLOQUEO: se ignoran sensor y balanza (TIEMPO_BLOQUEO_MS)
 *  ST_ERROR        señal de la balanza fuera de rango (puerta cerrada)
 * ===================================================================== */

/* ---------- Parámetros ---------- */
#define OBJETIVO_MIN_G            (0)
#define OBJETIVO_MAX_G            (2000)
#define OBJETIVO_PASO_G           (50)
#define OBJETIVO_INICIAL_G        (200)

#define TIEMPO_MAX_DISPENSADO_MS  (30000UL)   // seguridad: cierra aunque no llegue al peso
#define PERIODO_PANTALLA_MS       (200UL)     // la OLED tarda ~25 ms por refresco
#define MENSAJE_MS                (800)

typedef enum {
    ST_TARA,
    ST_SELECCION,
    ST_ESPERA,
    ST_DISPENSANDO,
    ST_BLOQUEO,
    ST_ERROR
} estado_t;

static const char *const NOMBRE_ESTADO[] = {
    "TARA", "SELECCION", "ESPERA (PUERTA_CERRADA)",
    "DISPENSANDO (PUERTA_ABIERTA)", "BLOQUEO (PUERTA_BLOQUEO)", "ERROR"
};

static estado_t estado    = ST_TARA;
static uint32_t t_estado  = 0;                 // ms en que se entró al estado
static int      objetivo_g = OBJETIVO_INICIAL_G;
static bool     redibujar = true;

/* ---------------------------------------------------------------------
 *  Auxiliares
 * ------------------------------------------------------------------- */
static inline uint32_t ahora_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

// Descarta pulsaciones viejas para que no "se cuelen" en el estado siguiente
static void limpiar_botones(void)
{
    pulsador_presionado(&puls_subir);
    pulsador_presionado(&puls_bajar);
    pulsador_presionado(&puls_seleccion);
    pulsador_presionado(&puls_tarar);
}

static int segundos_bloqueo_restantes(uint32_t ahora)
{
    uint32_t transcurrido = ahora - t_estado;
    if (transcurrido >= TIEMPO_BLOQUEO_MS) return 0;
    return (int)((TIEMPO_BLOQUEO_MS - transcurrido + 999UL) / 1000UL);
}

/* Acciones al ENTRAR a cada estado */
static void entrar_estado(estado_t nuevo)
{
    estado    = nuevo;
    t_estado  = ahora_ms();
    redibujar = true;
    printf("[ESTADO] %s\n", NOMBRE_ESTADO[nuevo]);

    switch (nuevo) {
    case ST_TARA:
    case ST_SELECCION:
        limpiar_botones();
        break;

    case ST_ESPERA:
        angulo_servo(PUERTA_CERRADA);
        hcsr04_objeto_detectado();      // descarta detecciones acumuladas
        limpiar_botones();
        break;

    case ST_DISPENSANDO:
        balanza_iniciar_dispensado((float)objetivo_g);
        angulo_servo(PUERTA_ABIERTA);
        break;

    case ST_BLOQUEO:
    case ST_ERROR:
        angulo_servo(PUERTA_CERRADA);
        balanza_detener_dispensado();
        break;
    }
}

/* ---------------------------------------------------------------------
 *  Pantalla OLED (fuente 6x8: 21 caracteres por línea, 10 en doble tamaño)
 * ------------------------------------------------------------------- */
static void txt(uint8_t x, uint8_t y, const char *s)
{
    oledDraw_string(&oled, x, y, s, NORMAL_SIZE, WHITE);
}

static void txt_grande(uint8_t x, uint8_t y, const char *s)
{
    oledDraw_string(&oled, x, y, s, DOUBLE_SIZE, WHITE);
}

static void dibujar_tara(void)
{
    oledClear(&oled, BLACK);
    txt(0, 0,  "TARA DE LA BALANZA");
    txt(0, 16, "Deje el plato vacio");
    txt(0, 32, "y presione TARAR");
}

static void dibujar_seleccion(void)
{
    char linea[16];
    snprintf(linea, sizeof(linea), "%d g", objetivo_g);

    oledClear(&oled, BLACK);
    txt(0, 0, "PESO OBJETIVO");
    txt_grande(0, 16, linea);
    txt(0, 40, "SUBIR/BAJAR: 50 g");
    txt(0, 52, "SELECCION: aceptar");
}

static void dibujar_principal(int peso, int seg_bloqueo)
{
    char linea[24];
    const char *puerta;

    switch (estado) {
    case ST_DISPENSANDO: puerta = "PUERTA_ABIERTA";  break;
    case ST_BLOQUEO:     puerta = "PUERTA_BLOQUEO";  break;
    default:             puerta = "PUERTA_CERRADA";  break;
    }

    oledClear(&oled, BLACK);
    txt(0, 0, puerta);
    txt(0, 14, "PESO ACTUAL:");
    snprintf(linea, sizeof(linea), "%d g", peso);
    txt_grande(0, 26, linea);

    if (estado == ST_BLOQUEO) {
        snprintf(linea, sizeof(linea), "Obj:%dg  Libre: %ds", objetivo_g, seg_bloqueo);
    } else {
        snprintf(linea, sizeof(linea), "Objetivo: %d g", objetivo_g);
    }
    txt(0, 54, linea);
}

static void dibujar_error(void)
{
    oledClear(&oled, BLACK);
    txt(0, 0,  "ERROR BALANZA");
    txt(0, 16, "Senal fuera de rango");
    txt(0, 32, "Revise la celda y el");
    txt(0, 42, "cableado del AD623");
}

static void actualizar_pantalla(uint32_t ahora)
{
    static uint32_t t_ultimo   = 0;
    static int      ult_estado = -1;
    static int      ult_peso   = 0;
    static int      ult_seg    = -1;

    bool principal = (estado == ST_ESPERA || estado == ST_DISPENSANDO || estado == ST_BLOQUEO);
    int  peso = principal ? balanza_peso_pantalla_g() : 0;
    int  seg  = (estado == ST_BLOQUEO) ? segundos_bloqueo_restantes(ahora) : -1;

    bool urgente = redibujar || ((int)estado != ult_estado);
    bool cambio  = urgente || (peso != ult_peso) || (seg != ult_seg);

    if (!cambio) return;
    if (!urgente && (ahora - t_ultimo) < PERIODO_PANTALLA_MS) return;

    switch (estado) {
    case ST_TARA:      dibujar_tara();                  break;
    case ST_SELECCION: dibujar_seleccion();             break;
    case ST_ERROR:     dibujar_error();                 break;
    default:           dibujar_principal(peso, seg);    break;
    }
    oledDisplay(&oled);

    t_ultimo   = ahora;
    ult_estado = (int)estado;
    ult_peso   = peso;
    ult_seg    = seg;
    redibujar  = false;
}

/* ---------------------------------------------------------------------
 *  Tara con mensajes en pantalla (bloqueante ~1 s: solo con puerta cerrada)
 * ------------------------------------------------------------------- */
static bool tarar_con_mensaje(void)
{
    oledClear(&oled, BLACK);
    txt(0, 0, "Tarando...");
    oledDisplay(&oled);

    bool ok = balanza_tarar();
    printf("Tara %s\n", ok ? "OK" : "FALLIDA");

    oledClear(&oled, BLACK);
    txt(0, 0,  ok ? "TARA OK" : "TARA FALLIDA");
    if (!ok) {
        txt(0, 16, "Senal inestable.");
        txt(0, 26, "Reintente.");
    }
    oledDisplay(&oled);
    sleep_ms(MENSAJE_MS);

    limpiar_botones();      // por si se presionó algo mientras tanto
    return ok;
}

/* ---------------------------------------------------------------------
 *  MAIN
 * ------------------------------------------------------------------- */
int main(void)
{
    init_config();                  // stdio, OLED, balanza (solo init), servo, botones, HC-SR04
    entrar_estado(ST_TARA);

    while (true) {
        balanza_actualizar();       // no bloquea: una muestra nueva cada 20 ms
        hcsr04_actualizar();        // no bloquea: dispara y mide por interrupción
        uint32_t ahora = ahora_ms();

        switch (estado) {

        /* ---- Tara inicial ---- */
        case ST_TARA:
            if (pulsador_presionado(&puls_tarar)) {
                if (tarar_con_mensaje()) {
                    entrar_estado(ST_SELECCION);
                } else {
                    redibujar = true;       // reintentar
                }
            }
            break;

        /* ---- Selección del peso objetivo ---- */
        case ST_SELECCION:
            if (pulsador_presionado(&puls_subir)) {
                if (objetivo_g + OBJETIVO_PASO_G <= OBJETIVO_MAX_G) objetivo_g += OBJETIVO_PASO_G;
                redibujar = true;
            }
            if (pulsador_presionado(&puls_bajar)) {
                if (objetivo_g - OBJETIVO_PASO_G >= OBJETIVO_MIN_G) objetivo_g -= OBJETIVO_PASO_G;
                redibujar = true;
            }
            if (pulsador_presionado(&puls_seleccion)) {
                printf("Objetivo elegido: %d g\n", objetivo_g);
                entrar_estado(ST_ESPERA);
            }
            break;

        /* ---- Puerta cerrada, esperando a la mascota ---- */
        case ST_ESPERA:
            if (!balanza_ok()) {
                entrar_estado(ST_ERROR);
            } else if (pulsador_presionado(&puls_tarar)) {      // extra: re-tarar (plato nuevo)
                tarar_con_mensaje();
                redibujar = true;
            } else if (pulsador_presionado(&puls_seleccion)) {  // extra: cambiar el objetivo
                entrar_estado(ST_SELECCION);
            } else if (hcsr04_objeto_detectado()) {
                // Solo libera comida si la balanza marca por debajo del objetivo
                if (balanza_necesita_alimento((float)objetivo_g)) {
                    printf("Mascota detectada (%u mm): peso %.1f g < objetivo %d g -> abre\n",
                           (unsigned)hcsr04_distancia_mm(), balanza_peso_g(), objetivo_g);
                    entrar_estado(ST_DISPENSANDO);
                } else {
                    printf("Mascota detectada: peso %.1f g, ya esta el objetivo -> no abre\n",
                           balanza_peso_g());
                }
            }
            break;

        /* ---- Puerta abierta hasta llegar al peso ---- */
        case ST_DISPENSANDO:
            if (!balanza_ok()) {
                printf("Falla de balanza: cierra\n");
                entrar_estado(ST_ERROR);
            } else if (balanza_objetivo_alcanzado()) {
                printf("Objetivo alcanzado: %.1f g (objetivo %d g)\n", balanza_peso_g(), objetivo_g);
                entrar_estado(ST_BLOQUEO);
            } else if ((ahora - t_estado) >= TIEMPO_MAX_DISPENSADO_MS) {
                printf("Timeout de dispensado: %.1f g (objetivo %d g)\n", balanza_peso_g(), objetivo_g);
                entrar_estado(ST_BLOQUEO);
            }
            break;

        /* ---- Bloqueo: la mascota come; se ignoran sensor y balanza ---- */
        case ST_BLOQUEO:
            if ((ahora - t_estado) >= TIEMPO_BLOQUEO_MS) {
                printf("Fin de bloqueo: peso %.1f g\n", balanza_peso_g());
                entrar_estado(ST_ESPERA);
            }
            break;

        /* ---- Falla de la balanza ---- */
        case ST_ERROR:
            if (balanza_ok()) {
                entrar_estado(ST_ESPERA);
            }
            break;
        }

        actualizar_pantalla(ahora);
    }
}
