#include <LPC21xx.h>

#include "types.h"
#include "lcd.h"
#include "lcd_defines.h"
#include "kpm.h"
#include "delay.h"
#include "spi.h"
#include "spi_eeprom.h"
#include "user.h"
#include "eint.h"
#include "switch.h"
#include "uart.h"
#include "gsm.h"
#include "rtc.h"
#include "otp.h"
#include "door.h"
#include "door_cgram.h"


int main(void)
{
		//u8 eeprom_status;
    /*======================================================
                        BASIC INITIALIZATION
      ======================================================*/

    InitLCD();

    Init_KPM();


    /*======================================================
                        SWITCH / EINT
      ======================================================*/

    /*
       EINT1 is used for User Management.

       P0.3 = EINT1

       UART0 continues to use:

       P0.0 = TXD0
       P0.1 = RXD0
    */

    Switch_Init();

    Init_EINT();


    /*======================================================
                        UART0 INITIALIZATION
      ======================================================*/

    UART0_Init();


    /*======================================================
                        STARTUP SCREEN
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(
        0,
        1,
        " SECURE DOOR"
    );

    LCD_StringXY(
        1,
        0,
        " ENTRY SECURITY"
    );

    delay_s(1);


    /*======================================================
                        GSM INITIALIZATION
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(
        0,
        0,
        "GSM INITIALIZING"
    );

    LCD_StringXY(
        1,
        0,
        "PLEASE WAIT..."
    );

    //GSM_Init();


    /*======================================================
                        GSM NETWORK STATUS
      ======================================================*/

    /*
       GSM_Init() performs the GSM startup sequence.

       Network checking is intentionally NOT placed inside
       an endless while loop here.

       This prevents the complete SecureEntry system from
       getting stuck if the modem temporarily has no network.
    */

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(
        0,
        0,
        "GSM NETWORK"
    );

    LCD_StringXY(
        1,
        0,
        "CHECKING..."
    );

    if(GSM_CheckNetwork())
    {
        CmdLCD(CLEAR_LCD);

        LCD_StringXY(
            0,
            0,
            "GSM CONNECTED"
        );

        LCD_StringXY(
            1,
            0,
            "SMS READY"
        );

        delay_s(2);
    }
    else
    {
        /*
           Network is not available at this moment.

           Do NOT block the entire project.

           GSM_CheckNetwork() can be called again later
           when required.
        */

        CmdLCD(CLEAR_LCD);

        LCD_StringXY(
            0,
            0,
            "NETWORK NOT READY"
        );

        LCD_StringXY(
            1,
            0,
            "CONTINUING..."
        );

        delay_s(2);
    }


    /*======================================================
                        RTC INITIALIZATION
      ======================================================*/

    RTC_Init();


    /*======================================================
                        OTP INITIALIZATION
      ======================================================*/

    OTP_Init();
		
		Init_SPI0();

		//eeprom_status = EEPROM_Diagnose();

		/*if(eeprom_status != EEPROM_OK)
		{
				EEPROM_DisplayDiagnosticError(eeprom_status);
				//delay_s(2);
		}*/


    /*======================================================
                        USER INITIALIZATION
      ======================================================*/

    InitializeUsers();


    /*======================================================
                        DOOR INITIALIZATION
      ======================================================*/

    Door_Init();
		
		Door_CGRAM_Init();


    /*======================================================
                        MAIN PROGRAM LOOP
      ======================================================*/

		SetRTCTimeInfo(9,0,0);
		SetRTCDateInfo(25,9,2026);
		
    while(1)
    {
        /*--------------------------------------------------
                    BACKGROUND SMS CHECK

           GSM_CheckSMS() is NON-BLOCKING.

           It checks incoming SMS data and allows the GSM
           driver to process:

               BLOCK <ID>
               UNBLOCK <ID>

           without stopping the main application.
          --------------------------------------------------*/

        GSM_CheckSMS();


        /*--------------------------------------------------
                    USER MANAGEMENT SWITCH
          --------------------------------------------------*/

        if(edit_request)
        {
            edit_request = 0;

            UserManagement();
        }


        /*--------------------------------------------------
                        NORMAL LOGIN
          --------------------------------------------------*/

        else
        {
            Login();
        }
    }
}
