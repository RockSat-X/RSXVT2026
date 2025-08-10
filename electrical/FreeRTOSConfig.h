#if TARGET_NAME_IS_SandboxNucleoH7S3L8
    #define configCPU_CLOCK_HZ                                     SYSTEM_CPU_CK_FREQ
    #define configTICK_RATE_HZ                                     100
    #define configUSE_PREEMPTION                                   1
    #define configUSE_TIME_SLICING                                 0
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION                0
    #define configUSE_TICKLESS_IDLE                                0
    #define configMAX_PRIORITIES                                   5
    #define configMINIMAL_STACK_SIZE                               128
    #define configMAX_TASK_NAME_LEN                                16
    #define configTICK_TYPE_WIDTH_IN_BITS                          TICK_TYPE_WIDTH_32_BITS
    #define configIDLE_SHOULD_YIELD                                1
    #define configTASK_NOTIFICATION_ARRAY_ENTRIES                  1
    #define configQUEUE_REGISTRY_SIZE                              0
    #define configENABLE_BACKWARD_COMPATIBILITY                    0
    #define configNUM_THREAD_LOCAL_STORAGE_POINTERS                0
    #define configUSE_MINI_LIST_ITEM                               1
    #define configSTACK_DEPTH_TYPE                                 size_t
    #define configMESSAGE_BUFFER_LENGTH_TYPE                       size_t
    #define configHEAP_CLEAR_MEMORY_ON_FREE                        1
    #define configSTATS_BUFFER_MAX_LENGTH                          0xFFFF
    #define configUSE_TIMERS                                       0
    #define configUSE_EVENT_GROUPS                                 1
    #define configUSE_STREAM_BUFFERS                               1
    #define configSUPPORT_STATIC_ALLOCATION                        1
    #define configSUPPORT_DYNAMIC_ALLOCATION                       0
    #define configTOTAL_HEAP_SIZE                                  4096
    #define configAPPLICATION_ALLOCATED_HEAP                       0
    #define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP              0
    #define configENABLE_HEAP_PROTECTOR                            0
    #define configKERNEL_INTERRUPT_PRIORITY                        0
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY                   0b11110000 // TODO Look into this more.
    #define configUSE_IDLE_HOOK                                    0
    #define configUSE_TICK_HOOK                                    0
    #define configUSE_MALLOC_FAILED_HOOK                           0
    #define configUSE_DAEMON_TASK_STARTUP_HOOK                     0
    #define configUSE_SB_COMPLETED_CALLBACK                        0
    #define configCHECK_FOR_STACK_OVERFLOW                         0
    #define configGENERATE_RUN_TIME_STATS                          0
    #define configUSE_TRACE_FACILITY                               0
    #define configUSE_STATS_FORMATTING_FUNCTIONS                   0
    #define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
    #define configTOTAL_MPU_REGIONS                                8
    #define configTEX_S_C_B_FLASH                                  0x07UL
    #define configTEX_S_C_B_SRAM                                   0x07UL
    #define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY            1
    #define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS             0
    #define configUSE_MPU_WRAPPERS_V1                              0
    #define configPROTECTED_KERNEL_OBJECT_POOL_SIZE                10
    #define configSYSTEM_CALL_STACK_SIZE                           128
    #define configENABLE_ACCESS_CONTROL_LIST                       1
    #define configCHECK_HANDLER_INSTALLATION                       1
    #define configUSE_TASK_NOTIFICATIONS                           1
    #define configUSE_MUTEXES                                      1
    #define configUSE_RECURSIVE_MUTEXES                            1
    #define configUSE_COUNTING_SEMAPHORES                          1
    #define configUSE_POSIX_ERRNO                                  0
    #define INCLUDE_vTaskPrioritySet                               1
    #define INCLUDE_uxTaskPriorityGet                              1
    #define INCLUDE_vTaskDelete                                    1
    #define INCLUDE_vTaskSuspend                                   1
    #define INCLUDE_vTaskDelayUntil                                1
    #define INCLUDE_vTaskDelay                                     1
    #define INCLUDE_xTaskGetSchedulerState                         1
    #define INCLUDE_xTaskGetCurrentTaskHandle                      1
    #define INCLUDE_xTaskResumeFromISR                             1
    #define configKERNEL_PROVIDED_STATIC_MEMORY                    1
#endif

