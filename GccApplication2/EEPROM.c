#include "EEPROM.h"

unsigned char EEPROM_read(unsigned int uiAddress)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE)) ;
	/* Set up address register */
	EEAR = uiAddress;
	/* Start eeprom read by writing EERE */
	EECR |= (1<<EERE);
	/* Return data from Data Register */
	return EEDR;
}

void EEPROM_write(unsigned int uiAddress, unsigned char ucData)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE)) ;
	/* Set up address and Data Registers */
	EEAR = uiAddress;
	EEDR = ucData;
	/* Write logical one to EEMPE */
	EECR |= (1<<EEMPE);
	/* Start eeprom write by setting EEPE */
	EECR |= (1<<EEPE);
}

void EEPROM_update(unsigned int uiAddress, unsigned char ucData)
{
	/* Lee el valor actual guardado en esa direccion */
	unsigned char valorActual = EEPROM_read(uiAddress);

	/* Solo escribe si el nuevo valor es diferente al que ya esta guardado */
	if(valorActual != ucData)
	{
		EEPROM_write(uiAddress, ucData);
	}
}