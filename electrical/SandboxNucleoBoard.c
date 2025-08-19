#include "defs.h"
#include "jig.c"



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static volatile b32 blink_green_led_fast = false;



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(button_observer, 400, tskIDLE_PRIORITY)
{
    for (;;)
    {
        static volatile b32 previous_button_state = false;

        b32 current_button_state = GPIO_READ(button);

        if (!previous_button_state && current_button_state)
        {
            blink_green_led_fast = !blink_green_led_fast;
        }

        previous_button_state = current_button_state;
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(green_blinker, 400, tskIDLE_PRIORITY)
{
    for (;;)
    {
        GPIO_TOGGLE(led_green);

        if (blink_green_led_fast)
        {
            vTaskDelay(10);
        }
        else
        {
            vTaskDelay(50);
        }
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



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



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



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

        for (i32 i = 0;; i += 1)
        {
            spinlock_nop(100'000'000);

            GPIO_TOGGLE(led_green);

            JIG_tx("Hello! The name of the current target is: " STRINGIFY(TARGET_NAME) ".\n");
            JIG_tx("We're on iteration %d.\n", i);
        }

    #endif
}
