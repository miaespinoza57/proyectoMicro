/*
 * Proyecto final - Galeria de tiro laser
 * @wilbe y Mia
 * ATmega328P a 16 MHz
 *
 * Perifericos:
 *   Timer0 : CTC cada 0.5 ms (cuenta los milisegundos) y tono del buzzer en OC0B (PD5)
 *   Timer1 : Fast PWM a 50 Hz para el servo 1 (OC1A, PB1) y el servo 2 (OC1B, PB2)
 *   Timer2 : Fast PWM para el servo 3 (OC2A, PB3)
 *   ADC    : LDR de las 3 dianas en PC0, PC1 y PC2
 *   USART  : 9600 8N1 para la interfaz de la computadora
 *   EEPROM : top 10 de puntajes con las iniciales
 *
 * Interrupciones:
 *   TIMER0_COMPA_vect : cuenta el tiempo
 *   USART_RX_vect     : comandos que manda la computadora
 *   INT0_vect         : boton INICIAR   (PD2)
 *   INT1_vect         : boton REINICIAR (PD3)
 *
 * Mensajes que manda el micro a la computadora (uno por linea):
 *   READY, ASK, REC,<record>, START,<segundos>, T,<segundos>,
 *   HIT,<diana>,<puntaje>, UP,<diana>, END,<puntaje>,<record>,<nuevo>,<premio>,
 *   TOP,p1,...,p10 y NOM,n1,...,n10
 *
 * Comandos que recibe de la computadora:
 *   N,ABC -> iniciales del jugador, empieza la partida
 *   S -> igual que el boton INICIAR   R -> reiniciar
 *   T -> mandar el top 10             X -> borrar el top 10
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"
#include "UART.h"
#include "EEPROM.h"

// ---------------- Configuracion del juego ----------------
#define DURACION_JUEGO   30000   // ms que dura la partida (30 s)
#define TIEMPO_ABAJO     1200    // ms que la diana se queda tirada
#define TIEMPO_BLOQUEO   700     // ms que se ignora la LDR cuando la diana se levanta
#define TIEMPO_BUZZER    150     // ms que suena el buzzer en cada acierto
#define PUNTOS_ACIERTO   10
#define PUNTOS_PREMIO1   50      // puntaje minimo para el premio 1
#define PUNTOS_PREMIO2   100     // puntaje minimo para el premio 2
#define MUESTRAS_ADC     5
#define ANTIRREBOTE      200     // ms para los botones

// ---------------- Estados del juego ----------------
#define ESPERA    0
#define PIDIENDO  1   // ya se presiono INICIAR, esperando las iniciales
#define JUGANDO   2

// ---------------- Direcciones de la EEPROM ----------------
// 0        : numero magico para saber si la EEPROM ya se inicializo
// 1 a 20   : los 10 puntajes, 2 bytes cada uno (byte bajo y byte alto)
// 21 a 50  : las iniciales, 3 letras por puntaje
#define EE_MAGICO     0
#define EE_PUNTAJES   1
#define EE_NOMBRES    21
#define VALOR_MAGICO  0xA7

// ---------------- Datos de cada diana ----------------
// Cada posicion del arreglo es una diana (0, 1 y 2)
int  canalADC[3]   = {0, 1, 2};
unsigned int umbral[3]     = {1000, 920, 1000};   // arriba de esto cuenta como acierto
unsigned int liberacion[3] = {955, 820, 915};   // abajo de esto la LDR ya se "libero"
int  angArriba[3]  = {90, 0, 90};       // angulo con la diana parada
int  angAbajo[3]   = {0, 90, 0};        // angulo con la diana tirada (servo 2 va al reves)

bool dianaAbajo[3] = {false, false, false};
bool detectando[3] = {false, false, false};   // la LDR sigue iluminada desde el ultimo tiro
unsigned long tiempoCaida[3]   = {0, 0, 0};   // cuando se tiro la diana
unsigned long tiempoBloqueo[3] = {0, 0, 0};   // hasta cuando se ignora la LDR

// ---------------- Variables que usan las interrupciones ----------------
volatile unsigned long milisegundos = 0;
volatile bool flagIniciar   = false;
volatile bool flagReiniciar = false;
volatile bool flagTop       = false;
volatile bool flagBorrar    = false;
volatile bool flagNombre    = false;
volatile char nombreRecibido[3];
volatile unsigned long ultimoIniciar   = 0;
volatile unsigned long ultimoReiniciar = 0;

volatile int  cmdPos = 0;      // en que parte de "N,ABC" vamos
volatile char cmdLetras[3];

// ---------------- Variables del juego ----------------
int  estado = ESPERA;
unsigned int puntaje = 0;
char jugador[3] = {'-', '-', '-'};
unsigned long inicioJuego = 0;
unsigned long finBuzzer = 0;
bool buzzerSonando = false;
int  ultimoSegundo = -1;

char msg[64];


/* ======================= TIEMPO ======================= */

