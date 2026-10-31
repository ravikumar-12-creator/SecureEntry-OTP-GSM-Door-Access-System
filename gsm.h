#ifndef GSM_H
#define GSM_H

#include "types.h"


/*==========================================================
                    SMS RESULT CODES
  ==========================================================*/

#define GSM_SMS_NONE       0
#define GSM_SMS_BLOCK      1
#define GSM_SMS_UNBLOCK    2
#define GSM_SMS_OTHER      3


/*==========================================================
                    GSM DIAGNOSTIC CODES
  ==========================================================*/

#define GSM_DIAG_OK                 0
#define GSM_DIAG_MODULE_FAIL        1
#define GSM_DIAG_SIM_NOT_INSERTED   2
#define GSM_DIAG_SIM_NOT_READY      3
#define GSM_DIAG_NO_NETWORK         4
#define GSM_DIAG_NETWORK_SEARCHING  5
#define GSM_DIAG_NETWORK_DENIED     6
#define GSM_DIAG_SIGNAL_WEAK        7
#define GSM_DIAG_SIGNAL_UNKNOWN     8
#define GSM_DIAG_UNKNOWN_ERROR      9


/*==========================================================
                    GSM INITIALIZATION
  ==========================================================*/

u8 GSM_Init(void);


/*==========================================================
                    GSM COMMANDS
  ==========================================================*/

void GSM_SendCommand(s8 *);

u8 GSM_WaitForResponse(s8 *, u32);

u8 GSM_TestCommand(s8 *);


/*==========================================================
                    SIM / NETWORK
  ==========================================================*/

u8 GSM_CheckSIM(void);

u8 GSM_CheckNetwork(void);


/*==========================================================
                    SMS CONFIGURATION
  ==========================================================*/

u8 GSM_SetSMSMode(void);

u8 GSM_ConfigureSMS(void);


/*==========================================================
                    SEND SMS
  ==========================================================*/

u8 GSM_SendSMS(s8 *, s8 *);


/*==========================================================
                    UART WRAPPERS
  ==========================================================*/

u8 GSM_RxAvailable(void);

u8 GSM_Rx(void);

void GSM_FlushRx(void);


/*==========================================================
                    SMS RECEIVE
  ==========================================================*/

u8 GSM_ReadSMS(
    s8 *,
    u8,
    s8 *,
    u8,
    u32 *
);


/*==========================================================
                    SIMPLE SMS CHECK
  ==========================================================*/

u8 GSM_CheckSMS(void);


/*==========================================================
                    RESET SMS RECEIVER
  ==========================================================*/

void GSM_ResetSMSReceiver(void);


/*==========================================================
                    GSM DIAGNOSTICS
  ==========================================================*/

u8 GSM_Diagnose(void);

void GSM_DisplayDiagnosticError(u8 status);


#endif
