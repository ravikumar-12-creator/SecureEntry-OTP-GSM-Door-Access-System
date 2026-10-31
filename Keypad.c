//kpm.c

// 4x4 Matrix Keypad driver.
// This file contains functions to:
//     1. Initialize the keypad GPIO pins.
//     2. Detect whether any key is pressed.
//     3. Identify the pressed row.
//     4. Identify the pressed column.
//     5. Find the actual key value.
//     6. Read a complete number from the keypad.

// Keypad arrangement:
//
//        C0   C1   C2   C3
//     R0  7    8    9    B
//     R1  4    5    6    /
//     R2  1    2    3    -
//     R3  C    0    E    +

//==============================================================

#include <LPC21xx.h>
#include "types.h"
#include "kpm_defines.h"
#include "defines.h"
#include "lcd_defines.h"
#include "lcd.h"
#include "delay.h"

/*==============================================================
                    EXTERNAL INTERRUPT FLAG
  =============================================================*/

extern volatile u8 edit_request;


/*==============================================================
                         KEYPAD LUT
  =============================================================*/

u8 kpmLUT[4][4] =
{
    /* Row 0 */
    {'1','2','3','A'},

    /* Row 1 */
    {'4','5','6','B'},

    /* Row 2 */
    {'7','8','9','C'},

    /* Row 3 */
    {'*','0','#','D'}
};


/*==============================================================
                          Init_KPM()
  =============================================================*/

void Init_KPM(void)
{
    /*
       Configure four row pins as output pins.
    */

    WRITENIBBLE(IODIR1,ROW0,15);
}


/*==============================================================
                           colscan()
  =============================================================*/

