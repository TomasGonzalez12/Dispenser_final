#ifndef SERVO_H
#define SERVO_H

#define PUERTA_CERRADA      (10)       //grados de angulos
#define PUERTA_ABIERTA      (170)
#define TIEMPO_BLOQUEO_MS   (10UL * 1000UL)   // 30seg

#define SERVO_MIN_US        (600)
#define SERVO_MAX_US        (2400)


//PWM
#define SERVO_FREQ          (50)
#define SERVO_PERIODO_US    (1000000 / SERVO_FREQ)
#define SERVO_TOP           (59999)
#define SERVO_DIV           (50.0f)

//Variables
extern uint slice;  //del PWM
extern uint channel; //PWM

//Prototipo funciones
void angulo_servo (uint8_t);

#endif