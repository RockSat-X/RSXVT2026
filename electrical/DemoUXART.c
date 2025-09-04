#include "system/defs.h"
#include "uxart.c"

extern noret void
main(void)
{

    SYSTEM_init();

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
