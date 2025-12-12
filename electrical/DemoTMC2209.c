#include "system.h"
#include "uxart.c"
#include "stepper.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    ////////////////////////////////////////////////////////////////////////////////



    _Stepper_partial_init(StepperHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////



    GPIO_LOW(driver_enable);

    for (;;)
    {

        // See if we can queue up the
        // next sequence of steps to take.

        i32 deltas_queued = _Stepper_drivers[StepperHandle_primary].writer - _Stepper_drivers[StepperHandle_primary].reader;

        if (deltas_queued < countof(_Stepper_drivers[StepperHandle_primary].deltas))
        {

            // Compute the steps that'll need to be taken.

            static i32 index = 0;

            #include "DemoTMC2209_STEPS.meta"
            /* #meta

                import math

                with Meta.enter(f'static const i8 STEPS[] ='):
                    for i in range(4000):
                        Meta.line(f'''
                            {round(math.sin(i / 4000 * 2 * math.pi * 16) * 127)},
                        ''')

            */

            i8 steps = STEPS[index];

            index += 1;
            index %= countof(STEPS);



            // Push into the ring-buffer.

            i32 write_index = _Stepper_drivers[StepperHandle_primary].writer % countof(_Stepper_drivers[StepperHandle_primary].deltas);
            _Stepper_drivers[StepperHandle_primary].deltas[write_index]  = steps;
            _Stepper_drivers[StepperHandle_primary].writer              += 1;

        }

    }

}
