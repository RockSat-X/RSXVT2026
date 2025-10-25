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
