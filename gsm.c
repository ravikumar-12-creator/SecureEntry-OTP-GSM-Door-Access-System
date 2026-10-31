#include <LPC21xx.h>
#include <string.h>

#include "types.h"
#include "uart.h"
#include "delay.h"
#include "gsm.h"
#include "lcd.h"
#include "lcd_defines.h"


/*==========================================================
                    GSM DEFINES
  ==========================================================*/

#define GSM_CR              '\r'
#define GSM_LF              '\n'
#define GSM_CTRL_Z          0x1A


#define GSM_RX_BUFFER_SIZE  400


/*==========================================================
                    INTERNAL SMS BUFFER
  ==========================================================*/

static s8 GSM_SMS_Buffer[GSM_RX_BUFFER_SIZE];

static u16 GSM_SMS_Index = 0;


/*==========================================================
                    LAST SMS DATA
  ==========================================================*/

static s8 GSM_SMS_Sender[32];

static s8 GSM_SMS_Message[120];

static u32 GSM_SMS_UserID;


/*==========================================================
                    UART WRAPPERS
  ==========================================================*/

u8 GSM_RxAvailable(void)
{
    return U0_RxAvailable();
}


u8 GSM_Rx(void)
{
    return U0_Rx();
}


void GSM_FlushRx(void)
{
    U0_FlushRx();
}


/*==========================================================
                    SEND AT COMMAND
  ==========================================================*/

void GSM_SendCommand(s8 *cmd)
{
    U0_TxStr(cmd);

    U0_Tx(GSM_CR);
}


/*==========================================================
                    WAIT FOR RESPONSE
  ==========================================================*/

u8 GSM_WaitForResponse(s8 *expected, u32 timeout)
{
    s8 response[180];

    u32 index = 0;

    u32 elapsed = 0;

    u8 ch;


    response[0] = '\0';


    while(elapsed < timeout)
    {
        while(U0_RxAvailable())
        {
            ch = U0_Rx();


            if(index < sizeof(response) - 1)
            {
                response[index++] = ch;

                response[index] = '\0';
            }


            /*----------------------------------------------
                        EXPECTED RESPONSE
              ----------------------------------------------*/

            if(strstr(response, expected) != 0)
            {
                return 1;
            }


            /*----------------------------------------------
                        MODEM ERROR
              ----------------------------------------------*/

            if(strstr(response, "ERROR") != 0)
            {
                return 0;
            }
        }


        delay_ms(1);

        elapsed++;
    }


    return 0;
}


/*==========================================================
                    TEST COMMAND
  ==========================================================*/

u8 GSM_TestCommand(s8 *cmd)
{
    GSM_FlushRx();

    GSM_SendCommand(cmd);

    return GSM_WaitForResponse("OK", 3000);
}


/*==========================================================
                    DIAGNOSTIC RESPONSE READER
  ==========================================================*/

static u8 GSM_DiagnosticReadResponse(
    s8 *response,
    u16 responseSize,
    u32 timeout
)
{
    u16 index = 0;

    u32 elapsed = 0;

    u8 ch;


    response[0] = '\0';


    while(elapsed < timeout)
    {
        while(U0_RxAvailable())
        {
            ch = U0_Rx();


            if(index < responseSize - 1)
            {
                response[index++] = ch;

                response[index] = '\0';
            }
        }


        /*
           If modem has already sent a response,
           give a small additional time for remaining
           characters to arrive.
        */

        if(index > 0)
        {
            delay_ms(20);

            while(U0_RxAvailable())
            {
                ch = U0_Rx();

                if(index < responseSize - 1)
                {
                    response[index++] = ch;

                    response[index] = '\0';
                }
            }

            return 1;
        }


        delay_ms(1);

        elapsed++;
    }


    return 0;
}


/*==========================================================
                    CHECK GSM MODULE
  ==========================================================*/

