//////////////////////////////////////////////////////////////// Primitives ////////////////////////////////////////////////////////////////



#define false                   0
#define true                    1
#define STRINGIFY_(X)           #X
#define STRINGIFY(X)            STRINGIFY_(X)
#define CONCAT_(X, Y)           X##Y
#define CONCAT(X, Y)            CONCAT_(X, Y)
#define IS_POWER_OF_TWO(X)      ((X) > 0 && ((X) & ((X) - 1)) == 0)
#define sizeof(...)             ((signed) sizeof(__VA_ARGS__))
#define countof(...)            (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define bitsof(...)             (sizeof(__VA_ARGS__) * 8)
#define implies(P, Q)           (!(P) || (Q))
#define iff(P, Q)               (!!(P) == !!(Q))
#define useret                  __attribute__((warn_unused_result))
#define mustinline              __attribute__((always_inline)) inline
#define noret                   __attribute__((noreturn))
#define fallthrough             __attribute__((fallthrough))
#define pack_push               _Pragma("pack(push, 1)")
#define pack_pop                _Pragma("pack(pop)")
#define memeq(X, Y)             (static_assert_expr(sizeof(X) == sizeof(Y)), !memcmp(&(X), &(Y), sizeof(Y)))
#define memzero(X)              memset((X), 0, sizeof(*(X)))
#define static_assert(...)      _Static_assert(__VA_ARGS__, #__VA_ARGS__)
#define static_assert_expr(...) ((void) sizeof(struct { static_assert(__VA_ARGS__, #__VA_ARGS__); }))
#ifndef offsetof
#define offsetof __builtin_offsetof
#endif

#include "primitives.meta"
/* #meta

    # We could just use the definitions from <stdint.h>,
    # but they won't necessarily match up well
    # with `printf` well at all. For example, we'd like
    # for the "%d" specifier to work with `i32`

    for name, underlying, size in (
        ('u8'  , 'unsigned char'     , 1),
        ('u16' , 'unsigned short'    , 2),
        ('u32' , 'unsigned'          , 4),
        ('u64' , 'unsigned long long', 8),
        ('i8'  , 'signed char'       , 1),
        ('i16' , 'signed short'      , 2),
        ('i32' , 'signed'            , 4),
        ('i64' , 'signed long long'  , 8),
        ('b8'  , 'signed char'       , 1),
        ('b16' , 'signed short'      , 2),
        ('b32' , 'signed'            , 4),
        ('b64' , 'signed long long'  , 8),
        ('f32' , 'float'             , 4),
        ('f64' , 'double'            , 8),
    ):
        Meta.line(f'''
            typedef {underlying} {name};
            static_assert(sizeof({name}) == {size});
        ''')

*/


extern nullptr_t INITIAL_STACK_ADDRESS[];
#define INITIAL_STACK_ADDRESS ((u32) &INITIAL_STACK_ADDRESS)


#include <stdint.h>
#include "deps/stpy/stpy.h"

#if TARGET_MCU_IS_STM32H7S3L8H6
    #include <deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h>
#endif

#if TARGET_MCU_IS_STM32H533RET6
    #include <deps/cmsis-device-h5/Include/stm32h533xx.h>
#endif


//////////////////////////////////////////////////////////////// System Initialization ////////////////////////////////////////////////////////////////


