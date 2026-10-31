// user.c

#include <LPC21xx.h>
#include "types.h"
#include "lcd.h"
#include "lcd_defines.h"
#include "kpm.h"
#include "spi_eeprom.h"
#include "kpm_defines.h"
#include "delay.h"
#include "user.h"
#include "otp.h"
#include "rtc.h"
#include "gsm.h"
#include "door.h"
#include "door_cgram.h"
#include <stdio.h>
#include <string.h>

extern volatile u8 edit_request;

/* USER DEFINES */

#define MAX_USERS                   4
#define USER_ID_LENGTH              4
#define PASSWORD_LENGTH             4
#define USER_SLOT_SIZE              0x10
#define USER_ID_OFFSET              0x00
#define USER_PASSWORD_OFFSET        0x04
#define USER_STATUS_OFFSET          0x08
#define USER_FAILED_COUNT_OFFSET    0x0C
#define USER_BLOCK_TIME_OFFSET      0x0D
#define EEPROM_INIT_ADDR            0x0040
#define INIT_MARKER                 0x3A
#define ADMIN_PASSWORD_ADDR         0x0050
#define DEFAULT_ADMIN_PASSWORD      0
#define ADMIN_INIT_ADDR       0x0054
#define ADMIN_INIT_MARKER     0x5B
#define MOBILE_INIT_ADDR        0x0055
#define MOBILE_INIT_MARKER      0x7D
#define MOBILE_BASE_ADDR            0x0060
#define MOBILE_MAX_LENGTH   10
#define MOBILE_SLOT_SIZE    (MOBILE_MAX_LENGTH + 1)
#define USER_ACTIVE                 1
#define USER_INACTIVE               0
#define MAX_PASSWORD_ATTEMPTS       3
#define PASSWORD_BLOCK_TIME         120
#define MAX_OTP_ATTEMPTS            3
#define mobile_1										"9063305755"
#define mobile_2										"7993875279"


/* EEPROM ADDRESS FUNCTIONS */

static void ChangeAdminPassword(void);

static u8 IsRegisteredSender(s8 *sender, u32 userID);

static u16 UserIDAddress(u8 userNumber)
{
    return ((u16)(userNumber - 1) * USER_SLOT_SIZE) + USER_ID_OFFSET;
}

static u16 UserPasswordAddress(u8 userNumber)
{
    return ((u16)(userNumber - 1) * USER_SLOT_SIZE) + USER_PASSWORD_OFFSET;
}

static u16 UserMobileAddress(u8 userNumber)
{
    return MOBILE_BASE_ADDR +
           ((u16)(userNumber - 1) * MOBILE_SLOT_SIZE);
}

static u16 UserStatusAddress(u8 userNumber)
{
    return ((u16)(userNumber - 1) * USER_SLOT_SIZE) + USER_STATUS_OFFSET;
}

static u16 UserFailedCountAddress(u8 userNumber)
{
    return ((u16)(userNumber - 1) * USER_SLOT_SIZE) + USER_FAILED_COUNT_OFFSET;
}

static u16 UserBlockTimeAddress(u8 userNumber)
{
    return ((u16)(userNumber - 1) * USER_SLOT_SIZE) + USER_BLOCK_TIME_OFFSET;
}

/* USER STATUS */

static u8 GetUserStatus(u8 userNumber)
{
    return (u8)ReadU32(UserStatusAddress(userNumber));
}

static void SetUserStatus(u8 userNumber, u8 status)
{
    StoreU32(UserStatusAddress(userNumber), status);
}

/* FAILED PASSWORD COUNT */

static u8 GetFailedCount(u8 userNumber)
{
    u8 count;

    count = ByteRead_25LC512(UserFailedCountAddress(userNumber));

    if(count > MAX_PASSWORD_ATTEMPTS)
    {
        count = 0;
        ByteWrite_25LC512(UserFailedCountAddress(userNumber), 0);
    }

    return count;
}

static void SetFailedCount(u8 userNumber, u8 count)
{
    ByteWrite_25LC512(UserFailedCountAddress(userNumber), count);
}

/* BLOCK EXPIRY */

static u32 GetBlockExpiry(u8 userNumber)
{
    u16 addr;
    u32 expiry;

    addr = UserBlockTimeAddress(userNumber);

    expiry = (u32)ByteRead_25LC512(addr);
    expiry |= (u32)ByteRead_25LC512(addr + 1) << 8;
    expiry |= (u32)ByteRead_25LC512(addr + 2) << 16;

    return expiry;
}

static void SetBlockExpiry(u8 userNumber, u32 expiry)
{
    u16 addr;

    addr = UserBlockTimeAddress(userNumber);

    ByteWrite_25LC512(addr, (u8)(expiry & 0xFF));
    ByteWrite_25LC512(addr + 1, (u8)((expiry >> 8) & 0xFF));
    ByteWrite_25LC512(addr + 2, (u8)((expiry >> 16) & 0xFF));
}

/* GET REMAINING BLOCK TIME */

static u32 GetBlockTime(u8 userNumber)
{
    u32 expiry;
    u32 now;
    u32 remaining;

    s32 hour;
    s32 minute;
    s32 second;

    expiry = GetBlockExpiry(userNumber);

    if(expiry == 0)
        return 0;

    GetRTCTimeInfo(&hour, &minute, &second);

    now = ((u32)hour * 3600) + ((u32)minute * 60) + (u32)second;

    if(expiry > now)
        remaining = expiry - now;
    else if(expiry < now)
        remaining = (86400 - now) + expiry;
    else
        remaining = 0;

    if(remaining == 0)
    {
        SetBlockExpiry(userNumber, 0);
        SetFailedCount(userNumber, 0);
        return 0;
    }

    if(remaining > PASSWORD_BLOCK_TIME)
    {
        SetBlockExpiry(userNumber, 0);
        SetFailedCount(userNumber, 0);
        return 0;
    }

    return remaining;
}

/* START / CLEAR BLOCK */

static void SetBlockTime(u8 userNumber, u16 seconds)
{
    u32 now;
    u32 expiry;

    s32 hour;
    s32 minute;
    s32 second;

    if(seconds == 0)
    {
        SetBlockExpiry(userNumber, 0);
        return;
    }

    GetRTCTimeInfo(&hour, &minute, &second);

    now = ((u32)hour * 3600) + ((u32)minute * 60) + (u32)second;

    expiry = now + seconds;

    if(expiry >= 86400)
        expiry -= 86400;

    SetBlockExpiry(userNumber, expiry);
}

/* STORE U32 */

void StoreU32(u16 address, u32 data)
{
    u8 i;

    for(i = 0; i < 4; i++)
    {
        ByteWrite_25LC512(
            address + i,
            (u8)((data >> (8 * i)) & 0xFF)
        );
    }
}

/* READ U32 */

u32 ReadU32(u16 address)
{
    u32 data = 0;
    u8 i;

    for(i = 0; i < 4; i++)
    {
        data |= ((u32)ByteRead_25LC512(address + i) << (8 * i));
    }

    return data;
}

static void StoreUserMobile(u8 userNumber, s8 *mobile)
{
    u8 i;
    u16 address;

    address = UserMobileAddress(userNumber);

    for(i = 0; i < MOBILE_SLOT_SIZE; i++)
    {
        if(mobile[i] == '\0')
        {
            ByteWrite_25LC512(address + i, 0);
            break;
        }

        ByteWrite_25LC512(address + i, mobile[i]);
    }

    for(i = i + 1; i < MOBILE_SLOT_SIZE; i++)
    {
        ByteWrite_25LC512(address + i, 0);
    }
}

static void ReadUserMobile(u8 userNumber, s8 *mobile)
{
    u8 i;
    u8 data;
    u16 address;

    address = UserMobileAddress(userNumber);

    for(i = 0; i < MOBILE_MAX_LENGTH; i++)
    {
        data = ByteRead_25LC512(address + i);

        if(data == 0)
            break;

        mobile[i] = data;
    }

    mobile[i] = '\0';
}

void EEPROM_DisplayDiagnosticError(u8 status)
{
    CmdLCD(CLEAR_LCD);

    if(status == EEPROM_ERROR)
    {
        LCD_StringXY(0,0,"EEPROM ERROR");
        LCD_StringXY(1,0,"CHECK CONNECTION");
    }

    delay_s(3);

    //CmdLCD(CLEAR_LCD);

}

/* INITIALIZE USERS */

