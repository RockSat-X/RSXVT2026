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

    STEPPER_partial_init(StepperHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////



    for (;;)
    {

        #include "DemoStepper_STEPS.meta"
        /* #meta

            import math

            STEPS_PER_REVOLUTION = 1600

            with Meta.enter(f'static const i32 STEPS[] ='):



                # Sequence of complicated movements...

                pattern = [
                    round(math.sin(math.sin(i / 500 * 2 * math.pi) * i / 125 * 2 * math.pi * 16) * 255)
                    for i in range(500)
                ]

                for step in pattern:
                    Meta.line(f'''
                        {step},
                    ''')



                # Home back to original position to see if any steps were lost.

                for i in range(-sum(pattern) % STEPS_PER_REVOLUTION // 100):
                    Meta.line(f'''
                        100,
                    ''')

                for i in range(-sum(pattern) % STEPS_PER_REVOLUTION % 100):
                    Meta.line(f'''
                        1,
                    ''')



                # Pause...

                for i in range(100):
                    Meta.line(f'''
                        0,
                    ''')

        */

        static i32 index = 0;

        while (!STEPPER_push_delta(StepperHandle_primary, STEPS[index]));

        index += 1;
        index %= countof(STEPS);

    }

}
