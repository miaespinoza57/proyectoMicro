#ifndef GPIO_H_
#define GPIO_H_

#include <avr/io.h>
#include <stdint.h>

typedef enum {
    GPIO_INPUT  = 0,
    GPIO_OUTPUT = 1
} GPIO_Direction;
 
/* Estado logico de pin: bajo 0V alto 5V */
typedef enum {
    GPIO_LOW  = 0,
    GPIO_HIGH = 1
} GPIO_State;
 
 
/*
 * GPIO_SetPinDirection
 * Configura un pin especifico de un puerto como entrada o salida.
 * Parametros:
 *   port -> direccion del registro PORTx
 *   pin  -> numero de pin dentro del puerto (0 a 7)
 *   dir  -> GPIO_INPUT o GPIO_OUTPUT
 */
void GPIO_SetPinDirection(volatile uint8_t *port, uint8_t pin, GPIO_Direction dir);
 
/*
 * GPIO_SetPortDirection
 * Configura los 8 pines de un puerto completo de una sola vez.
 * Parametros:
 *   port    -> direccion del registro PORTx 
 *   dirMask -> pone byte en 1 = salida 0 = entrada
 */
void GPIO_SetPortDirection(volatile uint8_t *port, uint8_t dirMask);
 
/*
 * GPIO_ReadPin
 * Lee el estado logico actual de un pin (0 = bajo, 1 = alto).
 * Parametros:
 *   port -> direccion del registro PORTx (ej. &PORTC)
 *   pin  -> numero de pin a leer (0 a 7)
 * Regresa:
 *   0 o 1 segun el estado del pin
 */
uint8_t GPIO_ReadPin(volatile uint8_t *port, uint8_t pin);
 
/*
 * GPIO_WritePin
 * Escribe un estado logico (alto o bajo) en un pin de salida.
 * Tambien sirve para activar la resistencia pull-up interna
 * cuando el pin esta configurado como entrada.
 * Parametros:
 *   port  -> direccion del registro PORTx (ej. &PORTD)
 *   pin   -> numero de pin a escribir (0 a 7)
 *   state -> GPIO_LOW o GPIO_HIGH
 */
void GPIO_WritePin(volatile uint8_t *port, uint8_t pin, GPIO_State state);
 
/*
 * GPIO_ReadPort
 * Lee el estado logico de los 8 pines de un puerto al mismo tiempo.
 * Parametros:
 *   port -> direccion del registro PORTx (ej. &PORTB)
 * Regresa:
 *   un byte donde cada bit representa el estado de un pin
 */
uint8_t GPIO_ReadPort(volatile uint8_t *port);
 
/*
 * GPIO_WritePort
 * Escribe un valor completo de 8 bits en un puerto de salida.
 * Util para mostrar directamente un numero binario en 8 LEDs.
 * Parametros:
 *   port  -> direccion del registro PORTx (ej. &PORTD)
 *   value -> byte a escribir en el puerto (0x00 a 0xFF)
 */
void GPIO_WritePort(volatile uint8_t *port, uint8_t value);
 
#endif /* GPIO_H_ */
 