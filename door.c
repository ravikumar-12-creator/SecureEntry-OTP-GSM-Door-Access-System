#include <LPC21xx.h>
#include "door.h"
#include "delay.h"

#define MOTOR_IN1 20
#define MOTOR_IN2 21

#define DOOR_MOTOR_TIME 2000
#define DOOR_OPEN_TIME  10000

void Door_Init(void)
{
    PINSEL1 &= ~(0x00000C00);

    IODIR0 |= (1 << MOTOR_IN1);
    IODIR0 |= (1 << MOTOR_IN2);

    Door_Stop();
}

void Door_Stop(void)
{
    IOCLR0 = (1 << MOTOR_IN1);
    IOCLR0 = (1 << MOTOR_IN2);
}

void Door_Open(void)
{
    IOSET0 = (1 << MOTOR_IN1);
    IOCLR0 = (1 << MOTOR_IN2);

    delay_ms(DOOR_MOTOR_TIME);

    Door_Stop();
}

void Door_Close(void)
{
    IOCLR0 = (1 << MOTOR_IN1);
    IOSET0 = (1 << MOTOR_IN2);

    delay_ms(DOOR_MOTOR_TIME);

    Door_Stop();
}

void Door_AccessSequence(void)
{
    /* Open door */
    Door_Open();

    /* Keep door open */
    delay_ms(DOOR_OPEN_TIME);

    /* Close door */
    Door_Close();
}