void InitializeUsers(void)
{
    u8 marker;
		u8 eeprom_status;

		eeprom_status = EEPROM_Diagnose();

		if(eeprom_status != EEPROM_OK)
		{
				EEPROM_DisplayDiagnosticError(eeprom_status);

				return;
		}
    marker = ByteRead_25LC512(EEPROM_INIT_ADDR);

    /* Initialize default users only once */
    if(marker != INIT_MARKER)
    {
        StoreU32(UserIDAddress(1), 1234);
        StoreU32(UserPasswordAddress(1), 1111);
        StoreUserMobile(1, mobile_1);
        SetUserStatus(1, USER_ACTIVE);
        SetFailedCount(1, 0);
        SetBlockTime(1, 0);

        StoreU32(UserIDAddress(2), 5678);
        StoreU32(UserPasswordAddress(2), 2222);
        StoreUserMobile(2, mobile_2);
        SetUserStatus(2, USER_ACTIVE);
        SetFailedCount(2, 0);
        SetBlockTime(2, 0);

        StoreU32(UserIDAddress(3), 0);
        StoreU32(UserPasswordAddress(3), 0);
        StoreUserMobile(3, "");
        SetUserStatus(3, USER_INACTIVE);
        SetFailedCount(3, 0);
        SetBlockTime(3, 0);

        StoreU32(UserIDAddress(4), 0);
        StoreU32(UserPasswordAddress(4), 0);
        StoreUserMobile(4, "");
        SetUserStatus(4, USER_INACTIVE);
        SetFailedCount(4, 0);
        SetBlockTime(4, 0);

        ByteWrite_25LC512(EEPROM_INIT_ADDR, INIT_MARKER);

        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "DEFAULT USERS");
        LCD_StringXY(1, 0, "INITIALIZED");
        delay_s(2);
    }

    /* Initialize mobile numbers separately */
    if(ByteRead_25LC512(MOBILE_INIT_ADDR) != MOBILE_INIT_MARKER)
    {
        StoreUserMobile(1, mobile_1);
        StoreUserMobile(2, mobile_2);
        StoreUserMobile(3, "");
        StoreUserMobile(4, "");

        ByteWrite_25LC512(MOBILE_INIT_ADDR, MOBILE_INIT_MARKER);
    }

    /* Initialize admin password only once */
    if(ByteRead_25LC512(ADMIN_INIT_ADDR) != ADMIN_INIT_MARKER)
    {
        StoreU32(ADMIN_PASSWORD_ADDR, DEFAULT_ADMIN_PASSWORD);

        ByteWrite_25LC512(ADMIN_INIT_ADDR, ADMIN_INIT_MARKER);
    }
}
/* FIND USER */

static u8 FindUserByID(u32 userID)
{
    u8 i;
    u32 storedID;

    for(i = 1; i <= MAX_USERS; i++)
    {
        if(GetUserStatus(i) == USER_ACTIVE)
        {
            storedID = ReadU32(UserIDAddress(i));

            if(storedID == userID)
                return i;
        }
    }

    return 0;
}

/* FIND EMPTY USER SLOT */

static u8 FindEmptyUserSlot(void)
{
    u8 i;

    for(i = 1; i <= MAX_USERS; i++)
    {
        if(GetUserStatus(i) == USER_INACTIVE)
            return i;
    }

    return 0;
}

/* COMPACT USERS */

static void CompactUsers(void)
{
    u8 i;
    u8 j;

    u32 id;
    u32 password;
    u8 failedCount;
    u32 blockTime;

    s8 mobile[MOBILE_SLOT_SIZE];

    for(i = 1; i <= MAX_USERS; i++)
    {
        if(GetUserStatus(i) == USER_INACTIVE)
        {
            for(j = i + 1; j <= MAX_USERS; j++)
            {
                if(GetUserStatus(j) == USER_ACTIVE)
                {
                    id = ReadU32(UserIDAddress(j));
                    password = ReadU32(UserPasswordAddress(j));
                    failedCount = GetFailedCount(j);
                    blockTime = GetBlockTime(j);

                    ReadUserMobile(j, mobile);

                    /* Copy user data to empty slot */
                    StoreU32(UserIDAddress(i), id);
                    StoreU32(UserPasswordAddress(i), password);
                    StoreUserMobile(i, mobile);

                    SetUserStatus(i, USER_ACTIVE);
                    SetFailedCount(i, failedCount);

                    if(blockTime > 0)
                        SetBlockTime(i, (u16)blockTime);
                    else
                        SetBlockTime(i, 0);

                    /* Clear old user slot */
                    StoreU32(UserIDAddress(j), 0);
                    StoreU32(UserPasswordAddress(j), 0);
                    StoreUserMobile(j, "");

                    SetUserStatus(j, USER_INACTIVE);
                    SetFailedCount(j, 0);
                    SetBlockTime(j, 0);

                    break;
                }
            }
        }
    }
}

/* ALREADY BLOCKED DISPLAY */

static void ShowUserBlocked(void)
{
    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "USER BLOCKED");
    LCD_StringXY(1, 0, "ACCESS DENIED");
    delay_s(3);
    CmdLCD(CLEAR_LCD);
}

static u8 IsValidMobileNumber(s8 *mobile)
{
    u8 i;
    u8 firstDigit;
    u8 allSame;

    /*
     * Must contain exactly 10 digits.
     */

    for(i = 0; i < 10; i++)
    {
        if(mobile[i] < '0' || mobile[i] > '9')
            return 0;
    }

    /*
     * Character after 10 digits must be NULL.
     */

    if(mobile[10] != '\0')
        return 0;

    /*
     * Indian mobile numbers should start
     * with 6, 7, 8 or 9.
     */

    firstDigit = mobile[0];

    if(firstDigit != '6' &&
       firstDigit != '7' &&
       firstDigit != '8' &&
       firstDigit != '9')
    {
        return 0;
    }

    /*
     * Reject numbers such as:
     *
     * 1111111111
     * 2222222222
     * ...
     * 9999999999
     */

    allSame = 1;

    for(i = 1; i < 10; i++)
    {
        if(mobile[i] != mobile[0])
        {
            allSame = 0;
            break;
        }
    }

    if(allSame)
        return 0;

    return 1;
}

static void MakeGSMNumber(s8 *mobile10, s8 *gsmNumber)
{
    u8 i;

    gsmNumber[0] = '+';
    gsmNumber[1] = '9';
    gsmNumber[2] = '1';

    for(i = 0; i < 10; i++)
    {
        gsmNumber[i + 3] = mobile10[i];
    }

    gsmNumber[13] = '\0';
}

/* SEND OTP */

static u8 SendOTPMessage(u32 userID, u32 otp)
{
    u8 userNumber;
		u8 gsm_status;
	
    s8 mobile[MOBILE_SLOT_SIZE];
		s8 gsmMobile[16];
    s8 message[256];
	
		s32 hour, minute, seconds;
    s32 date, month, year;

    userNumber = FindUserByID(userID);

    if(userNumber == 0)
        return 0;

    ReadUserMobile(userNumber, mobile);

    /* Mobile number must be valid 10-digit number */

		if(IsValidMobileNumber(mobile) == 0)
						return 0;

		/*
		 * Convert:
		 *
		 * 9063305755
		 *
		 * into:
		 *
		 * +919063305755
		 */

		MakeGSMNumber(mobile, gsmMobile);
		
		/* Get current RTC date and time */
    GetRTCTimeInfo(&hour, &minute, &seconds);
    GetRTCDateInfo(&date, &month, &year);

		
		CmdLCD(CLEAR_LCD);
		LCD_StringXY(0, 0, "SMS NUMBER:");
		LCD_StringXY(1, 0, gsmMobile);
		delay_s(2);

    sprintf(
    (char *)message,
    "SecureEntry: OTP:%06lu FOR USER ID:%lu TIME:%02lu-%02lu-%04lu %02lu:%02lu VALID:60SEC. IF NOT YOU, REPLY BLOCK %lu / UNBLOCK %lu.Please Do Not Share OTP with Anyone",
    (unsigned long)otp,
    (unsigned long)userID,
    (unsigned long)date,
    (unsigned long)month,
    (unsigned long)year,
    (unsigned long)hour,
    (unsigned long)minute,
    (unsigned long)userID,
    (unsigned long)userID
		);

		gsm_status = GSM_Diagnose();

		if(gsm_status != GSM_DIAG_OK)
		{
				GSM_DisplayDiagnosticError(gsm_status);

				return 0;
		}
		
    if(GSM_SendSMS(mobile, message))
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "OTP SENT");
        LCD_StringXY(1, 0, "CHECK MOBILE");
        delay_s(2);

        return 1;
    }

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "SMS FAILED");
    LCD_StringXY(1, 0, "TRY AGAIN");
    delay_s(2);

    return 0;
}

static u8 IsRegisteredSender(s8 *sender, u32 userID)
{
    u8 userNumber;
    s8 registeredMobile[MOBILE_SLOT_SIZE];
    s8 gsmMobile[16];

    userNumber = FindUserByID(userID);

    if(userNumber == 0)
        return 0;

    ReadUserMobile(userNumber, registeredMobile);

    if(IsValidMobileNumber(registeredMobile) == 0)
        return 0;

    MakeGSMNumber(registeredMobile, gsmMobile);

    if(strcmp(sender, gsmMobile) == 0)
        return 1;

    return 0;
}

