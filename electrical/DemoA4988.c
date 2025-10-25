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
        GPIO_TOGGLE(driver_step);
        spinlock_nop(1'000'000);
    }
}



////////////////////////////////////////////////////////////////////////////////



// When using an A4988 breakout
// driver module (@/url:`pololu.com/picture/view/0J3360`),
// there should be the following pins available:
//
//     - N_ENABLE.
//     - MS1, MS2, MS3.
//     - N_RESET, N_SLEEP.
//     - STEP, DIR.
//     - VMOT.
//     - 2B, 2A, 1A, 1B.
//     - VDD, GND.
//
// For the bare-minimum test without a stepper motor needed:
//
//     - Have the MCU control STEP and DIR.
//     - N_RESET and N_SLEEP must be tied together.
//     - VMOT can be 5V (this is only possible because
//       there's no stepper motor; otherwise, 8V or higher
//       is needed).
//     - VDD to 3.3V.
//     - GND to GND.
//     - An oscilloscope probe on 2B, 2A, 1A, or 1B.
//
// All other pins can be left floating.
// There should be an observable drive-train on 2B/2A/1A/1B.
