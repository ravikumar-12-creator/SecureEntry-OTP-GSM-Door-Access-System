#include <LPC21xx.h>

#include "types.h"
#include "rtc.h"


/*----------------------------------------------------------
                SYSTEM CLOCK DEFINITIONS
  ----------------------------------------------------------*/

#define FOSC            12000000
#define CCLK            (5 * FOSC)
#define PCLK            (CCLK / 4)


/*----------------------------------------------------------
                    RTC PRESCALER
  ----------------------------------------------------------*/

#define PREINT_VAL      ((int)(PCLK / 32768) - 1)

#define PREFRAC_VAL     (PCLK - \
                        ((PREINT_VAL + 1) * 32768))


/*----------------------------------------------------------
                    RTC CONTROL BITS
  ----------------------------------------------------------*/

#define RTC_ENABLE      (1 << 0)
#define RTC_RESET       (1 << 1)


/*----------------------------------------------------------
                    RTC INITIALIZATION
  ----------------------------------------------------------*/

void RTC_Init(void)
{
    /* Reset RTC */
    CCR = RTC_RESET;


    /* Configure RTC prescaler */

    PREINT = PREINT_VAL;
    PREFRAC = PREFRAC_VAL;


    /* Enable RTC */

    CCR = RTC_ENABLE;
}


/*----------------------------------------------------------
                    GET RTC TIME
  ----------------------------------------------------------*/

void GetRTCTimeInfo(s32 *hour,
                    s32 *minute,
                    s32 *second)
{
    *hour   = HOUR;
    *minute = MIN;
    *second = SEC;
}


/*----------------------------------------------------------
                    SET RTC TIME
  ----------------------------------------------------------*/

void SetRTCTimeInfo(u32 hour,
                    u32 minute,
                    u32 second)
{
    HOUR = hour;
    MIN  = minute;
    SEC  = second;
}


/*----------------------------------------------------------
                    GET RTC DATE
  ----------------------------------------------------------*/

void GetRTCDateInfo(s32 *date,
                    s32 *month,
                    s32 *year)
{
    *date  = DOM;
    *month = MONTH;
    *year  = YEAR;
}


/*----------------------------------------------------------
                    SET RTC DATE
  ----------------------------------------------------------*/

void SetRTCDateInfo(u32 date,
                    u32 month,
                    u32 year)
{
    DOM   = date;
    MONTH = month;
    YEAR  = year;
}


/*----------------------------------------------------------
                    GET RTC DAY
  ----------------------------------------------------------*/

void GetRTCDay(s32 *day)
{
    *day = DOW;
}


/*----------------------------------------------------------
                    SET RTC DAY
  ----------------------------------------------------------*/

void SetRTCDay(u32 day)
{
    DOW = day;
}