/* =========================================================
                     SECURITY BUFFER
   ========================================================= */

u8 SecurityBuffer(u32 loggedInUserID)
{
    s8 sender[32];
    s8 message[100];

    u32 smsUserID;
    u32 startTotalSeconds;
    u32 currentTotalSeconds;
    u32 elapsed;
    u32 remaining;

    s32 hour;
    s32 minute;
    s32 second;

    u8 result;
    u8 lastSecond;
    u8 blockReceived;
    u8 loggedUserNumber;

    loggedUserNumber = FindUserByID(loggedInUserID);

    blockReceived = 0;

    GetRTCTimeInfo(&hour, &minute, &second);

    startTotalSeconds =
        ((u32)hour * 3600) +
        ((u32)minute * 60) +
        (u32)second;

    lastSecond = 255;

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "SECURITY BUFFER");
    LCD_StringXY(1, 0, "TIME LEFT: 60s");

    while(1)
    {
        /*
         * IMPORTANT:
         * GSM_ReadSMS() now uses the 5-argument interface:
         *
         * sender
         * sender buffer size
         * message
         * message buffer size
         * SMS user ID
         */

        result = GSM_ReadSMS(
            sender,
            sizeof(sender),
            message,
            sizeof(message),
            &smsUserID
        );

        if(result == GSM_SMS_BLOCK)
        {
            if(IsRegisteredSender(sender, loggedInUserID) &&
											smsUserID == loggedInUserID)
            {
                blockReceived = 1;

                if(loggedUserNumber != 0)
                {
                    SetFailedCount(
                        loggedUserNumber,
                        MAX_PASSWORD_ATTEMPTS
                    );

                    SetBlockTime(
                        loggedUserNumber,
                        PASSWORD_BLOCK_TIME
                    );
                }

                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "BLOCK RECEIVED");
                LCD_StringXY(1, 0, "WAITING...");
                delay_ms(500);
            }
        }

        if(result == GSM_SMS_UNBLOCK)
        {
            if(IsRegisteredSender(sender, loggedInUserID) &&
										smsUserID == loggedInUserID)
            {
                blockReceived = 0;

                if(loggedUserNumber != 0)
                {
                    SetFailedCount(loggedUserNumber, 0);
                    SetBlockTime(loggedUserNumber, 0);
                }

                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "UNBLOCK RECEIVED");
                LCD_StringXY(1, 0, "WAITING...");
                delay_ms(500);
            }
        }

        GetRTCTimeInfo(&hour, &minute, &second);

        currentTotalSeconds =
            ((u32)hour * 3600) +
            ((u32)minute * 60) +
            (u32)second;

        if(currentTotalSeconds >= startTotalSeconds)
            elapsed = currentTotalSeconds - startTotalSeconds;
        else
            elapsed = (86400 - startTotalSeconds) +
                      currentTotalSeconds;

        if(elapsed >= 60)
            break;

        remaining = 60 - elapsed;

        if(remaining != lastSecond)
        {
            lastSecond = (u8)remaining;

            LCD_GotoXY(1, 11);

            CharLCD((remaining / 10) + '0');
            CharLCD((remaining % 10) + '0');
            CharLCD('s');
        }

        delay_ms(20);
    }

    if(blockReceived)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "ACCESS DENIED");
        LCD_StringXY(1, 0, "DOOR LOCKED");
        delay_s(2);

        return 0;
    }

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "BUFFER COMPLETE");
    LCD_StringXY(1, 0, "ACCESS ALLOWED");
    delay_s(2);

    return 1;
}

/* OTP DEFINES */

#define OTP_INPUT_COMPLETE  1
#define OTP_INPUT_TIMEOUT   0
#define OTP_INPUT_ABORTED   2

#define PASSWORD_INPUT_COMPLETE  1
#define PASSWORD_INPUT_TIMEOUT   0
#define PASSWORD_INPUT_ABORTED   2

#define LOGIN_PASSWORD_TIMEOUT   30

/* READ OTP WITH TIMEOUT */

static u8 ReadOTPWithTimeout(u32 *enteredOTP)
{
    u32 num = 0;
    u32 key;
    u32 remainingSeconds;
    u32 lastDisplayedSecond = 999;

    u8 digits = 0;
    u8 pos = 0;

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "OTP TIME: ");
    LCD_StringXY(1, 0, "OTP:");

    while(1)
    {
        if(edit_request)
            return OTP_INPUT_ABORTED;

        remainingSeconds = OTP_GetRemainingSeconds();

        if(remainingSeconds == 0)
            return OTP_INPUT_TIMEOUT;

        if(remainingSeconds != lastDisplayedSecond)
        {
            lastDisplayedSecond = remainingSeconds;

            LCD_GotoXY(0, 9);

            if(remainingSeconds >= 10)
            {
                CharLCD((remainingSeconds / 10) + '0');
                CharLCD((remainingSeconds % 10) + '0');
            }
            else
            {
                CharLCD('0');
                CharLCD(remainingSeconds + '0');
            }

            CharLCD(' ');
            CharLCD(' ');
        }

        key = keyscan_nb();

        if(key == 0)
            continue;

        if(key >= '0' && key <= '9')
        {
            if(digits < 6)
            {
                num = (num * 10) + (key - '0');

                LCD_CharXY(1, digits + 4, key);

                pos++;
                digits++;
            }
        }
        else if(key == KEY_BACKSPACE)
        {
            if(digits > 0)
            {
                num /= 10;
                digits--;
                pos--;

                LCD_CharXY(1, pos + 4, ' ');
                LCD_GotoXY(1, pos + 4);
            }
        }
        else if(key == KEY_CLEAR)
        {
            num = 0;
            digits = 0;
            pos = 0;

            LCD_CharXY(1, 4, ' ');
            LCD_CharXY(1, 5, ' ');
            LCD_CharXY(1, 6, ' ');
            LCD_CharXY(1, 7, ' ');
            LCD_CharXY(1, 8, ' ');
            LCD_CharXY(1, 9, ' ');

            LCD_GotoXY(1, 4);
        }
        else if(key == KEY_ENTER)
        {
            if(digits == 6)
            {
                *enteredOTP = num;
                return OTP_INPUT_COMPLETE;
            }
        }
    }
}

/* OTP TIMEOUT MENU */

static u8 OTPTimeoutMenu(void)
{
    u32 key;

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "OTP EXPIRED");
    LCD_StringXY(1, 0, "1:RESEND 2:BACK");

    while(1)
    {
        if(edit_request)
            return 0;

        key = keyscan_nb();

        if(key == 0)
            continue;

        if(key == '1')
            return 1;

        if(key == '2')
            return 0;
    }
}

static u8 ReadMobileNumber(s8 *mobile, u8 row, u8 col)
{
    u8 pos;
    u8 key;
    u8 i;

    /*
     * Mobile number:
     *
     * User enters ONLY 10 digits.
     *
     * +91 is NOT entered.
     */

    pos = 0;

    for(i = 0; i < MOBILE_SLOT_SIZE; i++)
        mobile[i] = '\0';

    LCD_GotoXY(row, col);

    while(1)
    {
        if(edit_request)
            return 0;
        
        key = keyscan_nb();

        if(key == 0)
        {
            delay_ms(20);
            continue;
        }

        /*======================================================
                           NUMBER
          ======================================================*/

        if(key >= '0' && key <= '9')
        {
            if(pos < 10)
            {
                mobile[pos] = key;
                pos++;

                mobile[pos] = '\0';

                LCD_CharXY(row, col + pos - 1, key);
            }
            else
            {
                /*
                 * 11th digit attempted.
                 */

                CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "ENTER 10 DIGITS");
                LCD_StringXY(1, 0, "ONLY");

                delay_s(2);

                CmdLCD(CLEAR_LCD);

                LCD_StringXY(0, 0, "MOBILE NUMBER");

                for(i = 0; i < MOBILE_SLOT_SIZE; i++)
                    mobile[i] = '\0';

                pos = 0;

                LCD_GotoXY(row, col);
            }
        }

        /*======================================================
                          BACKSPACE
          ======================================================*/

        else if(key == KEY_BACKSPACE)
        {
            if(pos > 0)
            {
                pos--;

                mobile[pos] = '\0';

                LCD_CharXY(row, col + pos, ' ');
                LCD_GotoXY(row, col + pos);
            }
        }

        /*======================================================
                            CLEAR
          ======================================================*/

        else if(key == KEY_CLEAR)
        {
            for(i = 0; i < MOBILE_SLOT_SIZE; i++)
                mobile[i] = '\0';

            pos = 0;

            for(i = 0; i < 10; i++)
                LCD_CharXY(row, col + i, ' ');

            LCD_GotoXY(row, col);
        }

        /*======================================================
                             ENTER
          ======================================================*/

        else if(key == KEY_ENTER)
        {
            /*
             * Mobile number must contain
             * exactly 10 digits.
             */

            if(pos == 10)
                return 1;

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "ENTER 10 DIGITS");
            LCD_StringXY(1, 0, "MOBILE NUMBER");

            delay_s(2);

            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "MOBILE NUMBER");

            pos = 0;

            for(i = 0; i < MOBILE_SLOT_SIZE; i++)
                mobile[i] = '\0';

            LCD_GotoXY(row, col);
        }
    }
}


