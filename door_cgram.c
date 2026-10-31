//door_cgram.c

#include "door_cgram.h"
#include "lcd.h"
#include "lcd_defines.h"

static u8 DoorOpen[8] =
{
    0x1F,
    0x11,
    0x11,
    0x11,
    0x11,
    0x15,
    0x09,
    0x03
};

static u8 DoorClose[8] =
{
    0x1F,
    0x11,
    0x11,
    0x11,
    0x11,
    0x11,
    0x11,
    0x1F
};


void Door_CGRAM_Init(void)
{
    u8 i;

    /* Select CGRAM location 0 */
    CmdLCD(0x40);

    for(i = 0; i < 8; i++)
        CharLCD(DoorOpen[i]);

    /* Select CGRAM location 1 */
    CmdLCD(0x48);

    for(i = 0; i < 8; i++)
        CharLCD(DoorClose[i]);

    /* Return to DDRAM */
    CmdLCD(0x80);
}


void Door_Open_Display(void)
{
    CmdLCD(0x01);

    CmdLCD(0x80);
	
		/* Display CGRAM character 0 */
    CharLCD(0x00);
	
    LCD_StringXY(0,3,"DOOR OPENING...");   
}


void Door_Close_Display(void)
{
    CmdLCD(0x01);

    CmdLCD(0x80);
	
    /* Display CGRAM character 1 */
    CharLCD(0x01);
	
		LCD_StringXY(0,3,"DOOR CLOSING...");
}