static u8 GSM_DiagnosticCheckModule(void)
{
    s8 response[100];


    GSM_FlushRx();

    GSM_SendCommand("AT");


    if(!GSM_DiagnosticReadResponse(
            response,
            sizeof(response),
            3000))
    {
        return GSM_DIAG_MODULE_FAIL;
    }


    if(strstr(response, "OK") != 0)
    {
        return GSM_DIAG_OK;
    }


    return GSM_DIAG_MODULE_FAIL;
}


/*==========================================================
                    CHECK SIM DIAGNOSTIC
  ==========================================================*/

static u8 GSM_DiagnosticCheckSIM(void)
{
    s8 response[120];


    GSM_FlushRx();

    GSM_SendCommand("AT+CPIN?");


    if(!GSM_DiagnosticReadResponse(
            response,
            sizeof(response),
            3000))
    {
        return GSM_DIAG_SIM_NOT_READY;
    }


    /* SIM is ready */

    if(strstr(response, "+CPIN: READY") != 0)
    {
        return GSM_DIAG_OK;
    }


    /* SIM PIN required */

    if(strstr(response, "+CPIN: SIM PIN") != 0)
    {
        return GSM_DIAG_SIM_NOT_READY;
    }


    /* SIM PUK required */

    if(strstr(response, "+CPIN: SIM PUK") != 0)
    {
        return GSM_DIAG_SIM_NOT_READY;
    }


    /*
       SIM missing / not inserted.

       Many SIMCom modems report:

       +CME ERROR: 10
    */

    if(strstr(response, "+CME ERROR: 10") != 0)
    {
        return GSM_DIAG_SIM_NOT_INSERTED;
    }


    if(strstr(response, "ERROR") != 0)
    {
        return GSM_DIAG_SIM_NOT_INSERTED;
    }


    return GSM_DIAG_SIM_NOT_READY;
}


/*==========================================================
                    CHECK NETWORK DIAGNOSTIC
  ==========================================================*/

static u8 GSM_DiagnosticCheckNetwork(void)
{
    s8 response[120];


    GSM_FlushRx();

    GSM_SendCommand("AT+CREG?");


    if(!GSM_DiagnosticReadResponse(
            response,
            sizeof(response),
            5000))
    {
        return GSM_DIAG_NO_NETWORK;
    }


    /*
       Registered on home network
    */

    if(strstr(response, "+CREG: 0,1") != 0)
    {
        return GSM_DIAG_OK;
    }


    /*
       Registered while roaming
    */

    if(strstr(response, "+CREG: 0,5") != 0)
    {
        return GSM_DIAG_OK;
    }


    /*
       Not registered and not searching
    */

    if(strstr(response, "+CREG: 0,0") != 0)
    {
        return GSM_DIAG_NO_NETWORK;
    }


    /*
       Registration/searching in progress
    */

    if(strstr(response, "+CREG: 0,2") != 0)
    {
        return GSM_DIAG_NETWORK_SEARCHING;
    }


    /*
       Registration denied
    */

    if(strstr(response, "+CREG: 0,3") != 0)
    {
        return GSM_DIAG_NETWORK_DENIED;
    }


    return GSM_DIAG_NO_NETWORK;
}


/*==========================================================
                    CHECK SIGNAL DIAGNOSTIC
  ==========================================================*/

static u8 GSM_DiagnosticCheckSignal(void)
{
    s8 response[100];

    s8 *p;

    u32 value = 0;

    u8 digitFound = 0;


    GSM_FlushRx();

    GSM_SendCommand("AT+CSQ");


    if(!GSM_DiagnosticReadResponse(
            response,
            sizeof(response),
            3000))
    {
        return GSM_DIAG_SIGNAL_UNKNOWN;
    }


    p = strstr(response, "+CSQ:");

    if(p == 0)
    {
        return GSM_DIAG_SIGNAL_UNKNOWN;
    }


    p += 5;


    /*
       Skip spaces
    */

    while(*p == ' ')
    {
        p++;
    }


    /*
       Read RSSI value
    */

    while(*p >= '0' && *p <= '9')
    {
        digitFound = 1;

        value = (value * 10) + (*p - '0');

        p++;
    }


    if(!digitFound)
    {
        return GSM_DIAG_SIGNAL_UNKNOWN;
    }


    /*
       99 = unknown / unavailable
    */

    if(value == 99)
    {
        return GSM_DIAG_SIGNAL_UNKNOWN;
    }


    /*
       RSSI 0 to 9 = very weak
    */

    if(value <= 9)
    {
        return GSM_DIAG_SIGNAL_WEAK;
    }


    return GSM_DIAG_OK;
}


