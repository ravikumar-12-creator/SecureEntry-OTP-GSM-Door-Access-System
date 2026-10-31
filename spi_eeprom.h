//spi_eeprom.h

#ifndef __SPI_EEPROM_H
#define __SPI_EEPROM_H

#include "types.h"

void Cmd_25LC512(u8 Cmd);
void ByteWrite_25LC512(u16 wBuffAddr, u8 dat);
u8 ByteRead_25LC512(u16 rBuffAddr);

/* EEPROM diagnostic status */
#define EEPROM_OK       0
#define EEPROM_ERROR    1

/* EEPROM diagnostic */
u8 EEPROM_Diagnose(void);

#endif
