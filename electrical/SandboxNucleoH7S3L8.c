#include <stm32h7s3xx.h>
#include "defs.h"
#include "misc.c"
#include "SYSTEM_init.meta"

extern void
HANDLER_Default(void)
{
    for(;;);
}

extern void
HANDLER_SysTick(void)
{
    systick_ms += 1;
}

extern noret void
main(void)
{
    SYSTEM_init();

    for(;;);
}