// Timer0 interrumpe cada 0.5 ms, entonces cada 2 interrupciones es 1 ms
volatile int mitad = 0;

ISR(TIMER0_COMPA_vect)
{
	mitad++;
	if (mitad >= 2) {
		mitad = 0;
		milisegundos++;
	}
}

// milisegundos es de 4 bytes, se lee con las interrupciones apagadas
// para que el timer no lo cambie a media lectura
unsigned long leerMilis(void)
{
	unsigned long t;
	uint8_t sreg = SREG;
	cli();
	t = milisegundos;
	SREG = sreg;
	return t;
}

void timer0_init(void)
{
	GPIO_SetPinDirection(&PORTD, 5, GPIO_OUTPUT);   // buzzer
	TCCR0A = (1 << WGM01);                // CTC
	TCCR0B = (1 << CS01) | (1 << CS00);   // prescaler 64
	OCR0A  = 124;                         // 16 MHz / 64 / 125 = 2 kHz -> 0.5 ms
	OCR0B  = 0;
	TIMSK0 = (1 << OCIE0A);
}


/* ======================= BOTONES ======================= */

ISR(INT0_vect)   // INICIAR
{
	if (milisegundos - ultimoIniciar >= ANTIRREBOTE) {
		ultimoIniciar = milisegundos;
		flagIniciar = true;
	}
}

ISR(INT1_vect)   // REINICIAR
{
	if (milisegundos - ultimoReiniciar >= ANTIRREBOTE) {
		ultimoReiniciar = milisegundos;
		flagReiniciar = true;
	}
}

void botones_init(void)
{
	GPIO_SetPinDirection(&PORTD, 2, GPIO_INPUT);
	GPIO_SetPinDirection(&PORTD, 3, GPIO_INPUT);
	GPIO_WritePin(&PORTD, 2, GPIO_HIGH);   // pull-up
	GPIO_WritePin(&PORTD, 3, GPIO_HIGH);   // pull-up

	EICRA = (1 << ISC01) | (1 << ISC11);   // INT0 e INT1 por flanco de bajada
	EIMSK = (1 << INT0) | (1 << INT1);
}


/* ======================= UART ======================= */

// Recibe los comandos de la computadora. Solo levanta banderas,
// lo que hay que hacer se hace en el main.
ISR(USART_RX_vect)
{
	char c = UDR0;   // leer UDR0 limpia la bandera RXC0

	if (cmdPos == 0) {
		if (c == 'S') flagIniciar = true;
		else if (c == 'R') flagReiniciar = true;
		else if (c == 'T') flagTop = true;
		else if (c == 'X') flagBorrar = true;
		else if (c == 'N') cmdPos = 1;
	}
	else if (cmdPos == 1) {          // despues de la N tiene que venir una coma
		if (c == ',') cmdPos = 2;
		else cmdPos = 0;
	}
	else {                           // cmdPos 2, 3 y 4 son las tres letras
		if (c >= 'a' && c <= 'z') c = c - 32;   // a mayuscula
		if (c >= 'A' && c <= 'Z') {
			cmdLetras[cmdPos - 2] = c;
			cmdPos++;
			if (cmdPos == 5) {       // ya llegaron las 3 letras
				cmdPos = 0;
				for (int k = 0; k < 3; k++) nombreRecibido[k] = cmdLetras[k];
				flagNombre = true;
			}
		} else {
			cmdPos = 0;              // caracter invalido, se descarta el comando
		}
	}
}


