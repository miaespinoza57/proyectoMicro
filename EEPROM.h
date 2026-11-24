#ifndef EEPROM_H_
#define EEPROM_H_

#include <avr/io.h>

unsigned char EEPROM_read(unsigned int);
void EEPROM_write(unsigned int, unsigned char);
void EEPROM_update(unsigned int, unsigned char);
#endif /* EEPROM_H_ */