/* =========================================================
              READ LOGIN PASSWORD WITH TIMEOUT
   ========================================================= */

static u8 ReadPasswordWithTimeout(u32 *enteredPassword)
{
    u32 num;
    u32 key;

    u32 startTotalSeconds;
    u32 currentTotalSeconds;
    u32 elapsed;
    u32 remainingSeconds;

    u8 digits;
    u8 lastDisplayedSecond;

    s32 hour;
    s32 minute;
    s32 second;

    num = 0;
    digits = 0;
    lastDisplayedSecond = 255;

    /* Get starting RTC time */
    GetRTCTimeInfo(&hour, &minute, &second);

    startTotalSeconds =
        ((u32)hour * 3600) +
        ((u32)minute * 60) +
        (u32)second;

    CmdLCD(CLEAR_LCD);

    LCD_StringXY(0, 0, "PASS TIME:");
    LCD_GotoXY(0, 10);
    CharLCD('3');
    CharLCD('0');
    CharLCD('s');

    LCD_StringXY(1, 0, "PASS:");

    while(1)
    {
        /* EINT0 / edit request */
        if(edit_request)
            return PASSWORD_INPUT_ABORTED;

        /* Get current RTC time */
        GetRTCTimeInfo(&hour, &minute, &second);

        currentTotalSeconds =
            ((u32)hour * 3600) +
            ((u32)minute * 60) +
            (u32)second;

        /* Calculate elapsed time */
        if(currentTotalSeconds >= startTotalSeconds)
        {
            elapsed =
                currentTotalSeconds - startTotalSeconds;
        }
        else
        {
            /* Midnight crossing */
            elapsed =
                (86400 - startTotalSeconds) +
                currentTotalSeconds;
        }

        /* Check timeout */
        if(elapsed >= LOGIN_PASSWORD_TIMEOUT)
        {
            return PASSWORD_INPUT_TIMEOUT;
        }

        remainingSeconds =
            LOGIN_PASSWORD_TIMEOUT - elapsed;

        /* Update LCD once every second */
        if(remainingSeconds != lastDisplayedSecond)
        {
            lastDisplayedSecond =
                (u8)remainingSeconds;

            LCD_GotoXY(0, 10);

            if(remainingSeconds >= 10)
            {
                CharLCD(
                    (remainingSeconds / 10) + '0'
                );

                CharLCD(
                    (remainingSeconds % 10) + '0'
                );
            }
            else
            {
                CharLCD('0');

                CharLCD(
                    remainingSeconds + '0'
                );
            }

            CharLCD('s');
        }

        /* Scan keypad */
        key = keyscan_nb();

        if(key == 0)
        {
            delay_ms(20);
            continue;
        }

        /* ---------------- NUMBER ---------------- */

        if(key >= '0' && key <= '9')
        {
            /*
             * Password is exactly 4 digits.
             */
            if(digits < 4)
            {
                num =
                    (num * 10) +
                    (key - '0');

                LCD_CharXY(
                    1,
                    digits + 5,
                    '*'
                );

                digits++;
            }
        }

        /* ---------------- BACKSPACE ---------------- */

        else if(key == KEY_BACKSPACE)
        {
            if(digits > 0)
            {
                digits--;

                num /= 10;

                LCD_CharXY(
                    1,
                    digits + 5,
                    ' '
                );

                LCD_GotoXY(
                    1,
                    digits + 5
                );
            }
        }

        /* ---------------- CLEAR ---------------- */

        else if(key == KEY_CLEAR)
        {
            num = 0;
            digits = 0;

            LCD_CharXY(1, 5, ' ');
            LCD_CharXY(1, 6, ' ');
            LCD_CharXY(1, 7, ' ');
            LCD_CharXY(1, 8, ' ');

            LCD_GotoXY(1, 5);
        }

        /* ---------------- ENTER ---------------- */

        else if(key == KEY_ENTER)
				{
						if(digits < PASSWORD_LENGTH)
						{
								CmdLCD(CLEAR_LCD);
								LCD_StringXY(0, 0, "ENTER MIN 4");
								LCD_StringXY(1, 0, "CHARACTERS");
								delay_s(2);

								/*
								 * Restart login password entry.
								 */
								return ReadPasswordWithTimeout(enteredPassword);
						}

						if(digits > PASSWORD_LENGTH)
						{
								CmdLCD(CLEAR_LCD);
								LCD_StringXY(0, 0, "ENTER 4 CHAR");
								LCD_StringXY(1, 0, "ONLY");
								delay_s(2);

								/*
								 * Restart login password entry.
								 */
								return ReadPasswordWithTimeout(enteredPassword);
						}

						/*
						 * Exactly 4 digits.
						 */
						*enteredPassword = num;

						return PASSWORD_INPUT_COMPLETE;
			}
    }
}

/*static u32 ReadIDLCDAt(u8 row, u8 start_pos, u8 *digits)
{
    u32 num = 0;
    u32 key;
    u8 pos = start_pos;

    *digits = 0;

    LCD_GotoXY(row,start_pos);

    while(1)
    {
        if(edit_request)
        {
            return num;
        }

        key = keyscan_nb();

        if(key == 0)
        {
            continue;
        }

        if(key >= '0' && key <= '9')
        {
            if(pos < 16)
            {
                num = (num * 10) + (key - '0');

                LCD_CharXY(row,pos,key);

                pos++;
                (*digits)++;
            }
        }

        else if(key == KEY_BACKSPACE)
        {
            if(*digits > 0)
            {
                num = num / 10;

                (*digits)--;
                pos--;

                LCD_CharXY(row,pos,' ');

                LCD_GotoXY(row,pos);
            }
        }

        else if(key == KEY_CLEAR)
        {
            num = 0;
            *digits = 0;
            pos = start_pos;

            while(pos < 16)
            {
                LCD_CharXY(row,pos,' ');
                pos++;
            }

            pos = start_pos;

            LCD_GotoXY(row,pos);
        }

        else if(key == KEY_ENTER)
        {
            break;
        }
    }

    return num;
}*/

/* =========================================================
                    READ USER ID
   ========================================================= */

static u32 ReadUserID(u8 row,
                      u8 start_pos,
                      s8 *line1,
                      s8 *line2)
{
    u32 num;
    u32 key;
    u8 digits;
    u8 pos;
    u8 i;

    while(1)
    {
        num = 0;
        digits = 0;
        pos = start_pos;

        /*--------------------------------------------------
                         DISPLAY ID SCREEN
          --------------------------------------------------*/

        CmdLCD(CLEAR_LCD);

        LCD_StringXY(0, 0, line1);
        LCD_StringXY(1, 0, line2);

        LCD_GotoXY(row, start_pos);

        /*--------------------------------------------------
                         USER ID INPUT
          --------------------------------------------------*/

        while(1)
        {
            if(edit_request)
                return 0;

            key = keyscan_nb();

            if(key == 0)
            {
                delay_ms(20);
                continue;
            }

            /*---------------- NUMBER ----------------*/

            if(key >= '0' && key <= '9')
            {
                /*
                 * Allow 5 digits temporarily so that
                 * we can detect ID greater than 4 digits.
                 */

                if(digits < 5)
                {
                    num = (num * 10) + (key - '0');

                    if(pos < 16)
                        LCD_CharXY(row, pos, key);

                    pos++;
                    digits++;
                }
            }

            /*---------------- BACKSPACE ----------------*/

            else if(key == KEY_BACKSPACE)
            {
                if(digits > 0)
                {
                    digits--;
                    num = num / 10;
                    pos--;

                    LCD_CharXY(row, pos, ' ');
                    LCD_GotoXY(row, pos);
                }
            }

            /*---------------- CLEAR ----------------*/

            else if(key == KEY_CLEAR)
            {
                num = 0;
                digits = 0;
                pos = start_pos;

                for(i = start_pos; i < 16; i++)
                    LCD_CharXY(row, i, ' ');

                LCD_GotoXY(row, start_pos);
            }

            /*---------------- ENTER ----------------*/

            else if(key == KEY_ENTER)
            {
                /*--------------------------------------
                    LESS THAN 4 DIGITS
                  --------------------------------------*/

                if(digits < USER_ID_LENGTH)
                {
                    CmdLCD(CLEAR_LCD);

                    LCD_StringXY(0, 0, "ENTER MIN 4");
                    LCD_StringXY(1, 0, "CHARACTERS");

                    delay_s(2);

                    /*
                     * Outer loop displays the SAME
                     * User ID screen again.
                     */

                    break;
                }

                /*--------------------------------------
                    MORE THAN 4 DIGITS
                  --------------------------------------*/

                if(digits > USER_ID_LENGTH)
                {
                    CmdLCD(CLEAR_LCD);

                    LCD_StringXY(0, 0, "ENTER 4");
                    LCD_StringXY(1, 0, "CHARACTERS ONLY");

                    delay_s(2);

                    /*
                     * Outer loop displays the SAME
                     * User ID screen again.
                     */

                    break;
                }

                /*--------------------------------------
                    EXACTLY 4 DIGITS
                  --------------------------------------*/

                return num;
            }
        }
    }
}