u32 colscan(void)
{
    /*
       0 -> Key pressed
       1 -> No key pressed
    */

    if(READNIBBLE(IOPIN1,COL0) < 15)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


/*==============================================================
                           rowcheck()
  =============================================================*/

u32 rowcheck(void)
{
    u32 rno;

    for(rno=0; rno<4; rno++)
    {
        WRITENIBBLE(IOPIN1,ROW0,(~(1<<rno)));

        if(colscan() == 0)
        {
            break;
        }
    }

    IOCLR1 = 15 << ROW0;

    return rno;
}


/*==============================================================
                           colcheck()
  =============================================================*/

u32 colcheck(void)
{
    u32 cno;

    for(cno=0; cno<4; cno++)
    {
        if(READBIT(IOPIN1,(cno+COL0)) == 0)
        {
            break;
        }
    }

    return cno;
}


/*==============================================================
                           keyscan()
  =============================================================*/

u32 keyscan(void)
{
    u32 row;
    u32 col;
    u32 key;

    /*----------------------------------------------------------
                       WAIT FOR KEY PRESS
      ----------------------------------------------------------*/

    while(colscan());


    /*----------------------------------------------------------
                          FIND ROW
      ----------------------------------------------------------*/

    row = rowcheck();


    /*----------------------------------------------------------
                         FIND COLUMN
      ----------------------------------------------------------*/

    col = colcheck();


    /*----------------------------------------------------------
                         FIND KEY VALUE
      ----------------------------------------------------------*/

    key = kpmLUT[row][col];


    /*----------------------------------------------------------
                       WAIT FOR KEY RELEASE
      ----------------------------------------------------------*/

    while(!colscan());


    return key;
}


/*==============================================================
                           readnum()
  =============================================================*/

u32 readnum(void)
{
    u32 num = 0;
    u8 key;

    while(1)
    {
        key = keyscan();

        if(key >= '0' && key <= '9')
        {
            num = (num * 10) + (key - 48);
        }
        else
        {
            break;
        }
    }

    return num;
}


/*==============================================================
                         ReadNumLCD()
  =============================================================*/

u32 ReadNumLCD(void)
{
    u32 num = 0;
    u8 key;
    u8 digits = 0;
    u8 pos = 0;

    CmdLCD(GOTO_LINE2_POS0);

    while(1)
    {
        key = keyscan();


        /*======================================================
                         NUMBER KEY
          ======================================================*/

        if(key >= '0' && key <= '9')
        {
            num = (num * 10) + (key - '0');

            LCD_CharXY(1,pos,key);

            pos++;
            digits++;
        }


        /*======================================================
                          BACKSPACE
          ======================================================*/

        else if(key == KEY_BACKSPACE)
        {
            if(digits > 0)
            {
                num = num / 10;

                digits--;
                pos--;

                LCD_CharXY(1,pos,' ');

                LCD_GotoXY(1,pos);
            }
        }


        /*======================================================
                            CLEAR
          ======================================================*/

        else if(key == KEY_CLEAR)
        {
            num = 0;
            digits = 0;
            pos = 0;

            CmdLCD(GOTO_LINE2_POS0);

            StrLCD("                ");
        }


        /*======================================================
                            ENTER
          ======================================================*/

        else if(key == KEY_ENTER)
        {
            break;
        }
    }

    return num;
}


/*==============================================================
                         keyscan_nb()
  =============================================================*/

/*
   Non-blocking keypad scan.

   Return:
       0 -> No key pressed
       key value -> Key pressed
*/

u32 keyscan_nb(void)
{
    u32 row;
    u32 col;
    u32 key;

    /*-----------------------------------------------
                    NO KEY PRESSED
      ------------------------------------------------*/

    if(colscan())
    {
        return 0;
    }

    /*-----------------------------------------------
                    KEY PRESSED
      ------------------------------------------------*/

    /* Small debounce delay */
    delay_ms(20);

    /* Check again after debounce */
    if(colscan())
    {
        return 0;
    }

    row = rowcheck();
    col = colcheck();

    key = kpmLUT[row][col];

    /*-----------------------------------------------
                    WAIT FOR KEY RELEASE
      ------------------------------------------------*/

    while(!colscan());

    /*-----------------------------------------------
              WAIT FOR RELEASE TO STABILIZE
      ------------------------------------------------*/

    delay_ms(20);

    /* Make sure key is really released */
    while(!colscan());

    return key;
}


/*==============================================================
                       ReadNumLCDAt()
  =============================================================*/

/*
   Reads a number from a specified LCD row and starting position.

   Supports:

       0-9          -> Number entry
       KEY_BACKSPACE -> Delete last digit
       KEY_CLEAR     -> Clear entered number
       KEY_ENTER     -> Accept number

   IMPORTANT:

       This function also checks edit_request continuously.

       If EINT0 is pressed while the user is entering
       an ID/password, the function exits immediately.

       This allows the main program to enter ADMIN MODE
       without waiting for the current login operation
       to finish.
*/

/*u32 ReadNumLCDAt(u8 row, u8 start_pos)
{
    u32 num = 0;
    u32 key;
    u8 digits = 0;
    u8 pos = start_pos;

    LCD_GotoXY(row, start_pos);

    while(1)
    {*/
        /* Check Admin interrupt */
       /* if(edit_request)
            return num;

        key = keyscan_nb();

        if(key == 0)
            continue;
*/
        /*======================================================
                          NUMBER KEY
          ======================================================*/

/*        if(key >= '0' && key <= '9')
        {*/
            /*
             * USER ID MUST BE EXACTLY 4 CHARACTERS
             */

  /*          if(digits < 4)
            {
                num = (num * 10) + (key - '0');

                LCD_CharXY(row, pos, key);

                pos++;
                digits++;
            }
            else
            {*/
                /*
                 * 5th character is not accepted.
                 */

               /* CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "ENTER 4");
                LCD_StringXY(1, 0, "CHARACTERS ONLY");

                delay_s(2);

                CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "ENTER USER ID");
                LCD_StringXY(1, 0, "ID:");

                digits = 0;
                pos = start_pos;
                num = 0;

                LCD_GotoXY(row, start_pos);
            }
        }*/

        /*======================================================
                            BACKSPACE
          ======================================================*/

        /*else if(key == KEY_BACKSPACE)
        {
            if(digits > 0)
            {
                num = num / 10;

                digits--;
                pos--;

                LCD_CharXY(row, pos, ' ');
                LCD_GotoXY(row, pos);
            }
        }*/

        /*======================================================
                              CLEAR
          ======================================================*/

        /*else if(key == KEY_CLEAR)
        {
            num = 0;
            digits = 0;
            pos = start_pos;

            while(pos < 16)
            {
                LCD_CharXY(row, pos, ' ');
                pos++;
            }

            pos = start_pos;

            LCD_GotoXY(row, pos);
        }
*/
        /*======================================================
                              ENTER
          ======================================================*/

  /*      else if(key == KEY_ENTER)
        {*/
            /*
             * Accept ONLY exactly 4 characters.
             */

           /* if(digits == 4)
            {
                return num;
            }

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "ENTER 4");
            LCD_StringXY(1, 0, "CHARACTERS ONLY");

            delay_s(2);

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "ENTER USER ID");
            LCD_StringXY(1, 0, "ID:");

            num = 0;
            digits = 0;
            pos = start_pos;

            LCD_GotoXY(row, start_pos);
        }
    }
}
*/

/*==============================================================
                    BACKWARD COMPATIBILITY
  =============================================================*/

/*
 * Existing programs can continue using ReadNumLCD().
 *
 * It starts input at line 2, position 0.
 */
/*==============================================================
                    ReadPasswordLCDAt()
  =============================================================*/

/*
   Reads a password from the keypad.

   Display:
       Actual password -> *

   Example:

       User enters:
           1234

       LCD displays:
           ****

   Internally:
       num = 1234

   Supports:

       0-9             -> Password digit
       KEY_BACKSPACE   -> Delete last digit
       KEY_CLEAR       -> Clear password
       KEY_ENTER       -> Accept password

   Also checks edit_request continuously so that
   EINT0 can interrupt password entry.
*/

/*u32 ReadPasswordLCDAt(u8 row, u8 start_pos)
{
    u32 num = 0;
    u32 key;

    u8 digits = 0;
    u8 pos = start_pos;

    LCD_GotoXY(row, start_pos);

    while(1)
    {*/
        /* Check Admin interrupt */
       /* if(edit_request)
            return num;

        key = keyscan_nb();

        if(key == 0)
            continue;
*/
        /*======================================================
                          NUMBER KEY
          ======================================================*/

  /*      if(key >= '0' && key <= '9')
        {*/
            /*
             * PASSWORD MUST BE EXACTLY 4 CHARACTERS
             */

           /* if(digits < 4)
            {
                num = (num * 10) + (key - '0');

                LCD_CharXY(row, pos, '*');

                pos++;
                digits++;
            }
            else
            {*/
                /*
                 * 5th character is not accepted.
                 */

               /* CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "ENTER 4");
                LCD_StringXY(1, 0, "CHARACTERS");

                delay_s(2);

                CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "ENTER PASSWORD");
                LCD_StringXY(1, 0, "PASS:");

                num = 0;
                digits = 0;
                pos = start_pos;

                LCD_GotoXY(row, start_pos);
            }
        }*/

        /*======================================================
                            BACKSPACE
          ======================================================*/
/*
        else if(key == KEY_BACKSPACE)
        {
            if(digits > 0)
            {
                num = num / 10;

                digits--;
                pos--;

                LCD_CharXY(row, pos, ' ');
                LCD_GotoXY(row, pos);
            }
        }
*/
        /*======================================================
                              CLEAR
          ======================================================*/
/*
        else if(key == KEY_CLEAR)
        {
            num = 0;
            digits = 0;
            pos = start_pos;

            while(pos < 16)
            {
                LCD_CharXY(row, pos, ' ');
                pos++;
            }

            pos = start_pos;

            LCD_GotoXY(row, start_pos);
        }
*/
        /*======================================================
                              ENTER
          ======================================================*/

  /*      else if(key == KEY_ENTER)
        {*/
            /*
             * Accept ONLY exactly 4 characters.
             */

           /* if(digits == 4)
            {
                return num;
            }

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "ENTER 4");
            LCD_StringXY(1, 0, "CHARACTERS");

            delay_s(2);

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "ENTER PASSWORD");
            LCD_StringXY(1, 0, "PASS:");

            num = 0;
            digits = 0;
            pos = start_pos;

            LCD_GotoXY(row, start_pos);
        }
    }
}*/
