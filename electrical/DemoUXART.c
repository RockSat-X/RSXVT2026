#include "system/defs.h"
#include "uxart.c"

extern noret void
main(void)
{

    SYSTEM_init();

    UXART_init(UXARTHandle_stlink);

    for (i32 iteration = 0;; iteration += 1)
    {

        if (GPIO_READ(button))
        {
            spinlock_nop(100'000'000);
        }
        else
        {
            spinlock_nop(50'000'000);
        }

        GPIO_TOGGLE(led_green);



        UXART_tx(UXARTHandle_stlink, "Hello! The name of the current target is: " STRINGIFY(TARGET_NAME) ".\n");
        UXART_tx(UXARTHandle_stlink, "FreeRTOS is currently disabled.\n");
        UXART_tx(UXARTHandle_stlink, "We're on iteration %d.\n", iteration);



        for (char received_data = {0}; UXART_rx(UXARTHandle_stlink, &received_data);)
        {
            UXART_tx(UXARTHandle_stlink, "Got '%c'!\n", received_data);
        }

    }

}
