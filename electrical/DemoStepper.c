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

    STEPPER_partial_init_all();



    ////////////////////////////////////////////////////////////////////////////////



    for (;;)
    {

        #include "DemoStepper_STEP_FREQUENCIES.meta"
        /* #meta

            import math

            with Meta.enter(f'static const i32 STEP_FREQUENCIES[] ='):

                for frequency in [
                    round(math.sin(i / 10000 * 2 * math.pi) * 500_000)
                    for i in range(10000)
                ]:
                    Meta.line(f'''
                        {frequency},
                    ''')

        */

        static i32 index = 0;

        while (!STEPPER_push_step_frequency(StepperHandle_primary, STEP_FREQUENCIES[index]));

        index += 1;
        index %= countof(STEP_FREQUENCIES);

    }

}