/*==========================================================
                    COMPLETE GSM DIAGNOSIS
  ==========================================================*/

u8 GSM_Diagnose(void)
{
    u8 status;


    /*
       STEP 1
       Check whether modem is responding
    */

    status = GSM_DiagnosticCheckModule();

    if(status != GSM_DIAG_OK)
    {
        return status;
    }


    /*
       STEP 2
       Check SIM
    */

    status = GSM_DiagnosticCheckSIM();

    if(status != GSM_DIAG_OK)
    {
        return status;
    }


    /*
       STEP 3
       Check network registration
    */

    status = GSM_DiagnosticCheckNetwork();

    if(status != GSM_DIAG_OK)
    {
        return status;
    }


    /*
       STEP 4
       Check signal strength
    */

    status = GSM_DiagnosticCheckSignal();

    if(status != GSM_DIAG_OK)
    {
        return status;
    }


    return GSM_DIAG_OK;
}


/*==========================================================
                    DISPLAY GSM ERROR
  ==========================================================*/

void GSM_DisplayDiagnosticError(u8 status)
{
    CmdLCD(CLEAR_LCD);


    switch(status)
    {
        case GSM_DIAG_MODULE_FAIL:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"MODULE OFF");

            break;


        case GSM_DIAG_SIM_NOT_INSERTED:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"SIM NOT INSERTED");

            break;


        case GSM_DIAG_SIM_NOT_READY:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"SIM NOT READY");

            break;


        case GSM_DIAG_NO_NETWORK:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"NO NETWORK");

            break;


        case GSM_DIAG_NETWORK_SEARCHING:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"NETWORK SEARCH");

            break;


        case GSM_DIAG_NETWORK_DENIED:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"NETWORK DENIED");

            break;


        case GSM_DIAG_SIGNAL_WEAK:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"SIGNAL VERY WEAK");

            break;


        case GSM_DIAG_SIGNAL_UNKNOWN:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"SIGNAL UNKNOWN");

            break;


        default:

            LCD_StringXY(0,0,"GSM ERROR");
            LCD_StringXY(1,0,"UNKNOWN ERROR");

            break;
    }


    delay_s(2);
}


/*==========================================================
                    CHECK SIM
  ==========================================================*/

u8 GSM_CheckSIM(void)
{
    GSM_FlushRx();

    GSM_SendCommand("AT+CPIN?");

    return GSM_WaitForResponse("READY", 5000);
}


/*==========================================================
                    CHECK NETWORK
  ==========================================================*/

u8 GSM_CheckNetwork(void)
{
    /*
       IMPORTANT:

       We do NOT continuously search for network here.

       The working standalone program only checks the
       current +CREG response.

       Examples:

           +CREG: 0,1
           +CREG: 0,5

       Both mean registered.
    */

    GSM_FlushRx();

    GSM_SendCommand("AT+CREG?");

    /*
       We only require +CREG: to exist.

       This prevents the GSM initialization from becoming
       stuck in a long network-search loop.
    */

    return GSM_WaitForResponse("+CREG:", 5000);
}


/*==========================================================
                    SMS TEXT MODE
  ==========================================================*/

u8 GSM_SetSMSMode(void)
{
    GSM_FlushRx();

    GSM_SendCommand("AT+CMGF=1");

    return GSM_WaitForResponse("OK", 5000);
}


/*==========================================================
                    SMS CONFIGURATION
  ==========================================================*/

