#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

    for (;;)
    {

        GPIO_TOGGLE(led_green);

        u16 measurement_A = GPIO_SPINLOCK_ANALOG_READ(analog_input_A);
        u16 measurement_B = GPIO_SPINLOCK_ANALOG_READ(analog_input_B);

        stlink_tx
        (
            "%6d : %f | %6d : %f\n",
            measurement_A, (f32) measurement_A / (1 << 12),
            measurement_B, (f32) measurement_B / (1 << 12)
        );

        spinlock_nop(10'000'000);

    }

}
