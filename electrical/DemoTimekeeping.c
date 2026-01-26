#include "system.h"
#include "uxart.c"
#include "timekeeping.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Miscellaneous initialization.
    //

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // As of right now, the timekeeping driver does not provide its own
    // initialization routine. This is mainly because its initialization
    // is closely connected to other timers (e.g. TIMPRE), so to make
    // things perfectly clear, the steps to initializing timekeeping is
    // spelled out rather than abstracted away. This design decision can
    // be changed later if need be.
    //



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Enable the peripheral.

    CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



    // Trigger an update event so that the shadow registers
    // are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate timekeeping.
    //

    i32 halfperiod_i     = 0;
    i32 halfperiods_us[] =
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

            stlink_tx("Period is now %d us...\n", halfperiods_us[halfperiod_i]);

        }

        spinlock_us(halfperiods_us[halfperiod_i]);
        GPIO_TOGGLE(led_green);

        spinlock_us(halfperiods_us[halfperiod_i]);
        GPIO_TOGGLE(led_green);

    }

}
