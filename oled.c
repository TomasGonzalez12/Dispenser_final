#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdlib.h>
#include "hardware.h"
#include "oled.h"

void oled_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 2, false);
}

void oled_init() {
    // Inicialización simplificada del SSD1306
    oled_cmd(0xAE); // Apagar pantalla
    oled_cmd(0x20); // Modo de direccionamiento de memoria
    oled_cmd(0x00); // Horizontal
    oled_cmd(0xB0); // Set Page Start Address
    oled_cmd(0xC8); // Set COM Output Scan Direction
    oled_cmd(0x00); // Set Low Column Address
    oled_cmd(0x10); // Set High Column Address
    oled_cmd(0x40); // Set Start Line Address
    oled_cmd(0x81); // Set Contrast Control
    oled_cmd(0x7F); 
    oled_cmd(0xA1); // Set Segment Re-map
    oled_cmd(0xA6); // Normal display
    oled_cmd(0xA8); // Multiplex ratio
    oled_cmd(0x3F); // 1/64
    oled_cmd(0xA4); // Output RAM to Display
    oled_cmd(0xD3); // Set Display Offset
    oled_cmd(0x00);
    oled_cmd(0xD5); // Set Display Clock Divide Ratio
    oled_cmd(0x80);
    oled_cmd(0xD9); // Set Pre-charge Period
    oled_cmd(0x22);
    oled_cmd(0xDA); // Set COM Pins Hardware Configuration
    oled_cmd(0x12);
    oled_cmd(0xDB); // Set VCOMH Deselect Level
    oled_cmd(0x20);
    oled_cmd(0x8D); // Charge Pump Settings
    oled_cmd(0x14); // Enable
    oled_cmd(0xAF); // Encender pantalla
}

void oled_clear() {
    uint8_t buf[129];
    buf[0] = 0x40; // Datos de control de RAM
    for(int i = 1; i <= 128; i++) buf[i] = 0;
    
    for (int page = 0; page < 8; page++) {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 129, false);
    }
}