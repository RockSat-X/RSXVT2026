////////////////////////////////////////////////////////////////////////////////
//
// Helper routines that'll be accessible in every meta-directive.
//



/* #meta global CMSIS_SET, CMSIS_WRITE, CMSIS_SPINLOCK, CMSIS_TUPLE, make_named_enums

    from deps.stpy.cmsis_tools import get_cmsis_tools

    cmsis_tools    = get_cmsis_tools(Meta)
    CMSIS_SET      = cmsis_tools.CMSIS_SET
    CMSIS_WRITE    = cmsis_tools.CMSIS_WRITE
    CMSIS_SPINLOCK = cmsis_tools.CMSIS_SPINLOCK
    CMSIS_TUPLE    = cmsis_tools.CMSIS_TUPLE

    def make_named_enums(members_string):

        members = members_string.split()

        enumeration_name = Meta.meta_directive.include_file_path.stem

        Meta.enums(enumeration_name, 'u32', members)

        Meta.lut(f'{enumeration_name}_TABLE', (
            (
                ('char*', 'name', f'"{member}"',),
            )
            for member in members
        ))

*/



////////////////////////////////////////////////////////////////////////////////
//
// Standard headers.
//



#include <string.h>
#include <stdatomic.h>



////////////////////////////////////////////////////////////////////////////////
//
// Some useful macro definitions.
//



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
#define static_assert_expr(...) ((void) sizeof(struct { static_assert(__VA_ARGS__, #__VA_ARGS__); }))
#define PI                      3.14159265359f
#ifndef offsetof
#define offsetof __builtin_offsetof
#endif



////////////////////////////////////////////////////////////////////////////////
//
// We could just use the definitions from <stdint.h>,
// but they won't necessarily match up well
// with `printf` well at all. For example, we'd like
// for the "%d" specifier to work with `i32`
//



typedef unsigned char      u8;  static_assert(sizeof(u8 ) == 1);
typedef unsigned short     u16; static_assert(sizeof(u16) == 2);
typedef unsigned           u32; static_assert(sizeof(u32) == 4);
typedef unsigned long long u64; static_assert(sizeof(u64) == 8);
typedef signed   char      i8;  static_assert(sizeof(i8 ) == 1);
typedef signed   short     i16; static_assert(sizeof(i16) == 2);
typedef signed             i32; static_assert(sizeof(i32) == 4);
typedef signed   long long i64; static_assert(sizeof(i64) == 8);
typedef unsigned char      b8;  static_assert(sizeof(b8 ) == 1);
typedef unsigned short     b16; static_assert(sizeof(b16) == 2);
typedef unsigned           b32; static_assert(sizeof(b32) == 4);
typedef unsigned long long b64; static_assert(sizeof(b64) == 8);
typedef float              f32; static_assert(sizeof(f32) == 4);
typedef double             f64; static_assert(sizeof(f64) == 8);



#if !COMPILING_ESP32



////////////////////////////////////////////////////////////////////////////////
//
// @/`Error Handling with Bugs and Sorrys`.
//



#define BUG_CODE 0xDEADC0DE // Large arbitrary value that an enum will unlikely overlap with.
#define sorry    sorry_();  // This syntax makes `sorry` pop out more obviously.

#if 1
    #define bug                                                                  \
        do                                                                       \
        {                                                                        \
            sorry            /* Halt the CPU to make debugging issues easier. */ \
            return BUG_CODE; /* Here for type-checking reasons. */               \
        }                                                                        \
        while (false)
#else
    #define bug return BUG_CODE // Bugs are bubbled up and handled by the caller.
#endif

static noret void sorry_(void);



////////////////////////////////////////////////////////////////////////////////
//
// CMSIS Device Headers.
//



#if TARGET_MCU_IS_STM32H533RET6 || TARGET_MCU_IS_STM32H533VET6
    #include <deps/cmsis-device-h5/Include/stm32h533xx.h>
#endif



////////////////////////////////////////////////////////////////////////////////
//
// System and FreeRTOS.
//



// @/`Linker Symbols`.

extern nullptr_t INITIAL_STACK_ADDRESS[];
#define INITIAL_STACK_ADDRESS ((u32) &INITIAL_STACK_ADDRESS)



// @/`STPY`.

#include "deps/stpy/stpy.h"
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



