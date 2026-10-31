// spi.c

#include <LPC21xx.h>
#include "types.h"
#include "spi_defines.h"

void Init_SPI0(void)
{
    PINSEL0 |= SCK0 | MISO0 | MOSI0;

    S0SPCCR = 14;

    // SPI Mode 0, Master, MSB first
    S0SPCR = (1 << MSTR_BIT);

    // CS = P0.7, initially HIGH
    IOSET0 = 1 << CS;
    IODIR0 |= 1 << CS;
}

u8 SPI0(u8 dat)
{
    S0SPDR = dat;

    while(((S0SPSR >> SPIF_BIT) & 1) == 0);

    return S0SPDR;
}