/* ======================= EEPROM (TOP 10) ======================= */

// La escritura de la EEPROM no se debe interrumpir
void eeprom_guardar(unsigned int dir, unsigned char dato)
{
	uint8_t sreg = SREG;
	cli();
	EEPROM_update(dir, dato);
	SREG = sreg;
}

unsigned int leerPuntaje(int i)
{
	unsigned int bajo = EEPROM_read(EE_PUNTAJES + i * 2);
	unsigned int alto = EEPROM_read(EE_PUNTAJES + i * 2 + 1);
	return (alto << 8) | bajo;
}

void guardarPuntaje(int i, unsigned int valor)
{
	eeprom_guardar(EE_PUNTAJES + i * 2, valor & 0xFF);
	eeprom_guardar(EE_PUNTAJES + i * 2 + 1, valor >> 8);
}

char leerLetra(int i, int k)
{
	return EEPROM_read(EE_NOMBRES + i * 3 + k);
}

void guardarLetra(int i, int k, char letra)
{
	eeprom_guardar(EE_NOMBRES + i * 3 + k, letra);
}

void top_borrar(void)
{
	for (int i = 0; i < 10; i++) {
		guardarPuntaje(i, 0);
		for (int k = 0; k < 3; k++) guardarLetra(i, k, '-');
	}
}

// Si la EEPROM es nueva (o tiene otro formato) se deja el top en ceros
void top_init(void)
{
	if (EEPROM_read(EE_MAGICO) != VALOR_MAGICO) {
		top_borrar();
		eeprom_guardar(EE_MAGICO, VALOR_MAGICO);
	}
}

// Mete el puntaje en su lugar del top 10 y recorre los de abajo
void top_insertar(unsigned int score, char *nom)
{
	unsigned int t[10];
	char n[10][3];

	for (int i = 0; i < 10; i++) {
		t[i] = leerPuntaje(i);
		for (int k = 0; k < 3; k++) n[i][k] = leerLetra(i, k);
	}

	if (score == 0 || score <= t[9]) return;   // no entra al top

	int pos = 0;
	while (pos < 10 && score <= t[pos]) pos++;   // con empate queda abajo

	for (int i = 9; i > pos; i--) {              // recorre una posicion hacia abajo
		t[i] = t[i - 1];
		for (int k = 0; k < 3; k++) n[i][k] = n[i - 1][k];
	}
	t[pos] = score;
	for (int k = 0; k < 3; k++) n[pos][k] = nom[k];

	for (int i = 0; i < 10; i++) {
		guardarPuntaje(i, t[i]);
		for (int k = 0; k < 3; k++) guardarLetra(i, k, n[i][k]);
	}
}

void top_enviar(void)
{
	UART_string("TOP");
	for (int i = 0; i < 10; i++) {
		sprintf(msg, ",%u", leerPuntaje(i));
		UART_string(msg);
	}
	UART_string("\n");

	UART_string("NOM");
	for (int i = 0; i < 10; i++) {
		sprintf(msg, ",%c%c%c", leerLetra(i, 0), leerLetra(i, 1), leerLetra(i, 2));
		UART_string(msg);
	}
	UART_string("\n");
}

void enviar_record(void)
{
	sprintf(msg, "REC,%u\n", leerPuntaje(0));
	UART_string(msg);
}


/* ======================= SERVOS ======================= */

