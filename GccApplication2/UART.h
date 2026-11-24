#ifndef UART_H_
#define UART_H_

void UART_init(unsigned long baudRate, unsigned char dataSize, char parity, unsigned char stopBits);
unsigned char UART_read();
void UART_write(unsigned char);
void UART_string(char*);

#endif /* UART_H_ */
