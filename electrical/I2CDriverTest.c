#include "defs.h"
#include "jig.c"
#include "i2c.c"

extern noret void
main(void)
{
    SYSTEM_init();
    JIG_init();
    I2C_reinit();

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000);
    }
}
