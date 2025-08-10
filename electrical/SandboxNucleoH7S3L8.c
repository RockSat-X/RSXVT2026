#include STM32_CMSIS_DEVICE_H
#include "defs.h"
#include "system/gpios.c"
#include "misc.c"
#include "jig.c"
#include <deps/FreeRTOS_Kernel/tasks.c>
#include <deps/FreeRTOS_Kernel/queue.c>
#include <deps/FreeRTOS_Kernel/list.c>
#include <port.c>



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT(Default)
{
    panic;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static noret void
task_a(void*)
{
    for(;;)
    {
        GPIO_TOGGLE(led_green);
        vTaskDelay(10);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static noret void
task_b(void*)
{
    for(;;)
    {
        GPIO_TOGGLE(led_yellow);
        vTaskDelay(15);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static noret void
task_c(void*)
{
    for(;;)
    {
        GPIO_TOGGLE(led_red);
        vTaskDelay(20);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern noret void
main(void)
{
    GPIO_init();
    NVIC_init();
    SYSTEM_init();
    JIG_init();



    ////////////////////////////////////////////////////////////////



    #if 1 // Stop FreeRTOS.
        for (;;)
        {
            GPIO_TOGGLE(led_green);
            spinlock_nop(100'000'000);
        }
    #endif



    ////////////////////////////////////////////////////////////////



    #include "tasks.meta"
    /* #meta

        for task_name, stack_size, priority in (
            ('task_a', 400, 'tskIDLE_PRIORITY'),
            ('task_b', 400, 'tskIDLE_PRIORITY'),
            ('task_c', 400, 'tskIDLE_PRIORITY'),
        ):
            Meta.line(f'''
                static StackType_t  {task_name}_stack[({stack_size} / sizeof(u32))] = {{0}};
                static StaticTask_t {task_name}_buffer = {{0}};
                TaskHandle_t        {task_name}_handle =
                    xTaskCreateStatic
                    (
                        {task_name},
                        "{task_name}",
                        countof({task_name}_stack),
                        nullptr,
                        {priority},
                        {task_name}_stack,
                        &{task_name}_buffer
                    );
            ''')

    */

    vTaskStartScheduler();

    panic;
}