// Igual que la libreria Servo de Arduino: 0 grados = 544 us, 180 grados = 2400 us
unsigned int angulo_a_us(int ang)
{
	return 544 + ((unsigned long)ang * 1856) / 180;
}

void servo_write(int i, int ang)
{
	unsigned int us = angulo_a_us(ang);

	if (i == 0) {
		OCR1A = us * 2;                     // Timer1: 0.5 us por cuenta
	} else if (i == 1) {
		OCR1B = us * 2;
	} else {
		OCR2A = ((us + 32) / 64) - 1;       // Timer2: 64 us por cuenta
	}
}

void pwm_init(void)
{
	GPIO_SetPinDirection(&PORTB, 1, GPIO_OUTPUT);   // servo 1
	GPIO_SetPinDirection(&PORTB, 2, GPIO_OUTPUT);   // servo 2
	GPIO_SetPinDirection(&PORTB, 3, GPIO_OUTPUT);   // servo 3

	// Timer1: Fast PWM modo 14 (TOP = ICR1), prescaler 8 -> 16 MHz / 8 / 40000 = 50 Hz
	ICR1   = 39999;
	TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);

	// Timer2: Fast PWM, prescaler 1024 -> unos 61 Hz, 64 us por cuenta
	TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20);
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
}

void servos_arriba(void)
{
	for (int i = 0; i < 3; i++) servo_write(i, angArriba[i]);
}


/* ======================= BUZZER ======================= */

// El buzzer es activo: basta con poner PD5 en alto para que suene
void buzzer_on(void)
{
	GPIO_WritePin(&PORTD, 5, GPIO_HIGH);
}

void buzzer_off(void)
{
	GPIO_WritePin(&PORTD, 5, GPIO_LOW);
}


/* ======================= ADC ======================= */

void adc_init(void)
{
	ADMUX  = (1 << REFS0);                                             // referencia AVcc
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler 128
	DIDR0  = (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D);               // apaga lo digital de PC0-PC2
}

unsigned int adc_leer(int canal)
{
	ADMUX = (1 << REFS0) | (canal & 0x07);   // cambia de canal

	ADCSRA |= (1 << ADSC);                   // la primera despues de cambiar se descarta
	while (ADCSRA & (1 << ADSC));

	unsigned long suma = 0;
	for (int i = 0; i < MUESTRAS_ADC; i++) {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));
		suma += ADC;
	}
	return suma / MUESTRAS_ADC;
}


/* ======================= JUEGO ======================= */

void registrar_punto(int i, unsigned long ahora)
{
	puntaje += PUNTOS_ACIERTO;

	servo_write(i, angAbajo[i]);   // tira la diana
	dianaAbajo[i] = true;
	tiempoCaida[i] = ahora;

	buzzer_on();
	buzzerSonando = true;
	finBuzzer = ahora + TIEMPO_BUZZER;

	sprintf(msg, "HIT,%d,%u\n", i + 1, puntaje);
	UART_string(msg);
}

void detectar(int i, unsigned int lectura, unsigned long ahora)
{
	// Con la diana tirada o recien levantada no se lee la LDR
	if (dianaAbajo[i] || ahora < tiempoBloqueo[i]) {
		detectando[i] = true;   // tiene que "liberarse" antes del siguiente tiro
		return;
	}

	if (lectura >= umbral[i] && detectando[i] == false) {
		detectando[i] = true;
		registrar_punto(i, ahora);
	}

	if (lectura <= liberacion[i]) {
		detectando[i] = false;
	}
}

void controlar_servo(int i, unsigned long ahora)
{
	if (dianaAbajo[i] && (ahora - tiempoCaida[i]) >= TIEMPO_ABAJO) {
		servo_write(i, angArriba[i]);   // levanta la diana
		dianaAbajo[i] = false;
		tiempoBloqueo[i] = ahora + TIEMPO_BLOQUEO;

		sprintf(msg, "UP,%d\n", i + 1);
		UART_string(msg);
	}
}

