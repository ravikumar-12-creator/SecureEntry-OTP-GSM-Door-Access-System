#include <LPC21xx.h>

#include "types.h"
#include "switch.h"


#define SWITCH_PIN    3


void Switch_Init(void)
{
    /*
       P0.3 is used as EINT1.

       Pin configuration is handled
       by Init_EINT().
    */
}


u8 Switch_Read(void)
{
    if(IOPIN0 & (1 << SWITCH_PIN))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
