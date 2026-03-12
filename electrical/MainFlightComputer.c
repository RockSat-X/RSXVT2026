#include "system.h"
#include "timekeeping.c"
#include "uxart.c"



////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(heartbeat, 0)
{
    for (;;)
    {
        GPIO_TOGGLE(led_channel_green);
        FREERTOS_delay_ms(500);
    }
}
