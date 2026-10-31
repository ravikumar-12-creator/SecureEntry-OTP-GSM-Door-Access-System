//lcd.h
#include "types.h"
void WriteLCD(u8);
void CmdLCD(u8);
void InitLCD(void);
void CharLCD(u8);
void StrLCD(s8 *);
void U32LCD(u32);
void S32LCD(s32);
void F32LCD(f32,u8);
void BuildCGRAM(u8*,u8);
void LCD_GotoXY(u8,u8);
void LCD_StringXY(u8,u8,s8 *);
void LCD_CharXY(u8,u8,u8);
void BuildCustomChars(void);
void SecureEntry_CGRAM_Init(void);
void ShowClosedDoor(void);
void ShowOpeningDoor(void);
