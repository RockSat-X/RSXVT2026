#include "defs.h"
#include "jig.c"

extern noret void
main(void)
{
    SYSTEM_init();
    JIG_init();

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000);
    }
}