#if TARGET_NAME_IS_SandboxNucleoH533RE
    #define configENABLE_FPU                                       1
    #define configENABLE_MPU                                       0
    #define configENABLE_TRUSTZONE                                 0
    #define configCPU_CLOCK_HZ                                     32'000'000 // TODO: SYSTEM_CPU_CK_FREQ
    #define configTICK_RATE_HZ                                     100
    #define configUSE_PREEMPTION                                   1
    #define configUSE_TIME_SLICING                                 0
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION                0
    #define configUSE_TICKLESS_IDLE                                0
    #define configMAX_PRIORITIES                                   5
    #define configMINIMAL_STACK_SIZE                               128
    #define configMAX_TASK_NAME_LEN                                16
    #define configTICK_TYPE_WIDTH_IN_BITS                          TICK_TYPE_WIDTH_32_BITS
    #define configIDLE_SHOULD_YIELD                                1
    #define configTASK_NOTIFICATION_ARRAY_ENTRIES                  1
    #define configQUEUE_REGISTRY_SIZE                              0
    #define configENABLE_BACKWARD_COMPATIBILITY                    0
    #define configNUM_THREAD_LOCAL_STORAGE_POINTERS                0
    #define configUSE_MINI_LIST_ITEM                               1
    #define configSTACK_DEPTH_TYPE                                 size_t
    #define configMESSAGE_BUFFER_LENGTH_TYPE                       size_t
    #define configHEAP_CLEAR_MEMORY_ON_FREE                        1
    #define configSTATS_BUFFER_MAX_LENGTH                          0xFFFF
    #define configUSE_TIMERS                                       0
    #define configUSE_EVENT_GROUPS                                 1
    #define configUSE_STREAM_BUFFERS                               1
    #define configSUPPORT_STATIC_ALLOCATION                        1
    #define configSUPPORT_DYNAMIC_ALLOCATION                       0
    #define configTOTAL_HEAP_SIZE                                  4096
    #define configAPPLICATION_ALLOCATED_HEAP                       0
    #define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP              0
    #define configENABLE_HEAP_PROTECTOR                            0
    #define configKERNEL_INTERRUPT_PRIORITY                        0
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY                   0b11110000 // TODO Look into this more.
    #define configUSE_IDLE_HOOK                                    0
    #define configUSE_TICK_HOOK                                    0
    #define configUSE_MALLOC_FAILED_HOOK                           0
    #define configUSE_DAEMON_TASK_STARTUP_HOOK                     0
    #define configUSE_SB_COMPLETED_CALLBACK                        0
    #define configCHECK_FOR_STACK_OVERFLOW                         0
    #define configGENERATE_RUN_TIME_STATS                          0
    #define configUSE_TRACE_FACILITY                               0
    #define configUSE_STATS_FORMATTING_FUNCTIONS                   0
    #define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
    #define configTOTAL_MPU_REGIONS                                8
    #define configTEX_S_C_B_FLASH                                  0x07UL
    #define configTEX_S_C_B_SRAM                                   0x07UL
    #define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY            1
    #define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS             0
    #define configUSE_MPU_WRAPPERS_V1                              0
    #define configPROTECTED_KERNEL_OBJECT_POOL_SIZE                10
    #define configSYSTEM_CALL_STACK_SIZE                           128
    #define configENABLE_ACCESS_CONTROL_LIST                       1
    #define configCHECK_HANDLER_INSTALLATION                       1
    #define configUSE_TASK_NOTIFICATIONS                           1
    #define configUSE_MUTEXES                                      1
    #define configUSE_RECURSIVE_MUTEXES                            1
    #define configUSE_COUNTING_SEMAPHORES                          1
    #define configUSE_POSIX_ERRNO                                  0
    #define INCLUDE_vTaskPrioritySet                               1
    #define INCLUDE_uxTaskPriorityGet                              1
    #define INCLUDE_vTaskDelete                                    1
    #define INCLUDE_vTaskSuspend                                   1
    #define INCLUDE_vTaskDelayUntil                                1
    #define INCLUDE_vTaskDelay                                     1
    #define INCLUDE_xTaskGetSchedulerState                         1
    #define INCLUDE_xTaskGetCurrentTaskHandle                      1
    #define INCLUDE_xTaskResumeFromISR                             1
    #define configKERNEL_PROVIDED_STATIC_MEMORY                    1
#endif

#define configASSERT(CONDITION) \
    do                          \
    {                           \
        if (!(CONDITION))       \
        {                       \
            panic;              \
        }                       \
    }                           \
    while (false)