u8 GSM_ConfigureSMS(void)
{
    GSM_FlushRx();


    /*
       DIRECT SMS DELIVERY

       The modem sends:

       +CMT:
       sender information
       SMS body

       directly to UART.
    */

    GSM_SendCommand("AT+CNMI=1,2,0,0,0");

    if(!GSM_WaitForResponse("OK", 5000))
    {
        return 0;
    }


    GSM_FlushRx();

    return 1;
}


/*==========================================================
                    GSM INITIALIZATION
  ==========================================================*/

u8 GSM_Init(void)
{
    /*
       Allow modem to complete power-up.
    */

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM INITIALIZE");
    LCD_StringXY(1,0,"MODEM STARTING");

    delay_s(5);


    /*======================================================
                        ATE0
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: ATE0");
    LCD_StringXY(1,0,"PLEASE WAIT");

    if(!GSM_TestCommand("ATE0"))
    {
        LCD_StringXY(1,0,"ATE0 FAILED");

        delay_s(2);

        return 0;
    }


    LCD_StringXY(1,0,"ATE0 OK");

    delay_ms(500);


    /*======================================================
                        AT TEST
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: MODEM TEST");
    LCD_StringXY(1,0,"CHECKING");


    if(!GSM_TestCommand("AT"))
    {
        LCD_StringXY(1,0,"MODEM FAILED");

        delay_s(2);

        return 0;
    }


    LCD_StringXY(1,0,"MODEM OK");

    delay_ms(500);


    /*======================================================
                        SIM CHECK
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: SIM CHECK");
    LCD_StringXY(1,0,"CHECKING");


    if(!GSM_CheckSIM())
    {
        LCD_StringXY(1,0,"SIM FAILED");

        delay_s(2);

        return 0;
    }


    LCD_StringXY(1,0,"SIM READY");

    delay_ms(500);


    /*======================================================
                    NETWORK CHECK
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: NETWORK");
    LCD_StringXY(1,0,"CHECKING...");


    if(!GSM_CheckNetwork())
    {
        LCD_StringXY(1,0,"NETWORK RESP FAIL");

        delay_s(2);

        return 0;
    }


    /*
       IMPORTANT:

       We are NOT waiting here for ,1 or ,5.

       We only verify that the modem responds to AT+CREG?.

       This is the same philosophy as your working
       standalone GSM program.
    */

    LCD_StringXY(1,0,"NETWORK RESP OK");

    delay_ms(500);


    /*======================================================
                    SMS TEXT MODE
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: SMS MODE");
    LCD_StringXY(1,0,"SETTING");


    if(!GSM_SetSMSMode())
    {
        LCD_StringXY(1,0,"CMGF FAILED");

        delay_s(2);

        return 0;
    }


    LCD_StringXY(1,0,"TEXT MODE OK");

    delay_ms(500);


    /*======================================================
                    SMS DIRECT DELIVERY
      ======================================================*/

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM: SMS RX");
    LCD_StringXY(1,0,"CONFIGURING");


    if(!GSM_ConfigureSMS())
    {
        LCD_StringXY(1,0,"CNMI FAILED");

        delay_s(2);

        return 0;
    }


    LCD_StringXY(1,0,"SMS RX READY");

    delay_ms(500);


    /*======================================================
                    RESET RECEIVER
      ======================================================*/

    GSM_ResetSMSReceiver();


    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0,0,"GSM READY");
    LCD_StringXY(1,0,"SMS ACTIVE");

    delay_s(1);


    return 1;
}


/*==========================================================
                    SEND SMS
  ==========================================================*/

