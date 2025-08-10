#include STM32_CMSIS_DEVICE_H
#include "defs.h"
#include "system/gpios.c"
#include "misc.c"
// TODO: #include "jig.c"
#include <deps/FreeRTOS_Kernel/tasks.c>
#include <deps/FreeRTOS_Kernel/queue.c>
#include <deps/FreeRTOS_Kernel/list.c>
#include <port.c>
#include <portasm.c>



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT(Default)
{
    panic;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{
    GPIO_init();
    NVIC_init();
    SYSTEM_init();
    // TODO: JIG_init();

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        delay_nop(10'000'000);
    }

    panic;
}
