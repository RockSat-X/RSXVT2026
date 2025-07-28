#include <stm32h7s3xx.h>
#include <FreeRTOS_Kernel/tasks.c>
#include <FreeRTOS_Kernel/queue.c>
#include <FreeRTOS_Kernel/list.c>
#include <FreeRTOS_Kernel/portable/MemMang/heap_1.c>
#include <port.c>
#include "defs.h"
#include "misc.c"
#include "SYSTEM_init.meta"
#include "system/gpios.c"

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
    GPIO_init();

    for(;;);
}
