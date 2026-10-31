//spi_eeprom.c

#include <LPC21xx.h>
#include "types.h"
#include "spi.h"
#include "spi_defines.h"
#include "spi_eeprom_defines.h"
#include "spi_eeprom.h"
#include "delay.h"


/* =========================================================
   SEND COMMAND TO EEPROM
   ========================================================= */

void Cmd_25LC512(u8 cmd)
{
    IOCLR0 = 1 << CS;

    SPI0(cmd);

    IOSET0 = 1 << CS;
}


/* =========================================================
   WRITE ONE BYTE
   ========================================================= */

void ByteWrite_25LC512(u16 wBuffAddr, u8 dat)
{
    Cmd_25LC512(WREN);

    IOCLR0 = 1 << CS;

    SPI0(WRITE);
    SPI0(wBuffAddr >> 8);
    SPI0(wBuffAddr);
    SPI0(dat);

    IOSET0 = 1 << CS;

    delay_ms(10);

    Cmd_25LC512(WRDI);
}


/* =========================================================
   READ ONE BYTE
   ========================================================= */

u8 ByteRead_25LC512(u16 rBuffAddr)
{
    u8 dat;

    IOCLR0 = 1 << CS;

    SPI0(READ);
    SPI0(rBuffAddr >> 8);
    SPI0(rBuffAddr);

    dat = SPI0(0x00);

    IOSET0 = 1 << CS;

    return dat;
}


/* =========================================================
   READ EEPROM STATUS REGISTER
   ========================================================= */

static u8 EEPROM_ReadStatus(void)
{
    u8 status;

    IOCLR0 = 1 << CS;

    SPI0(RDSR);
    status = SPI0(0x00);

    IOSET0 = 1 << CS;

    return status;
}


/* =========================================================
   EEPROM DIAGNOSTIC

   WREN -> RDSR

   Bit 1 of EEPROM status register is WEL.

   Connected EEPROM:
       WREN sets WEL = 1

   Disconnected EEPROM:
       WEL will not be correctly set
       because EEPROM is not responding.
   ========================================================= */

u8 EEPROM_Diagnose(void)
{
    u8 status;

    /* Enable write operation */
    Cmd_25LC512(WREN);

    /* Read status register */
    status = EEPROM_ReadStatus();

    /* Check Write Enable Latch bit */
    if((status & 0x02) == 0)
    {
        return EEPROM_ERROR;
    }

    /* Disable write operation */
    Cmd_25LC512(WRDI);

    return EEPROM_OK;
}
