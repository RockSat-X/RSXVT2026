#include "system.h"
#include "uxart.c"
#include "stepper.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_stepper_uart);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // The stepper driver relies on other timer
    // and UART initializations to be done first.

    STEPPER_partial_reinit();



    ////////////////////////////////////////////////////////////////////////////////



    for (;;)
    {

        #include "DemoStepper_VELOCITIES.meta"
        /* #meta

            import math

            with Meta.enter(f'static const i32 VELOCITIES[] ='):

                for i in range(500):
                    Meta.line(f'''
                        {round(math.sin(math.sin(i / 500 * 2 * math.pi) * i / 125 * 2 * math.pi * 16) * 10_000)},
                    ''')

        */

        static i32 index = 0;

        while (true)
        {

            // If there are multiple motors involved,
            // a new step velocity must be pushed for
            // all of them at the same time.
            // This demo however only has one motor involved,
            // that is the primary motor.

            b32 was_pushed =
                STEPPER_push_velocities
                (
                    &(i32[])
                    {
                        [StepperInstanceHandle_primary] = VELOCITIES[index],
                    }
                );

            if (was_pushed)
            {
                break; // The step velocity was sucessfully queued up.
            }
            else
            {
                // The stepper driver's ring-buffer is full;
                // wait until there's space to queue the next
                // step velocity.
            }

        }

        index += 1;
        index %= countof(VELOCITIES);

    }

}
