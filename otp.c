//otp.c

#include "types.h"
#include "rtc.h"
#include "otp.h"


/*==============================================================
                        OTP DEFINITIONS
  =============================================================*/

#define OTP_MIN                 100000UL
#define OTP_MAX                 999999UL

#define OTP_VALIDITY_SECONDS    60UL


/*==============================================================
                     GLOBAL OTP VARIABLES
  =============================================================*/

static u32 CurrentOTP = 0;

/*
   Date and time when the current OTP was generated.
*/

static u32 OTP_GeneratedDay = 0;
static u32 OTP_GeneratedTime = 0;


/*==============================================================
                    GET DAYS IN MONTH
  =============================================================*/

static u32 GetDaysInMonth(u32 month, u32 year)
{
    if(month == 2)
    {
        /*
           Leap year check.
        */

        if(((year % 400) == 0) ||
           (((year % 4) == 0) &&
            ((year % 100) != 0)))
        {
            return 29;
        }

        return 28;
    }


    if(month == 4 ||
       month == 6 ||
       month == 9 ||
       month == 11)
    {
        return 30;
    }


    return 31;
}


/*==============================================================
                    GET ABSOLUTE DAY
  =============================================================*/

/*
   Converts RTC date into an increasing day number.

   This allows OTP expiry to work correctly even when
   the 60-second period crosses midnight.
*/

static u32 GetAbsoluteDay(u32 date,
                          u32 month,
                          u32 year)
{
    u32 y;
    u32 m;

    u32 days = 0;


    /*
       Add complete years.
    */

    for(y = 0; y < year; y++)
    {
        if(((y % 400) == 0) ||
           (((y % 4) == 0) &&
            ((y % 100) != 0)))
        {
            days += 366;
        }
        else
        {
            days += 365;
        }
    }


    /*
       Add complete months of current year.
    */

    for(m = 1; m < month; m++)
    {
        days += GetDaysInMonth(m, year);
    }


    /*
       Add completed days of current month.
    */

    days += (date - 1);


    return days;
}


/*==============================================================
                         OTP_INIT
  =============================================================*/

void OTP_Init(void)
{
    CurrentOTP = 0;

    OTP_GeneratedDay = 0;
    OTP_GeneratedTime = 0;
}


/*==============================================================
                       OTP_GENERATE
  =============================================================*/

u32 OTP_Generate(void)
{
    s32 hour;
    s32 minute;
    s32 second;

    s32 date;
    s32 month;
    s32 year;

    u32 timeValue;
    u32 dateValue;
    u32 otp;


    /*
       Read current RTC time.
    */

    GetRTCTimeInfo(&hour,
                   &minute,
                   &second);


    /*
       Read current RTC date.
    */

    GetRTCDateInfo(&date,
                   &month,
                   &year);


    /*
       Convert current time into seconds since midnight.
    */

    timeValue =
          ((u32)hour * 3600UL)
        + ((u32)minute * 60UL)
        + (u32)second;


    /*
       Existing date-based OTP calculation.
    */

    dateValue =
          ((u32)date * 31UL)
        + ((u32)month * 372UL)
        + (u32)year;


    /*
       Generate OTP.
    */

    otp =
          (timeValue * 1009UL)
        + (dateValue * 9176UL);


    /*
       Convert into six-digit range:

           100000 - 999999
    */

    otp =
        (otp % (OTP_MAX - OTP_MIN + 1UL))
        + OTP_MIN;


    /*
       Store current OTP.
    */

    CurrentOTP = otp;


    /*
       Store exact OTP generation date.
    */

    OTP_GeneratedDay =
        GetAbsoluteDay((u32)date,
                       (u32)month,
                       (u32)year);


    /*
       Store exact OTP generation time.
    */

    OTP_GeneratedTime = timeValue;


    return CurrentOTP;
}


/*==============================================================
                 OTP_GET_REMAINING_SECONDS
  =============================================================*/

/*
   Returns:

       60 -> just generated
       59 -> one second elapsed
       ...
       1  -> one second remaining
       0  -> expired
*/

u32 OTP_GetRemainingSeconds(void)
{
    s32 hour;
    s32 minute;
    s32 second;

    s32 date;
    s32 month;
    s32 year;

    u32 currentDay;
    u32 currentTime;

    u32 elapsedSeconds;


    /*
       No OTP generated.
    */

    if(CurrentOTP == 0)
    {
        return 0;
    }


    /*
       Read current RTC time.
    */

    GetRTCTimeInfo(&hour,
                   &minute,
                   &second);


    /*
       Read current RTC date.
    */

    GetRTCDateInfo(&date,
                   &month,
                   &year);


    /*
       Convert current date to absolute day.
    */

    currentDay =
        GetAbsoluteDay((u32)date,
                       (u32)month,
                       (u32)year);


    /*
       Convert current time to seconds since midnight.
    */

    currentTime =
          ((u32)hour * 3600UL)
        + ((u32)minute * 60UL)
        + (u32)second;


    /*
       Calculate elapsed seconds.
    */

    if(currentDay >= OTP_GeneratedDay)
    {
        elapsedSeconds =
            ((currentDay - OTP_GeneratedDay)
             * 86400UL)
            + currentTime;


        if(elapsedSeconds >= OTP_GeneratedTime)
        {
            elapsedSeconds -= OTP_GeneratedTime;
        }
        else
        {
            /*
               RTC moved backward.

               Treat OTP as expired.
            */

            return 0;
        }
    }
    else
    {
        /*
           RTC date moved backward.

           Treat OTP as expired.
        */

        return 0;
    }


    /*
       OTP still valid.
    */

    if(elapsedSeconds < OTP_VALIDITY_SECONDS)
    {
        return OTP_VALIDITY_SECONDS -
               elapsedSeconds;
    }


    /*
       OTP expired.
    */

    return 0;
}


/*==============================================================
                       OTP_IS_EXPIRED
  =============================================================*/

u8 OTP_IsExpired(void)
{
    if(OTP_GetRemainingSeconds() == 0)
    {
        return 1;
    }

    return 0;
}


/*==============================================================
                         OTP_VERIFY
  =============================================================*/

u8 OTP_Verify(u32 enteredOTP)
{
    /*
       First check expiry.

       An expired OTP must never be accepted.
    */

    if(OTP_IsExpired())
    {
        return 0;
    }


    /*
       Check OTP value.
    */

    if(enteredOTP == CurrentOTP)
    {
        return 1;
    }


    return 0;
}
