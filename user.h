#ifndef USER_H
#define USER_H

#include "types.h"


/*==========================================================
                    USER FUNCTIONS
  ==========================================================*/

void InitializeUsers(void);

void Login(void);

void UserManagement(void);


/*==========================================================
                    SECURITY BUFFER
  ==========================================================*/

u8 SecurityBuffer(u32 loggedInUserID);


/*==========================================================
                    EEPROM FUNCTIONS
  ==========================================================*/

void StoreU32(u16 address, u32 data);

u32 ReadU32(u16 address);

void EEPROM_DisplayDiagnosticError(u8 status);

#endif