/* =========================================================
                    READ PASSWORD
   ========================================================= */

static u32 ReadUserPassword(u8 row,
                            u8 start_pos,
                            s8 *line1,
                            s8 *line2)
{
    u32 num;
    u32 key;
    u8 digits;
    u8 pos;
    u8 i;

    while(1)
    {
        num = 0;
        digits = 0;
        pos = start_pos;

        /*--------------------------------------------------
                         DISPLAY PASSWORD SCREEN
          --------------------------------------------------*/

        CmdLCD(CLEAR_LCD);

        LCD_StringXY(0, 0, line1);
        LCD_StringXY(1, 0, line2);

        LCD_GotoXY(row, start_pos);

        /*--------------------------------------------------
                         PASSWORD INPUT
          --------------------------------------------------*/

        while(1)
        {
            if(edit_request)
                return 0;

            key = keyscan_nb();

            if(key == 0)
            {
                delay_ms(20);
                continue;
            }

            /*---------------- NUMBER ----------------*/

            if(key >= '0' && key <= '9')
            {
                /*
                 * Allow maximum 5 digits.
                 * 5th digit is enough to detect
                 * password length > 4.
                 */

                if(digits < 5)
                {
                    num = (num * 10) + (key - '0');

                    if(pos < 16)
                        LCD_CharXY(row, pos, '*');

                    pos++;
                    digits++;
                }
            }

            /*---------------- BACKSPACE ----------------*/

            else if(key == KEY_BACKSPACE)
            {
                if(digits > 0)
                {
                    digits--;
                    num = num / 10;
                    pos--;

                    LCD_CharXY(row, pos, ' ');
                    LCD_GotoXY(row, pos);
                }
            }

            /*---------------- CLEAR ----------------*/

            else if(key == KEY_CLEAR)
            {
                num = 0;
                digits = 0;
                pos = start_pos;

                for(i = start_pos; i < 16; i++)
                    LCD_CharXY(row, i, ' ');

                LCD_GotoXY(row, start_pos);
            }

            /*---------------- ENTER ----------------*/

            else if(key == KEY_ENTER)
            {
                /*--------------------------------------
                    LESS THAN 4 DIGITS
                  --------------------------------------*/

                if(digits < PASSWORD_LENGTH)
                {
                    CmdLCD(CLEAR_LCD);

                    LCD_StringXY(0, 0, "ENTER MIN 4");
                    LCD_StringXY(1, 0, "CHARACTERS");

                    delay_s(2);

                    /*
                     * Leave input loop.
                     * Outer loop redraws the SAME
                     * password prompt.
                     */

                    break;
                }

                /*--------------------------------------
                    MORE THAN 4 DIGITS
                  --------------------------------------*/

                if(digits > PASSWORD_LENGTH)
                {
                    CmdLCD(CLEAR_LCD);

                    LCD_StringXY(0, 0, "ENTER 4 CHAR");
                    LCD_StringXY(1, 0, "ONLY");

                    delay_s(2);

                    /*
                     * Leave input loop.
                     * Outer loop redraws the SAME
                     * password prompt.
                     */

                    break;
                }

                /*--------------------------------------
                    EXACTLY 4 DIGITS
                  --------------------------------------*/

                return num;
            }
        }

        /*
         * Invalid length.
         *
         * Go back to the SAME password screen.
         *
         * line1 and line2 are supplied by the caller.
         */

    }
}

/* LOGIN */

void Login(void)
{
    u32 enteredID;
    u32 enteredPassword;
    u32 storedPassword;
    u32 otp;
    u32 enteredOTP;

    u8 userNumber;
    u8 smsResult;
    u8 otpInputResult;
    u8 resend;
    u8 passwordFailedAttempts;
    u8 otpFailedAttempts;
    u8 attemptsLeft;
		u8 passwordInputResult;
		//u8 idDigits;

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ENTER USER ID");
    LCD_StringXY(1, 0, "ID:");*/

    enteredID = ReadUserID(1, 3,
											 "ENTER USER ID",
                       "ID:");
	
		if(EEPROM_Diagnose() == EEPROM_ERROR)
		{
				EEPROM_DisplayDiagnosticError(EEPROM_ERROR);
				return;
		}

    if(edit_request)
        return;
		
		/*if(idDigits == 0)
		{
				CmdLCD(CLEAR_LCD);
				LCD_StringXY(0, 0, "PROVIDE USER ID");
				delay_s(2);
				return;
		}*/

    userNumber = FindUserByID(enteredID);

    if(userNumber == 0)
    {
        CmdLCD(CLEAR_LCD);
				LCD_StringXY(0,0,"USER NOT FOUND");
				delay_s(2);
				return;
    }

    if(GetBlockTime(userNumber) > 0)
    {
        ShowUserBlocked();
        return;
    }

    storedPassword = ReadU32(UserPasswordAddress(userNumber));
    passwordFailedAttempts = GetFailedCount(userNumber);

    while(passwordFailedAttempts < MAX_PASSWORD_ATTEMPTS)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER ");
        CharLCD(userNumber + '0');
        LCD_StringXY(1, 0, "ENTER PASSWORD");
        delay_ms(500);

        //CmdLCD(CLEAR_LCD);
        //LCD_StringXY(0, 0, "ENTER PASSWORD");
        //LCD_StringXY(1, 0, "PASS:");

        passwordInputResult =
							ReadPasswordWithTimeout(&enteredPassword);

				if(passwordInputResult == PASSWORD_INPUT_ABORTED)
											return;

				/* ------------------------------------------------------
												PASSWORD ENTRY TIMEOUT
					 ------------------------------------------------------ */

				if(passwordInputResult == PASSWORD_INPUT_TIMEOUT)
				{
								CmdLCD(CLEAR_LCD);

								LCD_StringXY(0,0,"PASSWORD TIMEOUT");

								LCD_StringXY(1,0,"LOGIN AGAIN");

								delay_s(2);

								return;
				}

				/* ------------------------------------------------------
											VERIFY PASSWORD
					 ------------------------------------------------------ */

				if(enteredPassword == storedPassword)
				{
						SetFailedCount(userNumber, 0);
						SetBlockTime(userNumber, 0);

						break;
				}

        passwordFailedAttempts++;

        SetFailedCount(userNumber, passwordFailedAttempts);

        if(passwordFailedAttempts >= MAX_PASSWORD_ATTEMPTS)
        {
            SetBlockTime(userNumber, PASSWORD_BLOCK_TIME);

            CmdLCD(CLEAR_LCD);
            LCD_StringXY(0, 0, "USER BLOCKED");
            LCD_StringXY(1, 0, "WAIT: 02:00");
            delay_s(3);

            return;
        }

        attemptsLeft =
            MAX_PASSWORD_ATTEMPTS - passwordFailedAttempts;

        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "WRONG PASSWORD");
        LCD_StringXY(1, 0, "LEFT: ");
        CharLCD(attemptsLeft + '0');
        delay_s(2);
    }

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "LOGIN SUCCESS");
    LCD_StringXY(1, 0, "GENERATING OTP");
    delay_s(1);

    otpFailedAttempts = 0;

    otp = OTP_Generate();

    smsResult = SendOTPMessage(enteredID, otp);

    if(smsResult == 0)
        return;

    while(1)
    {
        otpInputResult = ReadOTPWithTimeout(&enteredOTP);

        if(otpInputResult == OTP_INPUT_ABORTED)
            return;

        if(otpInputResult == OTP_INPUT_TIMEOUT)
        {
            resend = OTPTimeoutMenu();

            if(resend == 0)
                return;

            otpFailedAttempts = 0;

            otp = OTP_Generate();

            smsResult = SendOTPMessage(enteredID, otp);

            if(smsResult == 0)
                return;

            continue;
        }

        if(otpInputResult == OTP_INPUT_COMPLETE)
        {
            if(OTP_Verify(enteredOTP))
            {
                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "OTP VERIFIED");
                LCD_StringXY(1, 0, "ACCESS REQUEST");
                delay_s(1);

                break;
            }

            otpFailedAttempts++;

            if(otpFailedAttempts >= MAX_OTP_ATTEMPTS)
            {
                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "OTP FAILED");
                LCD_StringXY(1, 0, "3 TRIES USED");
                delay_s(2);

                return;
            }

            attemptsLeft =
                MAX_OTP_ATTEMPTS - otpFailedAttempts;

            CmdLCD(CLEAR_LCD);
            LCD_StringXY(0, 0, "WRONG OTP");
            LCD_StringXY(1, 0, "LEFT: ");
            CharLCD(attemptsLeft + '0');
            delay_s(2);
        }
    }

    if(SecurityBuffer(enteredID))
    {
        CmdLCD(CLEAR_LCD);
        StrLCD("ACCESS GRANTED");

        CmdLCD(0xC0);
        StrLCD("FOR USER");
			
				delay_ms(1000);
			
				Door_Open_Display();

        Door_AccessSequence();

        //CmdLCD(CLEAR_LCD);
        //StrLCD("DOOR CLOSED");
				Door_Close_Display();
        delay_s(2);
    }
    else
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "ACCESS DENIED");
        LCD_StringXY(1, 0, "DOOR LOCKED");
        delay_s(2);
    }
}

