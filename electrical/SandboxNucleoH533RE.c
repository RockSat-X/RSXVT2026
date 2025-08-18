#include STM32_CMSIS_DEVICE_H
#include "defs.h"
#include "jig.c"



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT_Default
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
        vTaskDelay(100);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{
    SYSTEM_init();
    JIG_init();



    ////////////////////////////////////////////////////////////////



    #if 0 // Stop FreeRTOS.
        for (;;)
        {
            GPIO_TOGGLE(led_green);
            spinlock_nop(100'000'000);
        }
    #endif



    ////////////////////////////////////////////////////////////////



    #include "SandboxNucleoH533RE_tasks.meta"
    /* #meta

        for task_name, stack_size, priority in (
            ('task_a', 400, 'tskIDLE_PRIORITY'),
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