u8 GSM_SendSMS(s8 *phone, s8 *message)
{
    /*
       Clear old UART data before starting a new SMS
       transaction.
    */

    GSM_FlushRx();


    /*======================================================
                    ENSURE TEXT MODE
      ======================================================*/

    GSM_SendCommand("AT+CMGF=1");

    if(!GSM_WaitForResponse("OK", 3000))
    {
        return 0;
    }


    /*======================================================
                    SEND CMGS COMMAND
      ======================================================*/

    GSM_FlushRx();

    U0_TxStr("AT+CMGS=\"");

    U0_TxStr(phone);

    U0_TxStr("\"");

    U0_Tx(GSM_CR);


    /*======================================================
                    WAIT FOR >
      ======================================================*/

    if(!GSM_WaitForResponse(">", 5000))
    {
        return 0;
    }


    /*======================================================
                    SEND MESSAGE
      ======================================================*/

    U0_TxStr(message);


    /*======================================================
                    CTRL+Z
      ======================================================*/

    U0_Tx(GSM_CTRL_Z);


    /*======================================================
                    WAIT FOR FINAL RESPONSE
      ======================================================*/

    if(GSM_WaitForResponse("OK", 30000))
    {
        return 1;
    }


    return 0;
}


/*==========================================================
                    BLOCK COMMAND CHECK
  ==========================================================*/

static u8 GSM_IsBlockCommand(s8 *msg, u32 *id)
{
    u32 value = 0;

    u32 i = 6;

    u8 digitFound = 0;


    if(strncmp(msg, "BLOCK ", 6) != 0)
    {
        return 0;
    }


    while(msg[i] >= '0' && msg[i] <= '9')
    {
        digitFound = 1;

        value = (value * 10) + (msg[i] - '0');

        i++;
    }


    if(!digitFound)
    {
        return 0;
    }


    if(msg[i] != '\0')
    {
        return 0;
    }


    *id = value;

    return 1;
}


/*==========================================================
                    UNBLOCK COMMAND CHECK
  ==========================================================*/

static u8 GSM_IsUnblockCommand(s8 *msg, u32 *id)
{
    u32 value = 0;

    u32 i = 8;

    u8 digitFound = 0;


    if(strncmp(msg, "UNBLOCK ", 8) != 0)
    {
        return 0;
    }


    while(msg[i] >= '0' && msg[i] <= '9')
    {
        digitFound = 1;

        value = (value * 10) + (msg[i] - '0');

        i++;
    }


    if(!digitFound)
    {
        return 0;
    }


    if(msg[i] != '\0')
    {
        return 0;
    }


    *id = value;

    return 1;
}


/*==========================================================
                    PARSE COMPLETE +CMT SMS
  ==========================================================*/

static u8 GSM_ParseCMT(
    s8 *buffer,
    s8 *sender,
    u8 senderSize,
    s8 *message,
    u8 messageSize,
    u32 *userID
)
{
    s8 *cmt;

    s8 *quote1;

    s8 *quote2;

    s8 *headerEnd;

    s8 *msgStart;

    s8 *msgEnd;

    u32 len;

    u32 id;


    /*======================================================
                        FIND +CMT:
      ======================================================*/

    cmt = strstr(buffer, "+CMT:");

    if(cmt == 0)
    {
        return GSM_SMS_NONE;
    }


    /*======================================================
                    FIND SENDER
      ======================================================*/

    quote1 = strchr(cmt, '"');

    if(quote1 == 0)
    {
        return GSM_SMS_NONE;
    }


    quote1++;


    quote2 = strchr(quote1, '"');

    if(quote2 == 0)
    {
        return GSM_SMS_NONE;
    }


    /*======================================================
                    COPY SENDER
      ======================================================*/

    len = quote2 - quote1;

    if(len >= senderSize)
    {
        len = senderSize - 1;
    }


    memcpy(sender, quote1, len);

    sender[len] = '\0';


    /*======================================================
                    FIND HEADER END
      ======================================================*/

    headerEnd = strstr(quote2, "\r\n");

    if(headerEnd == 0)
    {
        /*
           Header has not completely arrived yet.
        */

        return GSM_SMS_NONE;
    }


    /*======================================================
                    MESSAGE START
      ======================================================*/

    msgStart = headerEnd + 2;


    /*
       Ignore an empty message body temporarily.
    */

    if(*msgStart == '\0')
    {
        return GSM_SMS_NONE;
    }


    /*======================================================
                    FIND COMPLETE MESSAGE END
      ======================================================*/

    msgEnd = strstr(msgStart, "\r\n");

    if(msgEnd == 0)
    {
        /*
           VERY IMPORTANT:

           Do NOT clear the buffer here.

           More UART bytes may still arrive.

           This is what prevents:

               H

           from being processed before:

               HELLO
        */

        return GSM_SMS_NONE;
    }


    /*======================================================
                    COPY COMPLETE MESSAGE
      ======================================================*/

    len = msgEnd - msgStart;

    if(len >= messageSize)
    {
        len = messageSize - 1;
    }


    memcpy(message, msgStart, len);

    message[len] = '\0';


    /*======================================================
                    REMOVE TRAILING CHARACTERS
      ======================================================*/

    while(len > 0)
    {
        if(message[len - 1] == '\r' ||
           message[len - 1] == '\n' ||
           message[len - 1] == ' ')
        {
            message[len - 1] = '\0';

            len--;
        }
        else
        {
            break;
        }
    }


    /*======================================================
                    CHECK BLOCK
      ======================================================*/

    if(GSM_IsBlockCommand(message, &id))
    {
        *userID = id;

        return GSM_SMS_BLOCK;
    }


    /*======================================================
                    CHECK UNBLOCK
      ======================================================*/

    if(GSM_IsUnblockCommand(message, &id))
    {
        *userID = id;

        return GSM_SMS_UNBLOCK;
    }


    /*
       Complete SMS received, but it is not a BLOCK /
       UNBLOCK command.
    */

    *userID = 0;

    return GSM_SMS_OTHER;
}


