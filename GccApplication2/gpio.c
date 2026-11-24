#include "gpio.h"
 
/*
 * GPIO_SetPinDirection
 * Configura un solo pin como entrada o salida.
 * Como el registro DDRx queda 1 posicion antes de PORTx en
 * memoria, calculamos su direccion restando 1 al puntero.
 * Si dir es OUTPUT, ponemos el bit correspondiente en 1.
 * Si dir es INPUT, ponemos el bit correspondiente en 0.
 */
void GPIO_SetPinDirection(volatile uint8_t *port, uint8_t pin, GPIO_Direction dir)
{
    volatile uint8_t *ddr = port - 1;   /* DDRx esta 1 direccion antes de PORTx */
 
    if (dir == GPIO_OUTPUT)
        *ddr |= (1 << pin);   /* pone en 1 el bit -> salida */
    else
        *ddr &= ~(1 << pin);  /* pone en 0 el bit -> entrada */
}
 
/*
 * GPIO_SetPortDirection
 * Configura los 8 pines de un puerto en una sola instruccion,
 * escribiendo directamente todo el byte del registro DDRx.
 * Cada bit en 1 = salida, cada bit en 0 = entrada.
 */
void GPIO_SetPortDirection(volatile uint8_t *port, uint8_t dirMask)
{
    volatile uint8_t *ddr = port - 1;
    *ddr = dirMask;
}
 
/*
 * GPIO_ReadPin
 * Lee el estado de un solo pin usando el registro PINx, que
 * esta 2 posiciones antes de PORTx en memoria.
 * Se recorre el bit deseado hasta la posicion 0 y se aisla
 * con una mascara (0x01) para regresar solo 0 o 1.
 */
uint8_t GPIO_ReadPin(volatile uint8_t *port, uint8_t pin)
{
    volatile uint8_t *pinReg = port - 2;  /* PINx esta 2 direcciones antes de PORTx */
    return (*pinReg >> pin) & 0x01;
}
 
/*
 * GPIO_WritePin
 * Escribe un estado logico (alto o bajo) en un pin especifico
 * del registro PORTx. Si el pin esta configurado como entrada,
 * esta misma funcion tambien sirve para activar su resistencia
 * de pull-up interna (poniendolo en GPIO_HIGH).
 */
void GPIO_WritePin(volatile uint8_t *port, uint8_t pin, GPIO_State state)
{
    if (state == GPIO_HIGH)
        *port |= (1 << pin);
    else
        *port &= ~(1 << pin);
}
 
/*
 * GPIO_ReadPort
 * Lee de una sola vez el estado de los 8 pines de un puerto,
 * devolviendo directamente el valor del registro PINx.
 */
uint8_t GPIO_ReadPort(volatile uint8_t *port)
{
    volatile uint8_t *pinReg = port - 2;
    return *pinReg;
}
 
/*
 * GPIO_WritePort
 * Escribe un byte completo en el registro PORTx, cambiando el
 * estado de los 8 pines de salida al mismo tiempo. Esta es la
 * funcion que se usa para mostrar el valor binario del contador
 * en los 8 LEDs conectados al puerto.
 */
void GPIO_WritePort(volatile uint8_t *port, uint8_t value)
{
    *port = value;
}