/* #meta IMPLEMENT_DRIVER_ALIASES

    def IMPLEMENT_DRIVER_ALIASES(
        *,
        driver_name,
        cmsis_name,
        common_name,
        identifiers,
        cmsis_tuple_tags,
    ):

        for target in PER_TARGET():



            if driver_name not in target.drivers:

                Meta.line(
                    f'#error Target "{target.name}" cannot use the {driver_name} driver '
                    f'without first specifying the peripheral instances to have handles for.'
                )

                continue



            # Have the user be able to specify a specific driver unit.

            Meta.enums(
                f'{driver_name}Handle',
                'u32',
                (handle for handle, instance in target.drivers[driver_name])
            )



            # @/`CMSIS Suffix Dropping`.
            # e.g:
            # >
            # >    I2Cx <-> I2C3
            # >

            Meta.line(f'#define {common_name}_ {cmsis_name}_')



            # Create a look-up table to map the moniker
            # name to the actual underyling value.

            Meta.lut( f'{driver_name}_TABLE', (
                (
                    f'{driver_name}Handle_{handle}',
                    (common_name, instance),
                    *(
                        (identifier.format(common_name), identifier.format(instance))
                        for identifier in identifiers
                    ),
                    *(
                        (
                            cmsis_tuple_tag.format(common_name),
                            CMSIS_TUPLE(target.mcu, cmsis_tuple_tag.format(instance))
                        )
                        for cmsis_tuple_tag in cmsis_tuple_tags
                    )
                )
                for handle, instance in target.drivers[driver_name]
            ))



        # Macro to mostly bring stuff in the
        # look-up table into the local scope.

        Meta.line('#undef _EXPAND_HANDLE')

        with Meta.enter('#define _EXPAND_HANDLE'):

            Meta.line(f'''

                if (!(0 <= handle && handle < {driver_name}Handle_COUNT))
                {{
                    panic;
                }}

                struct {driver_name}Driver* const driver = &_{driver_name}_drivers[handle];

            ''')

            for alias in (common_name, *identifiers, *cmsis_tuple_tags):
                Meta.line(f'''
                    auto const {alias.format(common_name)} = {driver_name}_TABLE[handle].{alias.format(common_name)};
                ''')

*/


#include "STPY_init.meta"
/* #meta

    from deps.stpy.init import init

    for target in PER_TARGET():
        init(
            Meta       = Meta,
            target     = target.name,
            mcu        = target.mcu,
            schema     = target.schema,
            gpios      = target.gpios,
            interrupts = target.interrupts,
        )

*/



//////////////////////////////////////////////////////////////// Low-Level Routines ////////////////////////////////////////////////////////////////



#define nop() __asm__("nop")



static void
spinlock_nop(u32 count)
{
    // This is unrolled so that the branch penalty will be reduced.
    // Otherwise, the amount of delay that this procedure will produce
    // will be more inconsistent (potentially due to flash cache reasons?).
    for (u32 nop = 0; nop < count; nop += 256)
    {
        #define NOP4   nop(); nop(); nop(); nop();
        #define NOP16  NOP4   NOP4   NOP4   NOP4
        #define NOP64  NOP16  NOP16  NOP16  NOP16
        #define NOP256 NOP64  NOP64  NOP64  NOP64
        NOP256
        #undef NOP4
        #undef NOP16
        #undef NOP64
        #undef NOP256
    }
}



#if false

    static volatile u32 epoch_ms = 0;

    INTERRUPT_SysTick
    {
        epoch_ms += 1;
    }

    static void
    spinlock_ms(u32 duration_ms)
    {
        u32 start_ms = epoch_ms;
        while (epoch_ms - start_ms < duration_ms);
    }

#endif



#define sorry halt_(false); // @/`Halting`.
#define panic halt_(true)   // "
static noret void           // "
halt_(b32 panicking)        // "
{

    __disable_irq();

    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        if (panicking)
        {
            GPIO_HIGH(led_red);
        }
        else
        {
            GPIO_HIGH(led_yellow);
        }

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 8; j += 1)
                {
                    spinlock_nop(i);

                    if (panicking)
                    {
                        GPIO_TOGGLE(led_red);
                    }
                    else
                    {
                        GPIO_TOGGLE(led_yellow);
                    }
                }
            }
        }

    #else // We're going to assume there's an LED we can toggle.

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 4; j += 1)
                {
                    GPIO_HIGH(led_green);
                    spinlock_nop(i);

                    GPIO_LOW(led_green);
                    spinlock_nop(i * (panicking ? 1 : 4));
                }
            }
        }

    #endif

}



INTERRUPT_UsageFault
{

    // @/pg 611/sec B3.2.15/`Armv7-M`.
    // @/pg 1901/sec D1.2.267/`Armv8-M`.
    u32 usage_fault_status = CMSIS_GET(SCB, CFSR, USGFAULTSR);
    b32 stack_overflow     = (usage_fault_status >> 4) & 1; // This is only defined on Armv8-M.

    panic; // See the values above to determine what caused this UsageFault.

}



