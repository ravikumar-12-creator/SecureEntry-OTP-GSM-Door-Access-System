//uart.c

#include <LPC21xx.h>

#include "types.h"
#include "uart.h"
#include "uart_defines.h"


/*==========================================================
                    RX RING BUFFER
  ==========================================================*/

static volatile u8 U0_RxBuffer[U0_RX_BUFFER_SIZE];

static volatile u16 U0_RxHead = 0;

static volatile u16 U0_RxTail = 0;


/*==========================================================
                    OPTIONAL FLAG
  ==========================================================*/

volatile u8 flag = 0;


/*==========================================================
                    UART0 RX ISR
  ==========================================================*/

void UART0_ISR(void) __irq
{
    u8 ch;
    u16 nextHead;


    /*
       Read all available bytes from UART hardware FIFO.
    */

    while(U0LSR & (1 << DR_BIT))
    {
        ch = U0RBR;


        /*
           Calculate next buffer position.
        */

        nextHead = U0_RxHead + 1;

        if(nextHead >= U0_RX_BUFFER_SIZE)
        {
            nextHead = 0;
        }


        /*
           Store byte only when buffer is not full.
        */

        if(nextHead != U0_RxTail)
        {
            U0_RxBuffer[U0_RxHead] = ch;

            U0_RxHead = nextHead;
        }
    }


    /*
       End of interrupt.
    */

    VICVectAddr = 0;
}


/*==========================================================
                    UART0 INITIALIZATION
  ==========================================================*/

void UART0_Init(void)
{
    u32 divisor;


    /*------------------------------------------------------
                    RESET SOFTWARE BUFFER
      ------------------------------------------------------*/

    U0_RxHead = 0;

    U0_RxTail = 0;


    /*------------------------------------------------------
                    UART0 PIN CONFIGURATION
      ------------------------------------------------------*/

    PINSEL0 &= ~(0x0F);

    PINSEL0 |= TXD0_PIN | RXD0_PIN;


    /*------------------------------------------------------
                    8-BIT, 1 STOP, NO PARITY
      ------------------------------------------------------*/

    U0LCR = WORD_LEN;


    /*------------------------------------------------------
                    ENABLE DLAB
      ------------------------------------------------------*/

    U0LCR |= (1 << DLAB_BIT);


    /*------------------------------------------------------
                    BAUD RATE
      ------------------------------------------------------*/

    divisor = DIVISOR;

    U0DLL = divisor & 0xFF;

    U0DLM = (divisor >> 8) & 0xFF;


    /*------------------------------------------------------
                    DISABLE DLAB
      ------------------------------------------------------*/

    U0LCR &= ~(1 << DLAB_BIT);


    /*------------------------------------------------------
                    ENABLE FIFO
      ------------------------------------------------------*/

    U0FCR = 0x07;


    /*------------------------------------------------------
                    RX INTERRUPT
      ------------------------------------------------------*/

#if (U0_RX_INT_EN > 0)

    /*
       Enable Receiver Data Available interrupt.

       U0IER bit 0 = RBR interrupt enable.
    */

    U0IER = (1 << RDA_INT_EN_BIT);


    /*
       EINT1 already uses VIC slot 0.

       UART0 uses VIC slot 1.
    */

    VICVectAddr1 = (u32)UART0_ISR;

    VICVectCntl1 = (1 << 5) | U0_VIC_CHNO;


    /*
       Enable UART0 interrupt in VIC.
    */

    VICIntEnable = (1 << U0_VIC_CHNO);

#endif
}


/*==========================================================
                    TRANSMIT BYTE
  ==========================================================*/

void U0_Tx(u8 data)
{
    while(!(U0LSR & (1 << THRE_BIT)));

    U0THR = data;
}


/*==========================================================
                    TRANSMIT STRING
  ==========================================================*/

void U0_TxStr(s8 *str)
{
    while(*str)
    {
        U0_Tx(*str);

        str++;
    }
}


/*==========================================================
                    TRANSMIT U32
  ==========================================================*/

void U0_TxU32(u32 num)
{
    u8 digits[10];

    s32 i = 0;


    if(num == 0)
    {
        U0_Tx('0');

        return;
    }


    while(num > 0)
    {
        digits[i++] = (num % 10) + '0';

        num /= 10;
    }


    while(i > 0)
    {
        i--;

        U0_Tx(digits[i]);
    }
}


/*==========================================================
                    TRANSMIT S32
  ==========================================================*/

void U0_TxS32(s32 num)
{
    if(num < 0)
    {
        U0_Tx('-');

        num = -num;
    }

    U0_TxU32((u32)num);
}


/*==========================================================
                    TRANSMIT FLOAT
  ==========================================================*/

void U0_TxF32(f32 num, u32 decimal)
{
    u32 integerPart;
    u32 fractionPart;
    u32 multiplier = 1;
    u32 i;


    if(num < 0)
    {
        U0_Tx('-');

        num = -num;
    }


    integerPart = (u32)num;

    U0_TxU32(integerPart);

    U0_Tx('.');


    for(i = 0; i < decimal; i++)
    {
        multiplier *= 10;
    }


    fractionPart =
        (u32)((num - integerPart) * multiplier);


    if(decimal > 0)
    {
        u32 temp = multiplier / 10;

        while(temp >= 10 && temp > fractionPart)
        {
            U0_Tx('0');

            temp /= 10;
        }
    }


    U0_TxU32(fractionPart);
}


/*==========================================================
                    RX AVAILABLE
  ==========================================================*/

u8 U0_RxAvailable(void)
{
    if(U0_RxHead != U0_RxTail)
    {
        return 1;
    }

    return 0;
}


/*==========================================================
                    RECEIVE BYTE
  ==========================================================*/

u8 U0_Rx(void)
{
    u8 data;


    /*
       Wait until software RX buffer contains data.
    */

    while(U0_RxHead == U0_RxTail);


    data = U0_RxBuffer[U0_RxTail];


    U0_RxTail++;

    if(U0_RxTail >= U0_RX_BUFFER_SIZE)
    {
        U0_RxTail = 0;
    }


    return data;
}


/*==========================================================
                    FLUSH RX
  ==========================================================*/

void U0_FlushRx(void)
{
    /*
       Discard all bytes currently present in the
       software RX buffer.
    */

    U0_RxTail = U0_RxHead;


    /*
       Also clear any byte remaining in hardware FIFO.
    */

    while(U0LSR & (1 << DR_BIT))
    {
        (void)U0RBR;
    }
}


/*==========================================================
                    RECEIVE STRING
  ==========================================================*/

s8 *U0_RxStr(void)
{
    static s8 str[100];

    u8 ch;

    u32 i = 0;


    while(i < 99)
    {
        ch = U0_Rx();


        if(ch == '\r' || ch == '\n')
        {
            break;
        }


        str[i++] = ch;
    }


    str[i] = '\0';


    return str;
}
