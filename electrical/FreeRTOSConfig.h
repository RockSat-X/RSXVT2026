#define configCPU_CLOCK_HZ                    STPY_CPU_CK
#define configTICK_RATE_HZ                    100

#define configENABLE_TRUSTZONE                false
#define configENABLE_FPU                      true
#define configENABLE_MPU                      false // TODO Try out.
#define configCHECK_FOR_STACK_OVERFLOW        false // TODO Try out.
#define configUSE_PREEMPTION                  true
#define configUSE_MUTEXES                     true
#define configUSE_IDLE_HOOK                   false
#define configUSE_TICK_HOOK                   false
#define configSUPPORT_DYNAMIC_ALLOCATION      false
#define configSUPPORT_STATIC_ALLOCATION       true
#define configKERNEL_PROVIDED_STATIC_MEMORY   true

#define INCLUDE_vTaskDelay                    true
#define INCLUDE_xTaskGetSchedulerState        true

#define configTICK_TYPE_WIDTH_IN_BITS         TICK_TYPE_WIDTH_32_BITS
#define configMAX_TASK_NAME_LEN               (15 + 1) // +1 for NUL.
#define configMINIMAL_STACK_SIZE              (256 / sizeof(StackType_t))
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1



#define configASSERT(CONDITION) \
    do                          \
    {                           \
        if (!(CONDITION))       \
        {                       \
            panic;              \
        }                       \
    }                           \
    while (false)



// This setting just tells FreeRTOS how many
// task priorities it should allocate; the
// bigger the number, the more granularity a
// particular task has to indicate its priority.
// This option is irrelevant of interrupt priorities.

#define configMAX_PRIORITIES 8



// This setting splits the range of interrupt priorities
// at which any FreeRTOS API function calls can be made.
// e.g:
// >
// >               __NVIC_PRIO_BITS = 4
// >               ~~~~~~~~~~~~~~~~~~~~
// >    Niceness      |
// >          vv     ~~~~
// >          15 : 0b1111'xxxx \
// >          14 : 0b1110'xxxx |--- These interrupt priorities are nicer.
// >          13 : 0b1101'xxxx |    Any interrupt routine can execute
// >          12 : 0b1100'xxxx |    FreeRTOS API functions from here.
// >          11 : 0b1011'xxxx |
// >          10 : 0b1010'xxxx / <- configMAX_SYSCALL_INTERRUPT_PRIORITY = 0b1010'xxxx
// >           9 : 0b1001'xxxx \
// >           8 : 0b1000'xxxx |
// >           7 : 0b0111'xxxx |
// >           6 : 0b0110'xxxx |
// >           5 : 0b0101'xxxx |--- These interrupt priorities are more greedy.
// >           4 : 0b0100'xxxx |    These interrupts should not execute any
// >           3 : 0b0011'xxxx |    FreeRTOS API functions from here.
// >           2 : 0b0010'xxxx |
// >           1 : 0b0001'xxxx |
// >           0 : 0b0000'xxxx /
// >

#define configMAX_SYSCALL_INTERRUPT_PRIORITY (10 << (8 - __NVIC_PRIO_BITS))
static_assert(configMAX_SYSCALL_INTERRUPT_PRIORITY <= (u8) -1);



////////////////////////////////////////////////////////////////////////////////



// This file contains settings for FreeRTOS.
// Most of it is low-level stuff that you likely will never have to change.
// Nonetheless, it's good to understand what each configuration does,
// because if you can do that, you probably also understand FreeRTOS too.
//
// @/url:`https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/02-Customization`.