INTERRUPT_BusFault
{

    // @/pg 611/sec B3.2.15/`Armv7-M`.
    // @/pg 1472/sec D1.2.7/`Armv8-M`.
    u32 bus_fault_status             = CMSIS_GET(SCB, CFSR, BUSFAULTSR);
    b32 bus_fault_address_valid      = (bus_fault_status >> 7) & 1;
    b32 imprecise_data_access        = (bus_fault_status >> 2) & 1;
    b32 precise_data_access          = (bus_fault_status >> 1) & 1;
    b32 instruction_access_violation = (bus_fault_status >> 0) & 1;

    // @/pg 614/sec B3.2.18/`Armv7-M`.
    // @/pg 1471/sec D1.2.6/`Armv8-M`.
    u32 bus_fault_address = SCB->BFAR;

    panic; // See the values above to determine what caused this BusFault.

}



INTERRUPT_Default
{

    // @/pg 599/sec B3.2.4/`Armv7-M`.
    // @/pg 1680/sec D1.2.125/`Armv8-M`.
    i32 interrupt_number = CMSIS_GET(SCB, ICSR, VECTACTIVE) - 16;

    switch (interrupt_number)
    {
        case HardFault_IRQn:
        {
            panic; // There was a HardFault for some unhandled reason...
        } break;

        default:
        {
            panic; // TODO.
        } break;
    }

}



//////////////////////////////////////////////////////////////// FreeRTOS ////////////////////////////////////////////////////////////////



// FreeRTOS headers here so that we can still compile
// tasks that use FreeRTOS functions; if the function
// actually gets used, there'll be a link error anyways.

#include <deps/FreeRTOS_Kernel/include/FreeRTOS.h>
#include <deps/FreeRTOS_Kernel/include/task.h>
#include <deps/FreeRTOS_Kernel/include/semphr.h>



// This macro just expands to the function prototype used
// by all FreeRTOS tasks, but the main reason why we use
// this is because it allows us to have a meta-directive to
// search for instances of this macro and from there be able
// to determine the target's list of tasks. It is not a
// fool-proof strategy, however, because we can do something
// dumb like wrap a task with #if and the meta-directive wouldn't
// know that, but I think edge-cases like these can be ignored.
// We can, however, assert that the meta-directive picked up
// the `FREERTOS_TASK`; if we don't, the task might be silently
// defined but not be registered by the task-scheduler.

#define FREERTOS_TASK(NAME, STACK_SIZE, PRIORITY)                                                             \
    static_assert(                                                                                            \
        FreeRTOSTask_##NAME >= 0 || "The `FREERTOS_TASK` macro is being invoked with an unrecognized syntax!" \
    );                                                                                                        \
    static noret void NAME(void*)



#include "freertos.meta"
/* #meta

    import re

    for target in PER_TARGET():



        # Macros for whether or not the target will be using FreeRTOS.

        Meta.define('TARGET_USES_FREERTOS', target.use_freertos)



        # Find all the FreeRTOS tasks defined by the target.
        # Note that if `FREERTOS_TASK` is used in a file that's
        # included elsewhere, we would be missing that. In other
        # words, `FREERTOS_TASK` can only be used at the root of
        # the source file. This is pretty much fine for all use-cases.

        tasks = []

        for source_file_path in target.source_file_paths:

            for match in re.findall(
                r'FREERTOS_TASK\s*\((\s*[a-zA-Z0-9_]+\s*,\s*[0-9\']+\s*,\s*[a-zA-Z0-9_]+\s*)\)',
                '\n'.join(
                    line
                    for line in source_file_path.read_text().splitlines()
                    if not line.lstrip().startswith('//')
                )
            ):

                task_name, stack_size, priority = (field.strip() for field in match.split(','))

                tasks += [types.SimpleNamespace(
                    name       = task_name,
                    stack_size = int(stack_size),
                    priority   = priority
                )]



        # Create an enumeration of all FreeRTOS tasks for the target
        # so that if the meta-directive missed a FREERTOS_TASK
        # somewhere, compilation will fail.

        Meta.enums('FreeRTOSTask', 'u32', (task.name for task in tasks))



        # Rest of the stuff is only when the target actually uses FreeRTOS.

        if not target.use_freertos:
            continue



        # Declare some stuff for the tasks.

        for task in tasks:
            Meta.line(f'''
                static noret void   {task.name}(void*);
                static StackType_t  {task.name}_stack[{task.stack_size} / sizeof(StackType_t)] = {{0}};
                static StaticTask_t {task.name}_buffer = {{0}};
            ''')



        # Create the procedure that'll initialize and run the FreeRTOS scheduler.

        with Meta.enter('''
            static noret void
            FREERTOS_init(void)
        '''):

            for task in tasks:
                Meta.line(f'''
                    TaskHandle_t {task.name}_handle =
                        xTaskCreateStatic
                        (
                            {task.name},
                            "{task.name}",
                            countof({task.name}_stack),
                            nullptr,
                            {task.priority},
                            {task.name}_stack,
                            &{task.name}_buffer
                        );
                ''')

            Meta.line('''
                vTaskStartScheduler();
                panic;
            ''')

*/

