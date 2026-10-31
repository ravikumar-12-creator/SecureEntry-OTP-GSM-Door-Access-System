#ifndef UART_DEFINES_H
#define UART_DEFINES_H


/*==========================================================
                    UART0 PINS
  ==========================================================*/

#define TXD0_PIN        0x00000001
#define RXD0_PIN        0x00000004


/*==========================================================
                    CLOCK SETTINGS
  ==========================================================*/

#define FOSC            12000000
#define CCLK            (FOSC * 5)
#define PCLK            (CCLK / 4)

#define BAUD            9600

#define DIVISOR         (PCLK / (16 * BAUD))


/*==========================================================
                    UART SETTINGS
  ==========================================================*/

#define _8BIT           3
#define WORD_LEN        _8BIT

#define DLAB_BIT        7

#define DR_BIT          0
#define THRE_BIT        5
#define TEMT_BIT        6


/*==========================================================
                    UART INTERRUPTS
  ==========================================================*/

/*
   RX interrupt is enabled because GSM SMS reception
   is asynchronous.
*/

#define U0_RX_INT_EN        1

/*
   TX interrupt is not required.
   Transmission continues using polling.
*/

#define U0_TX_INT_EN        0
#define U0_TX_RX_INT_EN     0


#define RDA_INT_EN_BIT      0
#define THRE_INT_EN_BIT     1

#define THRE_INT            1
#define RDA_INT             2


/*==========================================================
                    STATUS LED PINS
  ==========================================================*/

#define U0_TX_STATUS_LED    8
#define U0_RX_STATUS_LED    9


/*==========================================================
                    VIC SETTINGS
  ==========================================================*/

/*
   LPC2148:

   UART0 -> VIC Channel 6
   EINT1 -> VIC Channel 15

   EINT1 already uses VIC Vector Slot 0.

   Therefore UART0 uses VIC Vector Slot 1.
*/

#define U0_VIC_CHNO         6
#define U0_VIC_SLOT         1


/*==========================================================
                    SOFTWARE RX BUFFER
  ==========================================================*/

#define U0_RX_BUFFER_SIZE   512


#endif
