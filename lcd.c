//lcd.c
#include<LPC21xx.h>
#include "types.h"
#include "lcd_defines.h"
#include "defines.h"
#include "delay.h"
void WriteLCD(u8 byte)
{
  //write byte on pins
  WRITEBYTE(IOPIN0,LCD_DATA,byte);
	//select write operation to write data onto lcd contoller register(DDRAM)
	//for selecting write operation we apply logic 0 onto RW register
	IOCLR0=1<<LCD_RW;
	//apply high to low pulse through enable pin for store data from pins to DDRAM
	IOSET0=1<<LCD_EN;
	delay_us(1);
	IOCLR0=1<<LCD_EN;
	delay_ms(2);
}
void CmdLCD(u8 cmd)
{
	//select command register for controlling the lcd
	//for selecting cmd register we apply logic 0 onto RS register
	IOCLR0=1<<LCD_RS;
	//write cmd to lcd
	WriteLCD(cmd);
}
void InitLCD(void)
{
	//cfg p0.8 to p0.15,p0.16,p0.17,p0.18 as gpio output pins
	IODIR0|=((0xFF<<LCD_DATA)|(1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_EN));
	//wait for 15ms to stabilize the lcd internal power supply and oscillator
	delay_ms(15);
	//0x30 is a function set cmd in lcd.it tells the lcd to intialize in 8 bit interface mode.
	//that's why we give 0x30 again and again to tell the lcd work in 8 bit mode
	CmdLCD(0x30);
	//then again wait 4.1 ms
	delay_ms(4);
	delay_us(100);
	CmdLCD(0x30);
	//again wait 100 ms
	delay_us(100);
	CmdLCD(0x30);
	//give which mode you want and how many lines you wnat
	CmdLCD(MODE_8BIT_2LINE);
	//diplay on cursor blinking
	CmdLCD(DSP_ON_CUR_OFF);
	//clear the LCD screen
	CmdLCD(CLEAR_LCD);
	//shift the cursor to right
	CmdLCD(SHIFT_CUR_RIGHT);
}
void CharLCD(u8 ch)
{
	//select the data register to diaplay the character on the LCD
	//for selecting data register we apply logic 1 onto RS register
	IOSET0=1<<LCD_RS;
	//write character to lcd through ddram
	WriteLCD(ch);
}
void StrLCD(s8 *str)
{
	while(*str)
	{
		CharLCD(*str);
		str++;
	}
}
void U32LCD(u32 n)
{
	u8 a[20];
	s32 i=0;
	if(n==0)
	{
		CharLCD('0');
	}
	else
	{
		while(n>0)
		{
			a[i]=(n%10)+48;
			n=n/10;
			i++;
		}
		for(--i;i>=0;i--)
		{
			CharLCD(a[i]);
		}
	}
}
void S32LCD(s32 n)
{
	if(n<0)
	{
		CharLCD('-');
		n=-n;
	}
	U32LCD(n);
}
void F32LCD(f32 fn,u8 nDp)
{
	u32 n;
	s32 i;
	if(fn<0.0)
	{
		CharLCD('-');
		fn=-fn;
	}
	n=fn;
	U32LCD(n);
	CharLCD('.');
	for(i=0;i<nDp;i++)
	{
		fn=(fn-n)*10;
		n=fn;
		CharLCD(n+48);
	}
}
void BuildCGRAM(u8 *p,u8 nBytes)
{
	s32 i;
	//start the cgram
	CmdLCD(GOTO_CGRAM_START);
	//write into cgram
	//select the data register for selecting apply logic 1 onto RS register
	IOSET0=1<<LCD_RS;
	for(i=0;i<nBytes;i++)
	{
		WriteLCD(p[i]);
	}
	CmdLCD(GOTO_LINE1_POS0);
}
void LCD_GotoXY(u8 row,u8 col)
{
    if(row==0)
        CmdLCD(0x80+col);
    else
        CmdLCD(0xC0+col);
}

void LCD_StringXY(u8 row,u8 col,s8 *str)
{
    LCD_GotoXY(row,col);
    StrLCD(str);
}
void LCD_CharXY(u8 row, u8 col, u8 ch)
{
    if(row == 0)
        CmdLCD(0x80 + col);   // Line 1
    else
        CmdLCD(0xC0 + col);   // Line 2

    CharLCD(ch);
}
 /*==========================================================
                SECURE ENTRY DOOR CHARACTERS
  ==========================================================*/

/*
   Door characters use CGRAM locations 2, 3, 4 and 5.

   CGRAM locations:

       2 -> Door left
       3 -> Door right
       4 -> Open door
       5 -> Door frame
*/


/*----------------------------------------------------------
                    CLOSED DOOR LEFT
  ----------------------------------------------------------*/

u8 DoorLeft[8] =
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


/*----------------------------------------------------------
                    CLOSED DOOR RIGHT
  ----------------------------------------------------------*/

u8 DoorRight[8] =
{
    0x1F,
    0x11,
    0x11,
    0x15,
    0x11,
    0x11,
    0x11,
    0x1F
};


/*----------------------------------------------------------
                    OPEN DOOR LEFT
  ----------------------------------------------------------*/

u8 DoorOpenLeft[8] =
{
    0x1F,
    0x10,
    0x10,
    0x10,
    0x10,
    0x10,
    0x10,
    0x1F
};


/*----------------------------------------------------------
                    OPEN DOOR RIGHT
  ----------------------------------------------------------*/

u8 DoorOpenRight[8] =
{
    0x01,
    0x03,
    0x07,
    0x0F,
    0x1F,
    0x0F,
    0x07,
    0x03
};


/*==========================================================
                INITIALIZE DOOR CGRAM
  ==========================================================*/

void SecureEntry_CGRAM_Init(void)
{
    u8 i;

    /*
       CGRAM location 2
       Address = 0x50
    */

    CmdLCD(0x50);

    for(i = 0; i < 8; i++)
    {
        CharLCD(DoorLeft[i]);
    }


    /*
       CGRAM location 3
       Address = 0x58
    */

    CmdLCD(0x58);

    for(i = 0; i < 8; i++)
    {
        CharLCD(DoorRight[i]);
    }


    /*
       CGRAM location 4
       Address = 0x60
    */

    CmdLCD(0x60);

    for(i = 0; i < 8; i++)
    {
        CharLCD(DoorOpenLeft[i]);
    }


    /*
       CGRAM location 5
       Address = 0x68
    */

    CmdLCD(0x68);

    for(i = 0; i < 8; i++)
    {
        CharLCD(DoorOpenRight[i]);
    }


    /* Return to DDRAM */
    CmdLCD(0x80);
}


/*==========================================================
                    CLOSED DOOR
  ==========================================================*/

void ShowClosedDoor(void)
{
    CmdLCD(CLEAR_LCD);

    /*
       Center "SECURE"
    */

    LCD_StringXY(0,5,"SECURE");


    /*
       Door on second line

       CGRAM character numbers:

           2 -> DoorLeft
           3 -> DoorRight
    */

    LCD_CharXY(1,6,2);
    LCD_CharXY(1,7,3);


    LCD_StringXY(1,9,"ENTRY");
}


/*==========================================================
                    OPENING DOOR
  ==========================================================*/

void ShowOpeningDoor(void)
{
    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,5,"SECURE");


    /*
       Opening-door characters

           4 -> OpenDoorLeft
           5 -> OpenDoorRight
    */

    LCD_CharXY(1,6,4);
    LCD_CharXY(1,7,5);


    LCD_StringXY(1,9,"ENTRY");
}
