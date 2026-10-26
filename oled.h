#ifndef OLED_H
#define OLED_H

// --- CONFIGURACIÓN OLED ---
#define I2C_PORT            (i2c0)
#define OLED_ADDR           (0x3C)
#define OLED_WIDTH          (128)
#define OLED_HEIGHT         (64)

//Prototipo funciones
void oled_cmd(uint8_t);
void oled_init();
void oled_clear();

#endif