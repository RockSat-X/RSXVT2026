#include <stm32h7s3xx.h>
#include "defs.h"
#include "SYSTEM_init.meta"
#include "system/gpios.c"
#include "misc.c"
#include <FreeRTOS_Kernel/tasks.c>
#include <FreeRTOS_Kernel/queue.c>
#include <FreeRTOS_Kernel/list.c>
#include <port.c>



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



extern void
HANDLER_Default(void)
{
    for(;;);
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
    SYSTEM_init();
    GPIO_init();

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