#if TARGET_USES_FREERTOS

    #if TARGET_MCU_IS_STM32H7S3L8H6
        #include <deps/FreeRTOS_Kernel/tasks.c>
        #include <deps/FreeRTOS_Kernel/queue.c>
        #include <deps/FreeRTOS_Kernel/list.c>
        #include <port.c>
    #endif

    #if TARGET_MCU_IS_STM32H533RET6
        #include <deps/FreeRTOS_Kernel/tasks.c>
        #include <deps/FreeRTOS_Kernel/queue.c>
        #include <deps/FreeRTOS_Kernel/list.c>
        #include <port.c>
        #include <portasm.c>
    #endif

#endif


#if TARGET_USES_FREERTOS

    #define MUTEX_TAKE(MUTEX)                                          \
        do                                                             \
        {                                                              \
            if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) \
            {                                                          \
                if (!xSemaphoreTake((MUTEX), portMAX_DELAY))           \
                {                                                      \
                    panic;                                             \
                }                                                      \
            }                                                          \
        }                                                              \
        while (false)

    #define MUTEX_GIVE(MUTEX)                                          \
        do                                                             \
        {                                                              \
            if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) \
            {                                                          \
                if (!xSemaphoreGive((MUTEX)))                          \
                {                                                      \
                    panic;                                             \
                }                                                      \
            }                                                          \
        }                                                              \
        while (false)

#else

    #define MUTEX_TAKE(MUTEX)
    #define MUTEX_GIVE(MUTEX)

#endif



//////////////////////////////////////////////////////////////// Notes /////////////////////////////////////////////////////////////////



// This file (and this includes the entire folder) contains
// definitions for stuff that's quite low-level and technical.
// Most of it is just infrastructure to make programming the
// higher-level stuff on STM32s very pain-free and fun.



// @/`Halting`:
//
// A `sorry` is used for code-paths that haven't been
// implemented yet. Eventually, when we want things to
// be production-ready, we replace all `sorry`s that we
// can with proper code, or at the very least, a `panic`.
// e.g:
// >
// >    if (is_sd_card_mounted_yet())
// >    {
// >        do_stuff();
// >    }
// >    else
// >    {              Note that `sorry` is specifically defined to be able to
// >        sorry   <- be written without the terminating semicolon. This is
// >    }              purely aesthetic; it makes `sorry` look very out-of-place.
// >
// >
//
// A `panic` is for absolute irrecoverable error conditions,
// like stack overflows or a function given ill-formed parameters.
// In other words, stuff like: "something horrible happened.
// I don't know how we got here. We need to reset!". It is useful
// to make this distinction from `sorry` because we can scan through
// the code-base and see what stuff is work-in-progress or just an
// irrecoverable error condition.
// e.g:
// >
// >    switch (option)
// >    {
// >        case Option_A: ...     The compiler will enforce switch-statements to be exhaustive
// >        case Option_B: ...     for enumerations. The compiler will also enforce all
// >        case Option_C: ...     switch-statements to have a `default` case even if we are
// >        default: panic;     <- exhaustive. In the event that the enumeration `option` is
// >    }                          somehow not any of the valid values, we will trigger a `panic`
// >                               rather than have the switch-statement silently pass.
// >
//
// When a `sorry` or `panic` is triggered during deployment,
// the microcontroller will undergo a reset through the
// watchdog timer (TODO). During development, however, we'd
// like for the controller to actually stop dead in its track
// (rather than reset) so that we can debug. To make it even
// more useful, the microcontroller can also blink an LED
// indicating whether or not a `panic` or a `sorry` condition
// has occured; how this is implemented, if at all, is
// entirely dependent upon the target.
