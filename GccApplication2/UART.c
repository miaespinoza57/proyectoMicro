#include "UART.h"
#include <avr/io.h>
#define F_CPU 16000000UL
/*
 * UART_init
 * baudRate: velocidad en bps, ej. 9600, 19200, 115200
 * dataSize: bits de datos por caracter (5, 6, 7, 8 o 9)
 * parity:   'N' = sin paridad, 'E' = par (even), 'O' = impar (odd)
 * stopBits: 1 o 2 bits de parada
 */
void UART_init(unsigned long baudRate, unsigned char dataSize, char parity, unsigned char stopBits)
{
	UCSR0A |= (0<<U2X0);   // Double USART Speed Disabled
	UCSR0A |= (0<<MPCM0);  // Multiprocessor Mode Disabled

	UCSR0B |= (1<<TXEN0);  // Tx Enable
	UCSR0B |= (1<<RXEN0);  // Rx Enable
	UCSR0B |= (0<<UDRIE0); // Data Register Empty IntEn
	UCSR0B |= (0<<TXCIE0); // Tx Complete IntEn
	UCSR0B |= (1<<RXCIE0); // Rx Complete IntEn

	// --- Data size (UCSZ02:00) ---
	switch(dataSize){
		case 5:
			UCSR0B &= ~(1<<UCSZ02);
			UCSR0C = (UCSR0C & ~((1<<UCSZ01)|(1<<UCSZ00)));
			break;
		case 6:
			UCSR0B &= ~(1<<UCSZ02);
			UCSR0C = (UCSR0C & ~(1<<UCSZ01)) | (1<<UCSZ00);
			break;
		case 7:
			UCSR0B &= ~(1<<UCSZ02);
			UCSR0C = (UCSR0C & ~(1<<UCSZ00)) | (1<<UCSZ01);
			break;
		case 9:
			UCSR0B |= (1<<UCSZ02);
			UCSR0C |= (1<<UCSZ01)|(1<<UCSZ00);
			break;
		case 8:
		default:
			UCSR0B &= ~(1<<UCSZ02);
			UCSR0C |= (1<<UCSZ01)|(1<<UCSZ00);
			break;
	}

	// --- Parity mode (UPM01:00) ---
	switch(parity){
		case 'E': case 'e':
			UCSR0C = (UCSR0C & ~(1<<UPM00)) | (1<<UPM01);   // 10 = Par
			break;
		case 'O': case 'o':
			UCSR0C |= (1<<UPM01)|(1<<UPM00);                // 11 = Impar
			break;
		case 'N': case 'n':
		default:
			UCSR0C &= ~((1<<UPM01)|(1<<UPM00));             // 00 = Sin paridad
			break;
	}

	// --- Stop bits (USBS0) ---
	if(stopBits == 2){
		UCSR0C |= (1<<USBS0);
	}else{
		UCSR0C &= ~(1<<USBS0);
	}

	UCSR0C |= (0<<UCPOL0);  // Clock Polarity: 0 para Async.

	// --- Baud rate ---
	UBRR0 = (F_CPU/(16UL*baudRate)) - 1;
}

unsigned char UART_read()
{
	if(UCSR0A & (1<<RXC0))  // Si la bandera Rx Complete en UCSR0A esta activa
	{
		return UDR0;         // Regresa el dato recibido en UDR0
	}
	return 0;                // Si no, regresa 0
}

void UART_write(unsigned char data)
{
	while(!(UCSR0A & (1<<UDRE0)));  // Mientras el Data Register no este vacio, espera
	UDR0 = data;                     // Pone el dato en el buffer, se envia
}

void UART_string(char* string)
{
	while(*string != 0)  // Mientras el valor no sea el caracter nulo
	{
		UART_write(*string);  // Envia el siguiente caracter del string
		string++;              // Avanza un caracter en la direccion del string
	}
}
