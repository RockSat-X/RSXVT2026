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



    // Enable the peripheral.

    CMSIS_SET(RCC, APB2ENR, TIM15EN, true);



    // Channel output in PWM mode so we can generate a pulse.

    CMSIS_SET(TIM15, CCMR1, OC1M, 0b0111);



    // The comparison channel output is inactive
    // when the counter is below this value.

    CMSIS_SET(TIM15, CCR1, CCR1, 1);



    // Enable the comparison channel output.

    CMSIS_SET(TIM15, CCER, CC1E, true);



    // Master enable for the timer's outputs.

    CMSIS_SET(TIM15, BDTR, MOE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIM15, PSC, PSC, STPY_TIM15_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIM15, EGR, UG, true);



    CMSIS_SET
    (
        TIM15, CR1 ,
        URS  , 0b1 , // So that the UG bit doesn't set the update event interrupt.
        OPM  , true, // Timer's counter stops incrementing after an update event.
    );



    ////////////////////////////////////////////////////////////////////////////////



    GPIO_LOW(driver_enable);

    #include "DemoTMC2209_POSITIONS.meta"
    /* #meta

        import math

        with Meta.enter(f'static const i32 POSITIONS[] ='):
            for i in range(4000):
                Meta.line(f'''
                    {round(math.sin(i / 4000 * 2 * math.pi * 16) * 1500)},
                ''')

    */

    i32 index = 0;

    for (;;)
    {

        while (!CMSIS_GET(TIM15, SR, UIF));
        CMSIS_SET(TIM15, SR, UIF, false);

        i32 now = POSITIONS[index];

        index += 1;
        index %= countof(POSITIONS);

        i32 next = POSITIONS[index];

        i32 delta = next - now;

        delta = delta < 0 ? -delta : delta;

        GPIO_SET(driver_direction, next > now);
        CMSIS_SET(TIM15, CCR1, CCR1, delta ?         1   : -1  );
        CMSIS_SET(TIM15, RCR , REP , delta ? delta - 1 : 0     );
        CMSIS_SET(TIM15, ARR , ARR , (10'000 - 1) / (delta + 1));
        CMSIS_SET(TIM15, EGR , UG  , true                      );
        CMSIS_SET(TIM15, CR1 , CEN , true                      );

    }

}
