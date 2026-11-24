#ifndef LCD_H_
#define LCD_H_

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>

/* LCD en el PUERTO C (PC0 a PC5), igual que el esquema de Proteus.
   PD0 y PD1 quedan libres para la UART.
   OJO: D4-D7 deben ir en los bits 2 a 5 del puerto, porque lcd_send_command
   y lcd_write_character usan la mascara 0b00111100. */
#define DATA_BUS    PORTC
#define CTL_BUS     PORTC
#define DATA_DDR    DDRC
#define CTL_DDR     DDRC
#define LCD_RS      PC0
#define LCD_EN      PC1
#define LCD_D4      PC2
#define LCD_D5      PC3
#define LCD_D6      PC4
#define LCD_D7      PC5

#define LCD_CMD_CLEAR_DISPLAY   0x01
#define LCD_CMD_CURSOR_HOME     0x02

// Display control
#define LCD_CMD_DISPLAY_OFF                 0x08
#define LCD_CMD_DISPLAY_NO_CURSOR           0x0c
#define LCD_CMD_DISPLAY_CURSOR_NO_BLINK     0x0E
#define LCD_CMD_DISPLAY_CURSOR_BLINK        0x0F

// Function set
#define LCD_CMD_4BIT_2ROW_5X7   0x28
#define LCD_CMD_8BIT_2ROW_5X7   0x38

// Functions prototypes
void lcd_init(void);
void lcd_send_command(uint8_t);
void lcd_write_character(uint8_t );
void lcd_write_word(uint8_t []);
void lcd_clear(void);
void lcd_goto_xy(uint8_t , uint8_t );

#endif /* LCD_H_ */