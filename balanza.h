#ifndef BALANZA_H
#define BALANZA_H

//Variables
extern const float VREF_PICO;
extern const float CONVERSION_FACTOR;

extern const float PESO_OBJETO_PRUEBA_GRAMOS;
extern const float VARIACION_VOLTAJE_MEDIDA;
extern const float FACTOR_GRAMOS_POR_VOLTIO;

// --- VARIABLES FILTRO DIGITAL ---
extern float gramos_filtrados;
extern const float alfa;
extern int peso_fijo_pantalla;
extern float voltaje_tara;

//Prototipo funciones
float leer_voltaje_promedio(int);

#endif