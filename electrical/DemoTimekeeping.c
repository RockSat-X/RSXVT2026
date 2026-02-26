#include "system.h"
#include "uxart.c"
#include "timekeeping.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize stuff.
    //



    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate timekeeping.
    //



    u32 halfperiod_i     = 0;
    u32 halfperiods_us[] =
        {
            100,
            250,
            500,
            1'000,
            2'500,
            5'000,
            10'000,
            25'000,
            50'000,
            100'000,
            250'000,
            500'000,
            1'000'000,
        };

    for (;;)
    {

        if (stlink_rx(&(u8) {0}))
        {

            halfperiod_i += 1;
            halfperiod_i %= countof(halfperiods_us);

            stlink_tx("Period is now %d us...\n", 2 * halfperiods_us[halfperiod_i]);

        }

        spinlock_us(halfperiods_us[halfperiod_i]);
        GPIO_TOGGLE(led_green);

        spinlock_us(halfperiods_us[halfperiod_i]);
        GPIO_TOGGLE(led_green);

    }

}