/* AUTHENTICATE ADMIN */

static u8 AuthenticateAdmin(void)
{
    u32 enteredPassword;
    u32 storedAdminPassword;

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ADMIN ACCESS");
    LCD_StringXY(1, 0, "PASSWORD:");*/

    enteredPassword = ReadUserPassword(1, 9,
                                   "ADMIN ACCESS",
                                   "PASSWORD:");

    if(edit_request)
        return 0;

    storedAdminPassword = ReadU32(ADMIN_PASSWORD_ADDR);

    if(enteredPassword == storedAdminPassword)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "ADMIN");
        LCD_StringXY(1, 0, "AUTHENTICATED");
        delay_s(1);

        return 1;
    }

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "WRONG PASSWORD");
    LCD_StringXY(1, 0, "ACCESS DENIED");
    delay_s(2);

    return 0;
}

/* ADD USER */

static void AddUser(void)
{
    u8 userNumber;
    u32 newID;
    u32 newPassword;
		u32 confirmPassword;
    s8 newMobile[MOBILE_SLOT_SIZE];

    userNumber = FindEmptyUserSlot();

    if(userNumber == 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER MEMORY FULL");
        delay_s(2);

        return;
    }

    /* ENTER USER ID */

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ADD USER");
    LCD_StringXY(1, 0, "ID:");*/

    newID = ReadUserID(1, 3,
                   "ADD USER",
                   "ID:");

    if(edit_request)
        return;

    if(FindUserByID(newID) != 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "ID ALREADY EXISTS");
        LCD_StringXY(1, 0, "TRY AGAIN");
        delay_s(2);

        return;
    }

    /* ENTER PASSWORD */
		pass:
    /*CmdLCD(CLEAR_LCD);
		LCD_StringXY(0, 0, "ADD PASSWORD");
		LCD_StringXY(1, 0, "PASS:");*/

		newPassword = ReadUserPassword(1, 5,
                               "ADD PASSWORD",
                               "PASS:");

		if(edit_request)
					return;

		/*CmdLCD(CLEAR_LCD);
		LCD_StringXY(0, 0, "CONFIRM PASSWORD");
		LCD_StringXY(1, 0, "PASS:");*/

		confirmPassword = ReadUserPassword(1, 5,
                                   "CONFIRM PASSWORD",
                                   "PASS:");

		if(edit_request)
					return;

		if(newPassword != confirmPassword)
		{
				CmdLCD(CLEAR_LCD);
				LCD_StringXY(0, 0, "PASSWORD");
				LCD_StringXY(1, 0, "MISMATCH");
				delay_s(2);
				goto pass;
		}

    /* ENTER MOBILE NUMBER */

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "MOBILE NUMBER");
    //LCD_StringXY(1, 0, "+");

    /* ENTER MOBILE NUMBER */

		mobile_again:

		CmdLCD(CLEAR_LCD);
		LCD_StringXY(0, 0, "MOBILE NUMBER");

		if(ReadMobileNumber(newMobile, 1, 0) == 0)
						return;

		if(edit_request)
						return;

		/*----------------------------------------------------------
												VALIDATE MOBILE
			----------------------------------------------------------*/

		if(IsValidMobileNumber(newMobile) == 0)
		{
				CmdLCD(CLEAR_LCD);
				LCD_StringXY(0, 0, "MOBILE NUMBER");
				LCD_StringXY(1, 0, "IS INVALID");

				delay_s(2);

				CmdLCD(CLEAR_LCD);
				LCD_StringXY(0, 0, "ENTER MOBILE");
				LCD_StringXY(1, 0, "NUMBER AGAIN");

				delay_s(2);

				goto mobile_again;
		}

    /* STORE USER */

    StoreU32(UserIDAddress(userNumber), newID);
    StoreU32(UserPasswordAddress(userNumber), newPassword);
    StoreUserMobile(userNumber, newMobile);

    SetUserStatus(userNumber, USER_ACTIVE);
    SetFailedCount(userNumber, 0);
    SetBlockTime(userNumber, 0);

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "USER ");
		CharLCD(userNumber+'0');
		LCD_StringXY(0,7,"ADDED");
    LCD_StringXY(1, 0, "SUCCESSFULLY");
    delay_s(2);
}

/* CHANGE USER ID */

/*static void ChangeUserID(u8 targetUser)
{
    u32 newID;

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "CHANGE USER ID");
    LCD_StringXY(1, 0, "NEW ID:");

    newID = ReadNumLCDAt(1, 7);

    if(edit_request)
        return;

    if(FindUserByID(newID) != 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "ID ALREADY EXISTS");
        delay_s(2);

        return;
    }

    StoreU32(UserIDAddress(targetUser), newID);

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ID UPDATED");
    LCD_StringXY(1, 0, "SUCCESSFULLY");
    delay_s(2);
}*/

/* CHANGE USER PASSWORD */

/*static void ChangeUserPassword(u8 targetUser)
{
    u32 newPassword;

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "CHANGE PASSWORD");
    LCD_StringXY(1, 0, "NEW PASS:");

    newPassword = ReadPasswordLCDAt(1, 9);

    if(edit_request)
        return;

    StoreU32(
        UserPasswordAddress(targetUser),
        newPassword
    );

    SetFailedCount(targetUser, 0);
    SetBlockTime(targetUser, 0);

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "PASSWORD UPDATED");
    LCD_StringXY(1, 0, "SUCCESSFULLY");
    delay_s(2);
}*/

/* MODIFY USER */

