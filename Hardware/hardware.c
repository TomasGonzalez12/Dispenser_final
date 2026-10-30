#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "balanza.h"
#include "botones.h"
#include "hardware.h"
#include "hc_sr04.h"
#include "picoOled.h"
#include "servo.h"


t_OledParams oled;

void init_config(){
    stdio_init_all();

    // Config PWM
    gpio_set_function(SIG_SERVO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(SIG_SERVO_PIN);
    channel = pwm_gpio_to_channel(SIG_SERVO_PIN);
    pwm_set_clkdiv(slice, SERVO_DIV);
    pwm_set_wrap(slice, SERVO_TOP);
    angulo_servo(PUERTA_CERRADA);
    pwm_set_enabled(slice, true);

    //Oled
    oled.i2c = I2C_PORT;
    oled.SDA_PIN = PIN_SDA;
    oled.SCL_PIN = PIN_SCL;

    oled.ctlrType = CTRL_SSD1306; //CTR_SH1106 si usamos la otra
    oled.i2c_address = OLED_ADDR;
    oled.height = H_64;
    oled.width = W_128;      

    oledI2cConfig(&oled);

    oledSetTTYMode(&oled, false);
    oledSet_invert(&oled, false);

    // 2. Inicialización de balanza ADC
    balanza_init();
    

    //Botones
    pulsador_init(&puls_subir, PULS_SUBIR_PIN, false);
    pulsador_init(&puls_bajar, PULS_BAJAR_PIN, false);
    pulsador_init(&puls_seleccion, PULS_SELECCION_PIN, false);
    pulsador_init(&puls_tarar, PULS_TARAR_PIN, false);

    gpio_set_irq_enabled_with_callback(puls_subir.gpio_pin, GPIO_IRQ_EDGE_FALL, true, &puls_callback);
    gpio_set_irq_enabled(puls_bajar.gpio_pin, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(puls_seleccion.gpio_pin, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(puls_tarar.gpio_pin, GPIO_IRQ_EDGE_FALL, true);

    //HC-SR04
    hcsr04_init();

}



