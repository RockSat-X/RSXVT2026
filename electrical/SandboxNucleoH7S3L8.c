#include <stm32h7s3xx.h>
#include "defs.h"
#include "SYSTEM_init.meta"
#include "system/gpios.c"
#include "misc.c"
#include <FreeRTOS_Kernel/tasks.c>
#include <FreeRTOS_Kernel/queue.c>
#include <FreeRTOS_Kernel/list.c>
#include <port.c>

extern void
HANDLER_Default(void)
{
    for(;;);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STACK_SIZE_A (400 / sizeof(u32))
StaticTask_t xTaskBuffer_A          = {0};
StackType_t  xStack_A[STACK_SIZE_A] = {0};

static noret void
vTaskCode_A(void*)
{
    for(;;)
    {
        GPIO_TOGGLE(led_green);
        vTaskDelay(10);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STACK_SIZE_B (400 / sizeof(u32))
StaticTask_t xTaskBuffer_B          = {0};
StackType_t  xStack_B[STACK_SIZE_B] = {0};

static noret void
vTaskCode_B(void*)
{
    for(;;)
    {
        GPIO_TOGGLE(led_yellow);
        vTaskDelay(15);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STACK_SIZE_C (400 / sizeof(u32))
StaticTask_t xTaskBuffer_C          = {0};
StackType_t  xStack_C[STACK_SIZE_C] = {0};

static noret void
vTaskCode_C(void*)
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

    TaskHandle_t xHandle_A = xTaskCreateStatic(vTaskCode_A, "This is Task A", STACK_SIZE_A, nullptr, tskIDLE_PRIORITY, xStack_A, &xTaskBuffer_A);
    TaskHandle_t xHandle_B = xTaskCreateStatic(vTaskCode_B, "This is Task B", STACK_SIZE_B, nullptr, tskIDLE_PRIORITY, xStack_B, &xTaskBuffer_B);
    TaskHandle_t xHandle_C = xTaskCreateStatic(vTaskCode_C, "This is Task C", STACK_SIZE_C, nullptr, tskIDLE_PRIORITY, xStack_C, &xTaskBuffer_C);
    vTaskStartScheduler();

    panic;
}