static void ModifyUser(void)
{
    u32 targetID;
    u32 enteredPassword;
    u32 storedPassword;
    u8 targetUser;
    u8 key;

    u32 newID;
    u32 newPassword;
		u32 confirmPassword;

    s8 newMobile[MOBILE_SLOT_SIZE];

    /*---------------------------------------------
                FIND TARGET USER
      ---------------------------------------------*/

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "MODIFY USER");
    LCD_StringXY(1, 0, "ID:");*/

    targetID = ReadUserID(1, 3,
                      "MODIFY USER",
                      "ID:");

    if(edit_request)
        return;

    targetUser = FindUserByID(targetID);

    if(targetUser == 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER NOT FOUND");
        delay_s(2);

        return;
    }

    /*---------------------------------------------
                USER PASSWORD AUTHENTICATION
      ---------------------------------------------*/

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ENTER PASSWORD");
    LCD_StringXY(1, 0, "PASS:");*/

    enteredPassword = ReadUserPassword(1, 5,
                                   "ENTER PASSWORD",
                                   "PASS:");

    if(edit_request)
        return;

    storedPassword =
        ReadU32(UserPasswordAddress(targetUser));

    if(enteredPassword != storedPassword)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "WRONG PASSWORD");
        LCD_StringXY(1, 0, "ACCESS DENIED");
        delay_s(2);

        return;
    }

    /*---------------------------------------------
                    MODIFY MENU
      ---------------------------------------------*/

    while(1)
    {
        CmdLCD(CLEAR_LCD);

        LCD_StringXY(0, 0, "1.ID 2.PASS");
        LCD_StringXY(1, 0, "3.MOBILE 4.BACK");

        while(1)
        {
            if(edit_request)
                return;

            key = keyscan_nb();

            if(key == '1' ||
               key == '2' ||
               key == '3' ||
               key == '4')
                break;

            delay_ms(20);
        }

        /*-----------------------------------------
                    CHANGE USER ID
          -----------------------------------------*/

        if(key == '1')
        {
            /*CmdLCD(CLEAR_LCD);
            LCD_StringXY(0, 0, "NEW USER ID");
            LCD_StringXY(1, 0, "ID:");*/

            newID = ReadUserID(1, 3,
                   "NEW USER ID",
                   "ID:");

            if(edit_request)
                return;

            if(newID == 0)
            {
                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "INVALID ID");
                delay_s(2);
                continue;
            }

            if(FindUserByID(newID) != 0 &&
               FindUserByID(newID) != targetUser)
            {
                CmdLCD(CLEAR_LCD);
                LCD_StringXY(0, 0, "ID ALREADY EXISTS");
                delay_s(2);
                continue;
            }

            StoreU32(UserIDAddress(targetUser), newID);

            CmdLCD(CLEAR_LCD);
						LCD_StringXY(0, 0, "USER ");
						CharLCD(targetUser+'0');
            LCD_StringXY(1, 0, "ID UPDATED");
            delay_s(2);
        }

        /*-----------------------------------------
                CHANGE USER PASSWORD
          -----------------------------------------*/

        else if(key == '2')
				{
					pass1:
						/*CmdLCD(CLEAR_LCD);
						LCD_StringXY(0, 0, "NEW PASSWORD");
						LCD_StringXY(1, 0, "PASS:");*/

						newPassword = ReadUserPassword(1, 5,
                               "NEW PASSWORD",
                               "PASS:");

						if(edit_request)
									return;

						/*CmdLCD(CLEAR_LCD);
						LCD_StringXY(0, 0, "CONFIRM PASSWORD");
						LCD_StringXY(1, 0, "PASS:");*/

						confirmPassword = ReadUserPassword(1, 5,
                                   "CONFIRM PASSWORD",
                                   "PASS:");

						if(edit_request)
									return;

						if(newPassword != confirmPassword)
						{
								CmdLCD(CLEAR_LCD);
								LCD_StringXY(0, 0, "PASSWORD");
								LCD_StringXY(1, 0, "MISMATCH");
								delay_s(2);
								goto pass1;
						}

						StoreU32(UserPasswordAddress(targetUser),
											newPassword);

						CmdLCD(CLEAR_LCD);
						LCD_StringXY(0, 0, "USER ");
						CharLCD(targetUser+'0');
						LCD_StringXY(1, 0, "PASS UPDATED");
						delay_s(2);
				}

        /*-----------------------------------------
                CHANGE USER MOBILE NUMBER
          -----------------------------------------*/

				else if(key == '3')
				{
					mobile_modify_again:

					CmdLCD(CLEAR_LCD);
					LCD_StringXY(0, 0, "NEW MOBILE");

					if(ReadMobileNumber(newMobile, 1, 0) == 0)
								return;

					if(edit_request)
								return;

					/*------------------------------------------------------
                     VALIDATE MOBILE
						------------------------------------------------------*/

					if(IsValidMobileNumber(newMobile) == 0)
					{
							CmdLCD(CLEAR_LCD);
							LCD_StringXY(0, 0, "MOBILE NUMBER");
							LCD_StringXY(1, 0, "IS INVALID");

							delay_s(2);

							CmdLCD(CLEAR_LCD);
							LCD_StringXY(0, 0, "ENTER MOBILE");
							LCD_StringXY(1, 0, "NUMBER AGAIN");

							delay_s(2);

							goto mobile_modify_again;
					}	

					/*------------------------------------------------------
																STORE NEW MOBILE
						------------------------------------------------------*/

					StoreUserMobile(targetUser, newMobile);

					CmdLCD(CLEAR_LCD);
					LCD_StringXY(0, 0, "USER ");
					CharLCD(targetUser + '0');
					LCD_StringXY(0, 7, "MOBILE");
					LCD_StringXY(1, 0, "NUMBER UPDATED");

					delay_s(2);
				}

        /*-----------------------------------------
                        BACK
          -----------------------------------------*/

        else if(key == '4')
        {
            return;
        }
    }
}

/* DELETE USER */

static u8 DeleteSelectedUser(void)
{
    u32 targetID;

    u8 targetUser;
    u8 key;

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "DELETE USER");
    LCD_StringXY(1, 0, "ID:");*/

    targetID = ReadUserID(1, 3,
                      "DELETE USER",
                      "ID:");

    if(edit_request)
        return 0;

    targetUser = FindUserByID(targetID);

    if(targetUser == 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER NOT FOUND");
        LCD_StringXY(1, 0, "TRY AGAIN");
        delay_s(2);

        return 0;
    }

		CmdLCD(CLEAR_LCD); 
		LCD_StringXY(0, 0, "DELETE USER "); 
		CharLCD(targetUser + '0');
		LCD_StringXY(1, 0, "CONFIRM DELETE");
		delay_ms(500);
		
		
    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "DELETE USER?");
    LCD_StringXY(1, 0, "1:YES  2:NO");

    while(1)
    {
        if(edit_request)
            return 0;

        key = keyscan_nb();

        if(key == '1' || key == '2')
            break;

        delay_ms(20);
    }

    if(key == '2')
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "DELETE CANCELLED");
        delay_s(2);

        return 0;
    }

    StoreU32(UserIDAddress(targetUser), 0);
		StoreU32(UserPasswordAddress(targetUser), 0);
		StoreUserMobile(targetUser, "");

		SetUserStatus(targetUser, USER_INACTIVE);
		SetFailedCount(targetUser, 0);
		SetBlockTime(targetUser, 0);

		CompactUsers();

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "USER ");
		CharLCD(targetUser + '0');
		LCD_StringXY(0,7,"DELETED");
    LCD_StringXY(1, 0, "SUCCESSFULLY");
    delay_s(2);

    return 0;
}

/* BLOCK / UNBLOCK USER */

static void BlockUnblockUser(void)
{
    u32 targetID;
    
    u8 targetUser;
    u8 key;
    u32 remaining;

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "BLOCK / UNBLOCK");
    LCD_StringXY(1, 0, "ID:");*/

    targetID = ReadUserID(1, 3,
                      "BLOCK/UNBLOCK",
                      "ID:");

    if(edit_request)
        return;

    targetUser = FindUserByID(targetID);

    if(targetUser == 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER NOT FOUND");
        LCD_StringXY(1, 0, "TRY AGAIN");
        delay_s(2);

        return;
    }
		
		
		remaining = GetBlockTime(targetUser);

    if(remaining == 0)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER NOT BLOCKED");
        LCD_StringXY(1, 0, "1:BLOCK 2:BACK");

        while(1)
        {
            if(edit_request)
                return;

            key = keyscan_nb();

            if(key == '1' || key == '2')
                break;

            delay_ms(20);
        }

        if(key == '2')
            return;

        SetFailedCount(
            targetUser,
            MAX_PASSWORD_ATTEMPTS
        );

        SetBlockTime(
            targetUser,
            PASSWORD_BLOCK_TIME
        );

        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "USER ");
				CharLCD(targetUser+'0');
				LCD_StringXY(1,0,"BLOCKED");
        delay_s(2);

        return;
    }

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "USER BLOCKED");
    LCD_StringXY(1, 0, "1:UNBLOCK 2:BACK");

    while(1)
    {
        if(edit_request)
            return;

        key = keyscan_nb();

        if(key == '1' || key == '2')
            break;

        delay_ms(20);
    }

    if(key == '2')
        return;

    SetFailedCount(targetUser, 0);
    SetBlockTime(targetUser, 0);

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "USER ");
		CharLCD(targetUser+'0');
		LCD_StringXY(0,7,"UNBLOCKED");
    LCD_StringXY(1, 0, "LOGIN AVAILABLE");
    delay_s(2);
}

static void ChangeAdminPassword(void)
{
    u32 oldPassword;
    u32 newPassword;
    u32 confirmPassword;
    u32 storedPassword;

    /*------------------------------------------------------
                    ENTER OLD PASSWORD
      ------------------------------------------------------*/

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "CHANGE ADMIN");
    LCD_StringXY(1, 0, "OLD PASS:");*/

    oldPassword = ReadUserPassword(1, 8,
                               "CHANGE ADMIN",
                               "OLDPASS:");

    if(edit_request)
        return;

    storedPassword = ReadU32(ADMIN_PASSWORD_ADDR);

    if(oldPassword != storedPassword)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "WRONG PASSWORD");
        LCD_StringXY(1, 0, "ACCESS DENIED");
        delay_s(2);
        return;
    }

    /*------------------------------------------------------
                    ENTER NEW PASSWORD
      ------------------------------------------------------*/

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "NEW ADMIN PASS");
    LCD_StringXY(1, 0, "PASS:");*/

    newPassword = ReadUserPassword(1, 5,
                               "NEW ADMIN PASS",
                               "PASS:");

    if(edit_request)
        return;

    /*------------------------------------------------------
                    CONFIRM NEW PASSWORD
      ------------------------------------------------------*/

    /*CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "CONFIRM PASSWORD");
    LCD_StringXY(1, 0, "PASS:");*/

    confirmPassword = ReadUserPassword(1, 5,
                                   "CONFIRM PASSWORD",
                                   "PASS:");

    if(edit_request)
        return;

    if(newPassword != confirmPassword)
    {
        CmdLCD(CLEAR_LCD);
        LCD_StringXY(0, 0, "PASSWORD");
        LCD_StringXY(1, 0, "MISMATCH");
        delay_s(2);
        return;
    }

    /*------------------------------------------------------
                    STORE NEW ADMIN PASSWORD
      ------------------------------------------------------*/

    StoreU32(ADMIN_PASSWORD_ADDR, newPassword);

    CmdLCD(CLEAR_LCD);
    LCD_StringXY(0, 0, "ADMIN PASSWORD");
    LCD_StringXY(1, 0, "UPDATED");
    delay_s(2);
}

