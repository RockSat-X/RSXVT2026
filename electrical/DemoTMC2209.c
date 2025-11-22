#include "system.h"
#include "uxart.c"
#include "timekeeping.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Enable the peripheral.

    CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);



    ////////////////////////////////////////////////////////////////////////////////



    #include "DemoTMC2209_POSITIONS.meta"
    /* #meta

        import math

        with Meta.enter(f'static const i32 POSITIONS[] ='):
            for i in range(4000):
                Meta.line(f'''
                    {round(math.sin(i / 4000 * 2 * math.pi * 2) * 1500)},
                ''')

    */

    i32 index = 0;

    GPIO_LOW(driver_enable);

    i32 desired_position = 0;
    i32 current_position = 0;

    for (;;)
    {
        desired_position  = POSITIONS[index];
        index            += 1;
        index            %= countof(POSITIONS);

        i32 delta       = desired_position - current_position;
        i32 duration_us = 1000;

        if (delta)
        {
            GPIO_SET(driver_direction, delta < 0);

            i32 abs_delta = delta < 0 ? -delta : delta;

            for (i32 i = 0; i < abs_delta; i += 1)
            {
                GPIO_HIGH(driver_step);
                spinlock_us(1);
                GPIO_LOW(driver_step);
                spinlock_us(duration_us / abs_delta - 1);
            }

            current_position = desired_position;
        }
        else
        {
            spinlock_us(duration_us);
        }
    }

}
