//delay.c
#include "types.h"
void delay_us(u32 tdly)
{
	s32 i,j;
	for(i=0;i<tdly;i++)
	{
		for(j=0;j<12;j++);
	}
	//5 clock cycles
	//12*5=60 clock cycles
}
void delay_ms(u32 tdly)
{
	s32 i,j;
	for(i=0;i<tdly;i++)
	{
		for(j=0;j<12000;j++);
	}
	//12000*5=60000 clock cycles
}
void delay_s(u32 tdly)
{
	s32 i,j;
	for(i=0;i<tdly;i++)
	{
		for(j=0;j<12000000;j++);
	}
	//12000000*5=60000000 clock cycles
}
