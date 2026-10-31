//otp.h 

#ifndef OTP_H
#define OTP_H

#include "types.h"

void OTP_Init(void);

u32 OTP_Generate(void);

u8 OTP_Verify(u32 enteredOTP);

u8 OTP_IsExpired(void);

u32 OTP_GetRemainingSeconds(void);

#endif
