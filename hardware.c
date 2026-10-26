#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware.h"
#include "oled.h"
#include "botones.h"
#include "servo.h"
#include "balanza.h"


void init_config(){
    stdio_init_all();

    // 1. Inicialización de I2C y Pantalla OLED
    i2c_init(I2C_PORT, 400000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    
    sleep_ms(100); // Esperar que la pantalla encienda
    oled_init();
    oled_clear();

    // 2. Inicialización de ADC
    adc_init();
    adc_gpio_init(PIN_ADC);
    adc_select_input(CANAL_ADC);

    // 3. Proceso de Tara Inicial
    printf("BALANZA AD623 - Tarando...\n");
    sleep_ms(2500);
    voltaje_tara = leer_voltaje_promedio(150);
    printf("Tara completada. Voltaje base: %.4f V\n", voltaje_tara);

    // Config PWM
    gpio_set_function(SIG_SERVO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(SIG_SERVO_PIN);
    channel = pwm_gpio_to_channel(SIG_SERVO_PIN);
    pwm_set_clkdiv(slice, SERVO_DIV);
    pwm_set_wrap(slice, SERVO_TOP);
    angulo_servo(PUERTA_CERRADA);
    pwm_set_enabled(slice, true);
    
    //Botones
    pulsador_init(&puls_subir, PULS_SUBIR_PIN, false);
    pulsador_init(&puls_bajar, PULS_BAJAR_PIN, false);
    pulsador_init(&puls_seleccion, PULS_SELECCION_PIN, false);

    gpio_set_irq_enabled_with_callback(puls_subir.gpio_pin, GPIO_IRQ_EDGE_FALL, true, &PULS_SELECCIONallback);
    gpio_set_irq_enabled(puls_bajar.gpio_pin, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(puls_seleccion.gpio_pin, GPIO_IRQ_EDGE_FALL, true);

}


