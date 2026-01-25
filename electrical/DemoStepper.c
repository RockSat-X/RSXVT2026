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

        #include "DemoStepper_ANGULAR_VELOCITIES.meta"
        /* #meta

            import math

            with Meta.enter(f'static const f32 ANGULAR_VELOCITIES[] ='):

                for i in range(125):
                    Meta.line(f'''
                        {math.sin(i / 125 * 2 * math.pi) * 55 :.03f}f,
                    ''')

        */

        static i32 index = 0;

        while (true)
        {

            // If there are multiple motors involved,
            // a new angular velocity must be pushed for
            // all of them at the same time.
            // This demo however only has one motor involved,
            // that is the primary motor.

            b32 was_pushed =
                STEPPER_push_angular_velocities
                (
                    &(f32[])
                    {
                        [StepperInstanceHandle_primary] = ANGULAR_VELOCITIES[index],
                    }
                );

            if (was_pushed)
            {
                break; // The angular velocity was sucessfully queued up.
            }
            else
            {
                // The stepper driver's ring-buffer is full;
                // wait until there's space to queue the next
                // angular velocity.
            }

        }

        index += 1;
        index %= countof(ANGULAR_VELOCITIES);

    }

}
