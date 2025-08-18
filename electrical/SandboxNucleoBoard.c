#include "defs.h"
#include "jig.c"



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT_Default
{
    panic;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(green_blinker, 400, tskIDLE_PRIORITY)
{
    for (;;)
    {
        GPIO_TOGGLE(led_green);
        vTaskDelay(100);
    }
}



FREERTOS_TASK(red_blinker, 400, tskIDLE_PRIORITY)
{
    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        for (;;)
        {
            GPIO_TOGGLE(led_red);
            vTaskDelay(50);
        }

    #endif

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;);

    #endif
}



FREERTOS_TASK(yellow_blinker, 400, tskIDLE_PRIORITY)
{
    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        for (;;)
        {
            GPIO_TOGGLE(led_yellow);
            vTaskDelay(10);
        }

    #endif

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;);

    #endif
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{
    SYSTEM_init();
    JIG_init();

    #if TARGET_USES_FREERTOS

        FREERTOS_init();

    #else

        for (;;)
        {
            GPIO_TOGGLE(led_green);
            spinlock_nop(100'000'000);
        }

    #endif
}