// Configurations for FreeRTOS.
// @/url:`https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/02-Customization`.

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

#define configASSERT(CONDITION) /* TODO Have a switch? */ \
    do                                                    \
    {                                                     \
        if (!(CONDITION))                                 \
        {                                                 \
            sorry                                         \
        }                                                 \
    }                                                     \
    while (false)

#define configMAX_PRIORITIES 8 // @/`FreeRTOS Max Priorities`.

#define configMAX_SYSCALL_INTERRUPT_PRIORITY (10 << (8 - __NVIC_PRIO_BITS)) // @/`FreeRTOS SysCall Interrupt Priority`.
static_assert(configMAX_SYSCALL_INTERRUPT_PRIORITY <= 255);



////////////////////////////////////////////////////////////////////////////////
//
// FreeRTOS Support.
//



#include <deps/FreeRTOS_Kernel/include/FreeRTOS.h>
#include <deps/FreeRTOS_Kernel/include/task.h>
#include <deps/FreeRTOS_Kernel/include/semphr.h>

#if TARGET_USES_FREERTOS
    #if TARGET_MCU_IS_STM32H533RET6 || TARGET_MCU_IS_STM32H533VET6
        #include <deps/FreeRTOS_Kernel/tasks.c>
        #include <deps/FreeRTOS_Kernel/queue.c>
        #include <deps/FreeRTOS_Kernel/list.c>
        #include <deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/port.c>
        #include <deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c>
    #endif
#endif



// @/`FREERTOS_TASK Macro`.

#define FREERTOS_TASK(NAME, STACK_SIZE, PRIORITY)                                                             \
    static_assert                                                                                             \
    (                                                                                                         \
        FreeRTOSTask_##NAME >= 0 || "The `FREERTOS_TASK` macro is being invoked with an unrecognized syntax!" \
    );                                                                                                        \
    static noret void NAME(void*)



#include "freertos_init.meta"
/* #meta

    import types, re

    for target in PER_TARGET():



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
                for(;;);
            ''')

*/



////////////////////////////////////////////////////////////////////////////////
//
// System Interrupts.
//



__attribute__((naked))
INTERRUPT_Reset(void)
{
    __asm volatile
    (R"(

        // For global variables that are initially non-zero,
        // we copy it from flash and write it into RAM.

        ldr r0, =LINK_data_load_addr_start
        ldr r1, =LINK_data_virt_addr_start
        ldr r2, =LINK_data_virt_addr_end

        b is_copying_data_done
        copy_data:
            ldr r3, [r0]
            str r3, [r1]
            add r0, 4
            add r1, 4
        is_copying_data_done:
            cmp r1, r2
            bne copy_data



        // For any global variables that are initially zero,
        // we zero out that part of the RAM.

        mov r0, 0
        ldr r1, =LINK_bss_addr_start
        ldr r2, =LINK_bss_addr_end

        b is_zeroing_bss_done
        zero_bss:
            str r0, [r1]
            add r1, 4
        is_zeroing_bss_done:
            cmp r1, r2
            bne zero_bss



        // Enable the floating point coprocessor.
        // Must be done here since GCC can emit
        // vector push/pop instructions on entering main
        // that'd otherwise emit an illegal instruction fault.
        // @/pg 597/tbl B3-4/`Armv7-M`.
        // @/pg 1488/sec D1.2.14/`Armv8-M`.

        ldr r0, =0xE000ED88                     // CPACR register address.
        ldr r1, [r0]
        orr r1, r1, (0b11 << 22) | (0b11 << 20) // Full access to coprocessor 10 and 11.
        str r1, [r0]



        // Finished initializing, we begin main!

        b main

    )");
}



INTERRUPT_UsageFault(void)
{

    // @/pg 611/sec B3.2.15/`Armv7-M`.
    // @/pg 1901/sec D1.2.267/`Armv8-M`.
    u32 usage_fault_status = CMSIS_GET(SCB, CFSR, USGFAULTSR);
    b32 stack_overflow     = (usage_fault_status >> 4) & 1; // This is only defined on Armv8-M.

    sorry // See the values above to determine what caused this UsageFault.

}



INTERRUPT_BusFault(void)
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

    sorry // See the values above to determine what caused this BusFault.

}