static void ViewUsers(void)
{
    u8 activeUsers[MAX_USERS];
    u8 userCount;
    u8 remaining;
    u8 i;
    u8 j;
    u8 key;
    u16 menuTime;
    u32 userID;
    s8 buffer[17];

    /* =====================================================
       FIND CURRENT ACTIVE USERS FROM EEPROM
       ===================================================== */

    userCount = 0;

    for(i = 1; i <= MAX_USERS; i++)
    {
        if(GetUserStatus(i) == USER_ACTIVE)
        {
            activeUsers[userCount] = i;
            userCount++;

            if(userCount >= MAX_USERS)
                break;
        }
    }

    /* Remaining user slots */
    remaining = MAX_USERS - userCount;


    /* =====================================================
       REVIEW LOOP
       ===================================================== */

    while(1)
    {
        /* =================================================
           DISPLAY USERS - TWO USERS AT A TIME
           ================================================= */

        j = 0;

        while(j < userCount)
        {
            CmdLCD(CLEAR_LCD);

            /* ---------------- FIRST LINE ---------------- */

            i = activeUsers[j];

            userID = ReadU32(UserIDAddress(i));

            sprintf((char *)buffer,
                    "USER %d - ID:%lu",
                    i,
                    (unsigned long)userID);

            LCD_StringXY(0, 0, buffer);


            /* ---------------- SECOND LINE --------------- */

            if((j + 1) < userCount)
            {
                i = activeUsers[j + 1];

                userID = ReadU32(UserIDAddress(i));

                sprintf((char *)buffer,
											"USER %d - ID:%lu",
                        i,
                        (unsigned long)userID);

                LCD_StringXY(1, 0, buffer);
            }


            /* ---------------------------------------------
               SHOW USER WINDOW FOR 3 SECONDS
               --------------------------------------------- */

            menuTime = 0;

            while(menuTime < 3000)
            {
                if(edit_request)
                    return;

                key = keyscan_nb();

                /*
                   1 = leave View section immediately
                */
                if(key == '1')
                    return;

                delay_ms(20);
                menuTime += 20;
            }

            /* Move to next pair */
            j += 2;
        }


        /* =================================================
           SUMMARY WINDOW
           ================================================= */

        CmdLCD(CLEAR_LCD);

        sprintf((char *)buffer,
                "TOTAL USERS:%d",
                MAX_USERS);

        LCD_StringXY(0, 0, buffer);

        sprintf((char *)buffer,
                "ADDED:%d REM:%d",
                userCount,
                remaining);

        LCD_StringXY(1, 0, buffer);


        /* ---------------------------------------------
           SUMMARY DISPLAY FOR 3 SECONDS
           --------------------------------------------- */

        menuTime = 0;

        while(menuTime < 3000)
        {
            if(edit_request)
                return;

            key = keyscan_nb();

            delay_ms(20);
            menuTime += 20;
        }


        /* =================================================
           LAST WINDOW
           ================================================= */

        while(1)
        {
            CmdLCD(CLEAR_LCD);

            LCD_StringXY(0, 0, "1.REVIEW");
            LCD_StringXY(1, 0, "2.EXIT");

            while(1)
            {
                if(edit_request)
                    return;

                key = keyscan_nb();

                /* REVIEW AGAIN */
                if(key == '1')
                {
                    break;
                }

                /* EXIT VIEW */
                else if(key == '2')
                {
                    return;
                }

                delay_ms(20);
            }

            /*
               If REVIEW was selected, break from the
               last-window loop and restart user display.
            */

            if(key == '1')
                break;
        }

        /*
           Rebuild the active user list before reviewing again.
           This ensures the display is always based on the
           latest EEPROM contents.
        */

        userCount = 0;

        for(i = 1; i <= MAX_USERS; i++)
        {
            if(GetUserStatus(i) == USER_ACTIVE)
            {
                activeUsers[userCount] = i;
                userCount++;

                if(userCount >= MAX_USERS)
                    break;
            }
        }

        remaining = MAX_USERS - userCount;
    }
}

void UserManagement(void)
{
    u32 key;
    u8 window;

    /*--------------------------------------------------
                    ADMIN AUTHENTICATION
      --------------------------------------------------*/

    if(!AuthenticateAdmin())
        return;

    /*--------------------------------------------------
                    START FROM WINDOW 0
      --------------------------------------------------*/

    window = 0;

    while(1)
    {
        /*================================================
                         WINDOW DISPLAY
          ================================================*/

        CmdLCD(CLEAR_LCD);

        if(window == 0)
        {
            /*--------------------------------------------
                         WINDOW 0
              --------------------------------------------*/

            LCD_StringXY(0, 0, "1.ADD  2.DELETE");
            LCD_StringXY(1, 0, "* PREV   # NEXT");
        }
        else if(window == 1)
        {
            /*--------------------------------------------
                         WINDOW 1
              --------------------------------------------*/

            LCD_StringXY(0, 0, "3.BLOCK/UNBLOCK");
            LCD_StringXY(1, 0, "* PREV   # NEXT");
        }
        else if(window == 2)
        {
            /*--------------------------------------------
                         WINDOW 2
              --------------------------------------------*/

            LCD_StringXY(0, 0, "4.MODIFY 5.AD PWD");
            LCD_StringXY(1, 0, "* PREV   # NEXT");
        }
        else if(window == 3)
        {
            /*--------------------------------------------
                         WINDOW 3
              --------------------------------------------*/

            LCD_StringXY(0, 0, "6.VIEW  7.EXIT");
            LCD_StringXY(1, 0, "* PREV   # NEXT");
        }

        /*================================================
                         KEY INPUT
          ================================================*/

        while(1)
        {
            if(edit_request)
                return;

            key = keyscan_nb();

            if(key == 0)
            {
                delay_ms(20);
                continue;
            }

            /*================================================
                         PREVIOUS WINDOW '*'
              ================================================*/

            if(key == KEY_PREVIOUS)
            {
                if(window == 0)
                    window = 3;
                else
                    window--;

                break;
            }

            /*================================================
                         NEXT WINDOW '#'
              ================================================*/

            if(key == KEY_NEXT)
            {
                if(window == 3)
                    window = 0;
                else
                    window++;

                break;
            }

            /*================================================
                         WINDOW 0 OPTIONS
              ================================================*/

            if(window == 0)
            {
                if(key == '1')
                {
                    AddUser();
                    break;
                }
                else if(key == '2')
                {
                    DeleteSelectedUser();
                    break;
                }

                /*
                 * Any other key is ignored.
                 *
                 * 3,4,5,6,7 -> NO ACTION
                 */
            }

            /*================================================
                         WINDOW 1 OPTIONS
              ================================================*/

            else if(window == 1)
            {
                if(key == '3')
                {
                    BlockUnblockUser();
                    break;
                }

                /*
                 * Any other key is ignored.
                 *
                 * 1,2,5,6,7 -> NO ACTION
                 */
            }

            /*================================================
                         WINDOW 2 OPTIONS
              ================================================*/

            else if(window == 2)
            {
								if(key == '4')
                {
                    ModifyUser();
                    break;
                }
                else if(key == '5')
                {
                    ChangeAdminPassword();
                    break;
                }

                /*
                 * Any other key is ignored.
                 *
                 * 1,2,3,4,7 -> NO ACTION
                 */
            }

            /*================================================
                         WINDOW 3 OPTIONS
              ================================================*/

            else if(window == 3)
            {
								if(key == '6')
                {
                    ViewUsers();
                    break;
                }
                else if(key == '7')
                {
                    CmdLCD(CLEAR_LCD);
                    LCD_StringXY(0, 0, "EXITING ADMIN");
                    LCD_StringXY(1, 0, "MODE...");
                    delay_s(1);

                    return;
                }

                /*
                 * Any other key is ignored.
                 *
                 * 1,2,3,4,5,6 -> NO ACTION
                 */
            }
        }
    }
}