void limpiar_dianas(void)
{
	for (int i = 0; i < 3; i++) {
		dianaAbajo[i] = false;
		detectando[i] = false;
		tiempoCaida[i] = 0;
		tiempoBloqueo[i] = 0;
	}
	servos_arriba();
}

void iniciar_juego(void)
{
	limpiar_dianas();
	puntaje = 0;
	ultimoSegundo = DURACION_JUEGO / 1000;   // START ya manda el tiempo total
	inicioJuego = leerMilis();
	estado = JUGANDO;

	sprintf(msg, "START,%d\n", DURACION_JUEGO / 1000);
	UART_string(msg);
}

void terminar_juego(void)
{
	unsigned int recordAnterior = leerPuntaje(0);
	int nuevo = 0;
	int premio = 0;

	if (puntaje > recordAnterior) nuevo = 1;

	top_insertar(puntaje, jugador);
	estado = ESPERA;
	limpiar_dianas();

	if (nuevo == 1)                      premio = 3;
	else if (puntaje >= PUNTOS_PREMIO2)  premio = 2;
	else if (puntaje >= PUNTOS_PREMIO1)  premio = 1;
	else                                 premio = 0;

	UART_string("T,0\n");
	sprintf(msg, "END,%u,%u,%d,%d\n", puntaje, leerPuntaje(0), nuevo, premio);   // record ya actualizado
	UART_string(msg);
	top_enviar();
}

void reiniciar_juego(void)
{
	estado = ESPERA;
	puntaje = 0;
	limpiar_dianas();
	buzzer_off();
	buzzerSonando = false;

	UART_string("READY\n");
	enviar_record();
	top_enviar();
}

// INICIAR (boton o 'S'): todavia no empieza, primero pide las iniciales a la PC
void pedir_iniciales(void)
{
	if (estado == JUGANDO) return;
	estado = PIDIENDO;
	UART_string("ASK\n");
}


/* ======================= MAIN ======================= */

int main(void)
{
	botones_init();
	UART_init(9600, 8, 'N', 1);
	UCSR0B |= (1 << RXCIE0);   // interrupcion de recepcion
	pwm_init();
	adc_init();
	timer0_init();
	top_init();
	servos_arriba();

	sei();

	UART_string("READY\n");
	enviar_record();
	top_enviar();

	while (1)
	{
		unsigned long ahora = leerMilis();

		// ---- Comandos de la PC ----
		if (flagNombre) {
			flagNombre = false;
			if (estado == PIDIENDO) {
				for (int k = 0; k < 3; k++) jugador[k] = nombreRecibido[k];
				iniciar_juego();
			}
		}
		if (flagTop) {
			flagTop = false;
			top_enviar();
		}
		if (flagBorrar) {
			flagBorrar = false;
			top_borrar();
			top_enviar();
		}

		// ---- Botones (o 'S' y 'R' desde la PC) ----
		if (flagReiniciar) {
			flagReiniciar = false;
			reiniciar_juego();
		}
		if (flagIniciar) {
			flagIniciar = false;
			pedir_iniciales();
		}

		// ---- Apagar el buzzer ----
		if (buzzerSonando && ahora >= finBuzzer) {
			buzzer_off();
			buzzerSonando = false;
		}

		// ---- Partida en curso ----
		if (estado == JUGANDO) {
			for (int i = 0; i < 3; i++) {
				unsigned int lectura = adc_leer(canalADC[i]);
				detectar(i, lectura, ahora);
				controlar_servo(i, ahora);
			}

			unsigned long transcurrido = ahora - inicioJuego;
			if (transcurrido >= DURACION_JUEGO) {
				terminar_juego();
			} else {
				// segundos que faltan, redondeado hacia arriba
				int restante = (DURACION_JUEGO - transcurrido + 999) / 1000;
				if (restante != ultimoSegundo) {
					ultimoSegundo = restante;
					sprintf(msg, "T,%d\n", restante);
					UART_string(msg);
				}
			}
		}
	}
}