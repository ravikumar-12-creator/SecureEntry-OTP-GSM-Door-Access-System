//kpm.h
#include "types.h"
void Init_KPM(void);
u32 colscan(void);
u32 rowcheck(void);
u32 colcheck(void);
u32 keyscan(void);
u32 readnum(void);
u32 ReadNumLCD(void);
u32 keyscan_nb(void);
u32 ReadNumLCDAt(u8 row, u8 start_pos);
u32 ReadPasswordLCDAt(u8 row, u8 start_pos);
#define PASSWORD_TIMEOUT    0xFFFFFFFFUL
