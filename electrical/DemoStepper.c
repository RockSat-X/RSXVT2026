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



    // The stepper driver relies on other timer initializations to be done first.

    STEPPER_partial_init(StepperHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////



    GPIO_LOW(driver_enable);

    for (;;)
    {

        #include "DemoStepper_STEPS.meta"
        /* #meta

            import math

            with Meta.enter(f'static const i8 STEPS[] ='):
                for i in range(4000):
                    Meta.line(f'''
                        {round(math.sin(i / 4000 * 2 * math.pi * 16) * 127)},
                    ''')

        */

        static i32 index = 0;

        while (!STEPPER_push_delta(StepperHandle_primary, STEPS[index]));

        index += 1;
        index %= countof(STEPS);



        stlink_tx("\n");

        u32 data = {0};
        STEPPER_read_register(StepperHandle_primary, 0x00, &data);
        stlink_tx("0x%08X\n", data);

        spinlock_nop(8'000'000);

        data |= (1 << 6);
        data ^= (1 << 3);
        STEPPER_write_register(StepperHandle_primary, 0x00, data);
        stlink_tx("0x%08X\n", data);

        spinlock_nop(8'000'000);

        STEPPER_read_register(StepperHandle_primary, 0x00, &data);
        stlink_tx("0x%08X\n", data);

        spinlock_nop(100'000'000);

    }

}
