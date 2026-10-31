#ifndef RTC_H
#define RTC_H

#include "types.h"


/* RTC Initialization */

void RTC_Init(void);


/* RTC Time */

void GetRTCTimeInfo(s32 *hour,
                    s32 *minute,
                    s32 *second);

void SetRTCTimeInfo(u32 hour,
                    u32 minute,
                    u32 second);


/* RTC Date */

void GetRTCDateInfo(s32 *date,
                    s32 *month,
                    s32 *year);

void SetRTCDateInfo(u32 date,
                    u32 month,
                    u32 year);


/* RTC Day */

void GetRTCDay(s32 *day);

void SetRTCDay(u32 day);

#endif