INTERRUPT_Default(void)
{

    // @/pg 599/sec B3.2.4/`Armv7-M`.
    // @/pg 1680/sec D1.2.125/`Armv8-M`.
    i32 interrupt_number = (i32) CMSIS_GET(SCB, ICSR, VECTACTIVE) - 16;

    switch (interrupt_number)
    {
        case HardFault_IRQn:
        {
            sorry // There was a HardFault for some unhandled reason...
        } break;

        default:
        {
            sorry // TODO.
        } break;
    }

}



////////////////////////////////////////////////////////////////////////////////
//
// System Helpers.
//



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



static noret void
sorry_(void) // @/`Halting`.
{

    __disable_irq();

    for (;;)
    {
        u32 i = 10'000'000;

        for (; i > 1'000; i /= 2)
        {
            for (u32 j = 0; j < 4; j += 1)
            {

                // TODO For now, the way we're telling that we're on
                //      a Nucleo-H533RE board is if we're working with
                //      a STM32H533RET6 MCU; if it's a STM32H533VET6,
                //      then we assume we're on a PCB with an RGB led.
                //      The better way to do actually do this is just
                //      have the target definition be able to specify
                //      the method to which a halt is indicated.

                #if TARGET_MCU_IS_STM32H533RET6

                    GPIO_ACTIVE(led_green);
                    spinlock_nop(i);

                    GPIO_INACTIVE(led_green);
                    spinlock_nop(i);

                #else

                    GPIO_ACTIVE  (led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );
                    spinlock_nop(i);

                    GPIO_INACTIVE(led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );
                    spinlock_nop(i);

                #endif

            }
        }
    }

}



/* #meta export IMPLEMENT_DRIVER_SUPPORT

    import types, collections

    def IMPLEMENT_DRIVER_SUPPORT(
        *,
        driver_type,
        cmsis_name,
        common_name,
        expansions,
        terms,
    ):

        for target in PER_TARGET():



            # Get all of the target's drivers.

            drivers = [
                driver
                for driver in target.drivers
                if driver['type'] == driver_type
            ]

            if not drivers:

                Meta.line(
                    f'#error Target {repr(target.name)} defines no '
                    f'handle for the {repr(driver_type)} driver.'
                )

                continue



            # Make an enumeration for driver handles.

            Meta.enums(
                f'{driver_type}Handle',
                'u32',
                (driver['handle'] for driver in drivers)
            )



            # @/`CMSIS Suffix Dropping`.
            # e.g:
            # >
            # >    I2Cx <-> I2C3
            # >

            Meta.line(f'#define {common_name}_ {cmsis_name}_')



            # Process each driver term; some are about adding a field to
            # the look-up table while others are about interrupt routines.

            lookup_table_fields = collections.defaultdict(lambda: [])
            interrupt_routines  = collections.defaultdict(lambda: [])
            nvic_mappings       = []

            for driver in drivers:

                for term_name, term_type, *term_value in terms(**driver):

                    if term_value:
                        term_value, = term_value
                    else:
                        term_value = term_name.format(driver['peripheral'])

                    match term_type:



                        # e.g:
                        # >
                        # >                  NVICInterrupt_{}_EV
                        # >                           |
                        # >                           v
                        # >    NVICInterrupt_I2Cx_EV <-> NVICInterrupt_I2C3_EV
                        # >

                        case 'expression':

                            field_identifier                       = term_name.format(common_name)
                            lookup_table_fields[field_identifier] += [term_value]



                        # e.g:
                        # >
                        # >                {}_KERNEL_SOURCE
                        # >                        |
                        # >                        v
                        # >    I2Cx_KERNEL_SOURCE <-> I2C3_KERNEL_SOURCE
                        # >                                   |
                        # >                                   v
                        # >                          RCC_CCIPR4_I2C3SEL
                        # >

                        case 'cmsis_tuple':

                            field_identifier                       = term_name.format(common_name)
                            lookup_table_fields[field_identifier] += [CMSIS_TUPLE(target.mcu, term_value)]



                        # e.g:
                        # >
                        # >     INTERRUPT_{}_UP
                        # >             |
                        # >             v
                        # >    INTERRUPT_TIM5_UP
                        # >

                        case 'interrupt':

                            nvic_mappings += [(
                                f'{term_name.format(common_name)}_{driver['handle']}',
                                f'{term_value}',
                            )]

                            interrupt_routines[driver['handle']] += [term_value]



                        case idk: raise ValueError(f'Unknown term type: {repr(idk)}.')



            # Create macros to map NVIC interrupts using the driver handle name.

            for nvic_driver_handle, nvic_peripheral in nvic_mappings:

                Meta.define(
                    f'NVICInterrupt_{nvic_driver_handle}',
                    f'NVICInterrupt_{nvic_peripheral}'
                )



            # TODO Rework `Meta.lut`...

            Meta.lut(f'{driver_type.upper()}_TABLE', (
                (
                    f'{driver_type}Handle_{driver['handle']}',
                    *(
                        (identifier, values[driver_i])
                        for identifier, values in lookup_table_fields.items()
                    ),
                ) for driver_i, driver in enumerate(drivers)
            ))



            # All of the interrupt routines will be redirected
            # to a common procedure of `_XYZ_driver_interrupt`.

            if interrupt_routines:

                Meta.line(f'''
                    static void
                    _{driver_type.upper()}_driver_interrupt(enum {driver_type}Handle handle);
                ''')

                for driver_handle, driver_interrupt_names in interrupt_routines.items():

                    for driver_interrupt_name in driver_interrupt_names:

                        Meta.line(f'''
                            INTERRUPT_{driver_interrupt_name}(void)
                            {{
                                _{driver_type.upper()}_driver_interrupt({driver_type}Handle_{driver_handle});
                            }}
                        ''')



        # Make a macro to bring all of the fields
        # in the look-up table into the local stack.

        Meta.line('#undef _EXPAND_HANDLE')

        with Meta.enter('#define _EXPAND_HANDLE'):

            Meta.line(f'''
                if (!(0 <= handle && handle < {driver_type}Handle_COUNT))
                    sorry
            ''') # TODO Replace sorry with bug.

            for identifier, value in expansions:
                Meta.line(f'''
                    auto const {identifier} = {value};
                ''')

            for field_identifier in lookup_table_fields:

                Meta.line(f'''
                    auto const {field_identifier} = {driver_type.upper()}_TABLE[handle].{field_identifier};
                ''')

*/



////////////////////////////////////////////////////////////////////////////////
//
// Miscellaneous.
//



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include <printf/printf.c>
#pragma GCC diagnostic pop

#include "ringbuffer.c"
#include "log.c"



#endif



////////////////////////////////////////////////////////////////////////////////
//
// ESP32 related stuff.
//



#define PACKET_ESP32_START_TOKEN 0xBABE
#define PACKET_LORA_START_TOKEN  0xCAFE

pack_push

    struct PacketLoRa
    {
        f32 quaternion_i;
        f32 quaternion_j;
        f32 quaternion_k;
        f32 quaternion_r;
        f32 accelerometer_x;
        f32 accelerometer_y;
        f32 accelerometer_z;
        f32 gyro_x;
        f32 gyro_y;
        f32 gyro_z;
        f32 computer_vision_confidence;
        u16 timestamp_ms;
        u8  sequence_number;
        u8  crc;
    };

    struct PacketESP32
    {
        f32               magnetometer_x;
        f32               magnetometer_y;
        f32               magnetometer_z;
        u8                image_chunk[190];
        struct PacketLoRa nonredundant;
    };

pack_pop

static_assert(sizeof(struct PacketESP32) <= 250);



// TODO Document.
// TODO Have look-up table.
extern useret u8
ESP32_calculate_crc(u8* data, i32 length)
{
    u8 crc = 0xFF;

    for (i32 i = 0; i < length; i += 1)
    {
        crc ^= data[i];

        for (i32 j = 0; j < 8; j += 1)
        {
            crc = (crc & (1 << 7))
                ? (crc << 1) ^ 0x2F
                : (crc << 1);
        }
    }

    return crc;
}



#define VN100_ESP32_BAUD 400000 // @/`Coupled Baud Rate between STM32 and ESP32`.

#if COMPILING_ESP32

    #include <WiFi.h>
    #include <esp_wifi.h>
    #include <esp_now.h>
    #include <RadioLib.h>



    extern void
    common_init_uart(void)
    {
        Serial1.setRxBufferSize(1024); // TODO Look into more?
        Serial1.begin(VN100_ESP32_BAUD, SERIAL_8N1, D7, D6);
        while (!Serial1);
    }



    // TODO Look more into the specs.
    // TODO Make robust.
    extern void
    common_init_esp_now(void)
    {

        WiFi.mode(WIFI_STA);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

        if (esp_now_init() != ESP_OK)
        {
            Serial.printf("Error initializing ESP-NOW.\n");
            ESP.restart();
            return;
        }

    }



    static SX1262 packet_lora_radio = new Module(41, 39, 42, 40);



    // TODO Look more into the specs.
    // TODO Make robust.
    extern void
    common_init_lora()
    {

        if (packet_lora_radio.begin() != RADIOLIB_ERR_NONE)
        {
            Serial.printf("Failed to initialize radio.\n");
            ESP.restart();
            return;
        }

        packet_lora_radio.setFrequency(915.0);
        packet_lora_radio.setBandwidth(7.8);
        packet_lora_radio.setSpreadingFactor(6);
        packet_lora_radio.setCodingRate(5);
        packet_lora_radio.setOutputPower(22);
        packet_lora_radio.setPreambleLength(8);
        packet_lora_radio.setSyncWord(0x34);
        packet_lora_radio.setCRC(true);

        extern void packet_lora_callback(void);

        packet_lora_radio.setDio1Action(packet_lora_callback);

    }

#endif



////////////////////////////////////////////////////////////////////////////////
//
// Vehicle interface.
//



// TODO Document.
// TODO Have look-up table.
extern useret u8
VEHICLE_INTERFACE_calculate_crc(u8* data, i32 length)
{
    u8 crc = 0xFF;

    for (i32 i = 0; i < length; i += 1)
    {
        crc ^= data[i];

        for (i32 j = 0; j < 8; j += 1)
        {
            crc = (crc & (1 << 7))
                ? (crc << 1) ^ 0x2F
                : (crc << 1);
        }
    }

    return crc;
}



pack_push

    enum VehicleInterfacePayloadFlag : u16
    {
        VehicleInterfacePayloadFlag_stepper_motor_axis_x_okay,
        VehicleInterfacePayloadFlag_stepper_motor_axis_y_okay,
        VehicleInterfacePayloadFlag_stepper_motor_axis_z_okay,
        VehicleInterfacePayloadFlag_wifi_okay,
        VehicleInterfacePayloadFlag_lora_okay,
        VehicleInterfacePayloadFlag_openmv_okay,
        VehicleInterfacePayloadFlag_vn100_okay,
    };

    struct VehicleInterfacePayload
    {
        u16                              timestamp_us;
        enum VehicleInterfacePayloadFlag flags;
        u8                               crc;
    };

pack_pop



//////////////////////////////////////// Notes ////////////////////////////////////////



// This file (and this includes the entire folder) contains
// definitions for stuff that's quite low-level and technical.
// Most of it is just infrastructure to make programming the
// higher-level stuff on STM32s very pain-free and fun.



// @/`Error Handling with Bugs and Sorrys`:
//
// There are two main ways handling run-time errors.
//
// The most brute-forced way is with a `sorry`.
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
//
// When a `sorry` is executed, the CPU is halted and the implementation of `sorry`
// will have the MCU do something to indicate the error condition (e.g. blinking LED).
// This way of error handling makes it very easy to debug stuff, because the
// offending line (or at least when we detected the error) can be inspected right
// where the CPU was halted. The `sorry` makes no assumption on where it can be
// called, so it can be used within an interrupt routine or before the MCU is even
// fully initialized.
//
// The obvious disadvantage of this method is that it is absolutely destructive.
// A small error would just completely halt the entire MCU, which is not desirable
// in any production context.
//
// The proper way to handle errors gracefully is to use the `bug` macro.
// e.g:
// >
// >    if (is_sd_card_mounted_yet())
// >    {
// >        do_stuff();
// >    }
// >    else
// >    {
// >        bug;
// >    }
// >
//
// The difference is that the procedure that `bug` is used in must have an integral
// error code as a return value, because that's what all `bug` does really:
// return the error code `BUG_CODE`.
//
// The reason why the `bug` macro is useful is because during development,
// it can be synonymous with `sorry`. This will make the CPU halt whenever
// a bug error condition happens, and debugging can be done easily from there
// on out. For production, however, the `bug` macro will turn into a simple
// return statement. How the error condition will be handled will be up to
// the caller.
//
// The procedure's return type could just be a boolean where zero
// indicates success (no bug) or non-zero where a bug did happen,
// but the most general approach is to have a dedicated enumeration
// for that procedure's return error code. By doing this, more specific
// error conditions can be expressed, with `BUG_CODE` being the most vague
// and "catastrophic" (as in this shouldn't have happened at all!).
//
// Thus `bug` should be used whenever the programmer expects such an
// error condition to never happen, but if it were to, it's worthwhile to
// halt the MCU for and debug. Other "explainable" error conditions like
// "the SD card not being able to be initialized" should just be a separate
// error code.
//
// The `sorry` macro is still useful as a temporary measure for error handling;
// once things become more production-ready, they become replaced with `bug`.



// @/`FreeRTOS Max Priorities`:
//
// This setting just tells FreeRTOS how many
// task priorities it should allocate; the
// bigger the number, the more granularity a
// particular task has to indicate its priority.
// This option is irrelevant of interrupt priorities.



// @/`FreeRTOS SysCall Interrupt Priority`:
//
// This setting splits the range of interrupt priorities
// at which any FreeRTOS API function calls can be made.
// e.g:
// >
// >               __NVIC_PRIO_BITS = 4
// >               ~~~~~~~~~~~~~~~~~~~~
// >    Niceness      |
// >          vv     ~~~~
// >          15 : 0b1111'xxxx ~
// >          14 : 0b1110'xxxx |--- These interrupt priorities are nicer.
// >          13 : 0b1101'xxxx |    Any interrupt routine can execute
// >          12 : 0b1100'xxxx |    FreeRTOS API functions from here.
// >          11 : 0b1011'xxxx |
// >          10 : 0b1010'xxxx ~ <- configMAX_SYSCALL_INTERRUPT_PRIORITY = 0b1010'xxxx
// >           9 : 0b1001'xxxx ~
// >           8 : 0b1000'xxxx |
// >           7 : 0b0111'xxxx |
// >           6 : 0b0110'xxxx |
// >           5 : 0b0101'xxxx |--- These interrupt priorities are more greedy.
// >           4 : 0b0100'xxxx |    These interrupts should not execute any
// >           3 : 0b0011'xxxx |    FreeRTOS API functions from here.
// >           2 : 0b0010'xxxx |
// >           1 : 0b0001'xxxx |
// >           0 : 0b0000'xxxx ~
// >



// @/`Linker Symbols`:
//
// The initial stack address is determined by the linker script.
//
// The linker will create the `INITIAL_STACK_ADDRESS` symbol,
// and the way linker symbols work is that symbols are associated
// with the address of the value that the symbol for.
//
// For instance, the symbol `foobar` might be associated with the
// address `0xDEADBEEF` because that's where `foobar` is located at.
// In C, we can get the address of the symbol by doing `&foobar`.
//
// However, `INITIAL_STACK_ADDRESS` is associated with an address,
// not for some variable called `INITIAL_STACK_ADDRESS`, but literally
// where the initial stack pointer will be at.
//
// Thus, in order to get that initial address value, we do
// `&INITIAL_STACK_ADDRESS`. The value `INITIAL_STACK_ADDRESS`
// itself isn't what we'd actually want.



// @/`FREERTOS_TASK Macro`:
//
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



// @/`STPY`:
//
// STPY is something I, Phuc Doan, am currently working on.
//
// All it is just a set of Python scripts that generate C code
// to initialize the STM32 MCU easily. This includes brute-forcing
// the clock-tree and configuring GPIOs and interrupts.



// @/`Coupled Baud Rate between STM32 and ESP32`:
//
// There's two places where the ESP32 UART baud rate
// is currently defined: in `./electrical/Shared.py` and
// right here in `./electrical/system.h`. This is because
// of the mixed development environments between Arduino
// and the stuff for STM32. To ensure the baud rate is
// consistent between the two environment, we take advantage
// of macro redefinition conflicts.
//
// If `./electrical/Shared.py` defines the buad rate to be
// something different than in `./electrical/system.h`,
// the macro definition will create a compile-time error;
// if they're the same, nothing happens.
//
// In short, to change the UART baud rate, update the value
// in `./electrical/system.h` and `./electrical/Shared.py`.
// It can't get better than that.