/*==========================================================
                    READ SMS
  ==========================================================*/

u8 GSM_ReadSMS(
    s8 *sender,
    u8 senderSize,
    s8 *message,
    u8 messageSize,
    u32 *userID
)
{
    u8 ch;

    u8 result;


    /*======================================================
                    COLLECT ALL AVAILABLE DATA
      ======================================================*/

    while(U0_RxAvailable())
    {
        ch = U0_Rx();


        if(GSM_SMS_Index < GSM_RX_BUFFER_SIZE - 1)
        {
            GSM_SMS_Buffer[GSM_SMS_Index++] = ch;

            GSM_SMS_Buffer[GSM_SMS_Index] = '\0';
        }
        else
        {
            /*
               Buffer overflow protection.
            */

            GSM_SMS_Index = 0;

            GSM_SMS_Buffer[0] = '\0';
        }
    }


    /*======================================================
                    CHECK +CMT
      ======================================================*/

    if(strstr(GSM_SMS_Buffer, "+CMT:") == 0)
    {
        return GSM_SMS_NONE;
    }


    /*======================================================
                    PARSE SMS
      ======================================================*/

    result = GSM_ParseCMT(
                GSM_SMS_Buffer,
                sender,
                senderSize,
                message,
                messageSize,
                userID
             );


    /*
       IMPORTANT:

       Only clear the buffer after a COMPLETE SMS
       has been successfully identified.

       GSM_SMS_NONE means:

           "not complete yet"

       Therefore the accumulated UART data is preserved.
    */

    if(result != GSM_SMS_NONE)
    {
        GSM_SMS_Index = 0;

        GSM_SMS_Buffer[0] = '\0';
    }


    return result;
}


/*==========================================================
                    SIMPLE SMS CHECK
  ==========================================================*/

u8 GSM_CheckSMS(void)
{
    return GSM_ReadSMS(
                GSM_SMS_Sender,
                sizeof(GSM_SMS_Sender),
                GSM_SMS_Message,
                sizeof(GSM_SMS_Message),
                &GSM_SMS_UserID
           );
}


/*==========================================================
                    RESET SMS RECEIVER
  ==========================================================*/

void GSM_ResetSMSReceiver(void)
{
    GSM_SMS_Index = 0;

    GSM_SMS_Buffer[0] = '\0';

    GSM_SMS_Sender[0] = '\0';

    GSM_SMS_Message[0] = '\0';

    GSM_SMS_UserID = 0;


    GSM_FlushRx();
}
