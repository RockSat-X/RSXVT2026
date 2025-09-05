#meta STLINK_BAUD, TARGETS, MCUS, PER_TARGET, PER_MCU :

from deps.pxd.utils import root
import types



################################################################################
#
# Communication speed with the ST-Link.
#

STLINK_BAUD = 1_000_000



################################################################################
#
# Build directory where all build artifacts will be dumped to.
#

BUILD = root('./build')



################################################################################
#
# Supported microcontrollers.
#

MCUS = {
    'STM32H7S3L8H6' : types.SimpleNamespace(
        cmsis_file_path        = root('./deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h'),
        freertos_port_dir_path = root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM7/r0p1'),
        freertos_interrupts    = {
            'SysTick' : 'xPortSysTickHandler',
            'SVCall'  : 'vPortSVCHandler'    ,
            'PendSV'  : 'xPortPendSVHandler' ,
        },
        freertos_source_files = (
            'deps/FreeRTOS_Kernel/tasks.c',
            'deps/FreeRTOS_Kernel/queue.c',
            'deps/FreeRTOS_Kernel/list.c',
            'port.c',
        ),
    ),
    'STM32H533RET6' : types.SimpleNamespace(
        cmsis_file_path        = root('./deps/cmsis-device-h5/Include/stm32h533xx.h'),
        freertos_port_dir_path = root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        freertos_interrupts    = {
            'SysTick' : 'SysTick_Handler',
            'SVCall'  : 'SVC_Handler'    ,
            'PendSV'  : 'PendSV_Handler' ,
        },
        freertos_source_files = (
            'deps/FreeRTOS_Kernel/tasks.c',
            'deps/FreeRTOS_Kernel/queue.c',
            'deps/FreeRTOS_Kernel/list.c',
            'port.c',
            'portasm.c',
        ),
    ),
}



################################################################################
#
# Basic description of each of our firmware targets.
#

TARGETS = ( # @/`Defining a TARGET`.

    types.SimpleNamespace(

        name               = 'SandboxNucleoH7S3L8',
        mcu                = 'STM32H7S3L8H6',
        source_file_paths  = root('''
            ./electrical/SandboxNucleoBoard.c
            ./electrical/system/Startup.S
        '''),

        use_freertos = False,

        main_stack_size = 8192,

        clock_tree = {
            'HSI_ENABLE'    : True,
            'HSI48_ENABLE'  : True,
            'CSI_ENABLE'    : True,
            'PER_CK_SOURCE' : 'HSI_CK',
            'PLL1_P_CK'     : 600_000_000,
            'PLL2_S_CK'     : 200_000_000,
            'CPU_CK'        : 600_000_000,
            'AXI_AHB_CK'    : 300_000_000,
            'APB1_CK'       : 150_000_000,
            'APB2_CK'       : 150_000_000,
            'APB4_CK'       : 150_000_000,
            'APB5_CK'       : 150_000_000,
            'USART3_BAUD'   : STLINK_BAUD,
        },

        gpios = (
            ('led_red'   , 'B7' , 'OUTPUT'    , { 'initlvl' : False       }),
            ('led_yellow', 'D13', 'OUTPUT'    , { 'initlvl' : False       }),
            ('led_green' , 'D10', 'OUTPUT'    , { 'initlvl' : False       }),
            ('stlink_tx' , 'D8' , 'ALTERNATE' , { 'altfunc' : 'USART3_TX' }),
            ('stlink_rx' , 'D9' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX' }),
            ('swdio'     , 'A13', 'RESERVED'  , {                         }),
            ('swclk'     , 'A14', 'RESERVED'  , {                         }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : 'UP'        }),
        ),

        interrupt_priorities = (
            ('USART3', 0),
        ),

        drivers = {
            'UXART' : (
                ('stlink', 'USART3'),
            )
        }

    ),

    types.SimpleNamespace(

        name               = 'SandboxNucleoH533RE',
        mcu                = 'STM32H533RET6',
        source_file_paths  = root('''
            ./electrical/SandboxNucleoBoard.c
            ./electrical/system/Startup.S
        '''),

        use_freertos = False,

        main_stack_size = 8192,

        clock_tree = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1_P_CK'    : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
        },

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False       }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX' }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX' }),
            ('swdio'     , 'A13', 'RESERVED'  , {                         }),
            ('swclk'     , 'A14', 'RESERVED'  , {                         }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : 'UP'        }),
        ),

        interrupt_priorities = (
            ('USART2', 0),
        ),

        drivers = {
            'UXART' : (
                ('stlink', 'USART2'),
            )
        }

    ),

    types.SimpleNamespace(

        name               = 'DemoI2C',
        mcu                = 'STM32H533RET6',
        source_file_paths  = root('''
            ./electrical/DemoI2C.c
            ./electrical/system/Startup.S
        '''),

        use_freertos = False,

        main_stack_size = 8192,

        clock_tree = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1_P_CK'    : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'I2C1_BAUD'    : 100_000,
        },

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                                          }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'     , 'A13', 'RESERVED'  , {                                                            }),
            ('swclk'     , 'A14', 'RESERVED'  , {                                                            }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : 'UP'                                           }),
            ('i2c1_scl'  , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('i2c1_sda'  , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
        ),

        interrupt_priorities = (
            ('USART2' , 0),
            ('I2C1_EV', 1),
            ('I2C1_ER', 1),
        ),

        drivers = {
            'UXART' : (
                ('stlink', 'USART2'),
            ),
            'I2C' : (
                ('primary', 'I2C1'),
            ),
        }

    ),

    types.SimpleNamespace(

        name               = 'DemoTimer',
        mcu                = 'STM32H533RET6',
        source_file_paths  = root('''
            ./electrical/DemoTimer.c
            ./electrical/system/Startup.S
        '''),

        use_freertos = False,

        main_stack_size = 8192,

        clock_tree = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1_P_CK'    : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'I2C1_BAUD'    : 100_000,
        },

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                                          }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'     , 'A13', 'RESERVED'  , {                                                            }),
            ('swclk'     , 'A14', 'RESERVED'  , {                                                            }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : 'UP'                                           }),
            ('i2c1_scl'  , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('i2c1_sda'  , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
        ),

        interrupt_priorities = (
            ('USART2', 0),
        ),

        drivers = {
            'UXART' : (
                ('stlink', 'USART2'),
            ),
        }

    ),

)



################################################################################
#
# Figure out the compiler and linker flags for each firmware target.
#

for target in TARGETS:



    # Flags for both the compiler and linker.

    architecture_flags = '''
        -mcpu=cortex-m7
        -mfloat-abi=hard
    '''



    # Warning configurations.

    enabled_warnings = '''
        error
        all
        extra
        switch-enum
        undef
        fatal-errors
        strict-prototypes
        shadow
        switch-default
    '''

    disabled_warnings = '''
        unused-function
        main
        double-promotion
        conversion
        unused-variable
        unused-parameter
        comment
        unused-but-set-variable
        format-zero-length
        unused-label
    '''


    # Additional search paths for the compiler to search through for #includes.

    include_file_paths = (
        root(BUILD, 'meta'),
        root('./deps/CMSIS_6/CMSIS/Core/Include'),
        root('./deps/FreeRTOS_Kernel/include'),
        root('./deps/printf/src'),
        root('.'),                               # For <deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h> and such.
        root('./electrical/system'),             # For <FreeRTOSConfig.h>.
        MCUS[target.mcu].freertos_port_dir_path, # For <portmacro.h> and such.
    )



    # Additional macro defines.

    defines = [
        ('TARGET_NAME'    , target.name           ),
        ('TARGET_MCU'     , target.mcu            ),
        ('MAIN_STACK_SIZE', target.main_stack_size),
    ]

    for other in TARGETS:
        defines += [
            (f'TARGET_NAME_IS_{other.name}', int(other.name == target.name)),
            (f'TARGET_MCU_IS_{other.mcu}'  , int(other.mcu  == target.mcu )),
        ]



    # The target's final set of compiler flags. @/`Linker Garbage Collection`.

    target.compiler_flags = (
        f'''
            {architecture_flags}
            -O0
            -ggdb3
            -std=gnu2x
            -fmax-errors=1
            -fno-strict-aliasing
            -fno-eliminate-unused-debug-types
            -ffunction-sections
            -fcompare-debug-second
            {'\n'.join(f'-D {name}="{value}"'    for name, value in defines                  )}
            {'\n'.join(f'-W{name}'               for name        in enabled_warnings .split())}
            {'\n'.join(f'-Wno-{name}'            for name        in disabled_warnings.split())}
            {'\n'.join(f'-I "{path.as_posix()}"' for path        in include_file_paths       )}
        '''
    )



    # The target's final set of linker flags. @/`Linker Garbage Collection`.

    target.linker_flags = f'''
        {architecture_flags}
        -nostdlib
        -lgcc
        -lc
        -Xlinker --fatal-warnings
        -Xlinker --gc-sections
    '''



################################################################################
#
# Some helpers for generating code that's specific to a particular target
# or a particular MCU. This is better placed in <Helpers.py>, but a limitation
# of the meta-preprocessor right now is that `TARGETS` and `MCUS` won't be
# accessible there (TODO).
#



def PER_TARGET():

    for target in TARGETS:

        with Meta.enter(f'#if TARGET_NAME_IS_{target.name}'):

            yield target



def PER_MCU():

    for mcu in MCUS:

        with Meta.enter(f'#if TARGET_MCU_IS_{mcu}'):

            yield mcu



################################################################################



# This meta-directive typically contains definitions for things that'll be
# used both in other meta-directives and also in `cli.py`. Stuff like the
# ST-Link baud rate is defined here because the clock-tree is automated by
# a meta-directive in order to configure the UART for the right clock speed
# and also so that `cli.py talk` can open the serial port to the right baud.



# @/`Defining a TARGET`:
#
# A target is essentially a program for a specific microcontroller in mind.
# For this project, we'll have a target for the flight computer, camera
# subsystems, and maybe some other stuff. As of writing, this is what defines
# a target:
#
#     - name                 = Self-explanatory; the unique name of the target.
#
#     - mcu                  = The full commerical part number of an STM32
#                              microcontroller as seen in `MCUS`. Technically,
#                              the last few suffixes are not necessary because
#                              they just indicate the flash size and operating
#                              temperature and stuff, but I think it's a hassle
#                              to deal with different variations of MCU "names",
#                              so just might as well use the whole thing even
#                              if it ends up being redundant later on.
#
#     - source_file_paths    = Source files that the build system will compile each
#                              of and then link all together.
#
#     - use_freertos         = Whether or not to compile with FreeRTOS and set up
#                              the task-scheduler. Typically, we'd disable FreeRTOS
#                              when we want to start off programming in a simpler
#                              environment where we don't have to worry about concurrency
#                              and reentrance of code. Eventually, as the firmware
#                              matures, we'd want to start using a task-scheduler
#                              so we can have multiple systems in the firmware
#                              work together and not have one big state machine
#                              to handle it all.
#
#     - main_stack_size      = The amount of bytes to reserve for the stack of `main`.
#                              This option is more useful when not using FreeRTOS.
#                              If FreeRTOS is enabled, then each task will have their
#                              own stack, the size of which can be individually specified.
#                              Before the task-scheduler can starts, we still go into
#                              `main`, so this option is still used, albeit less
#                              critically.
#                              TODO Look more into how FreeRTOS organizes its memory.
#
#     - clock_tree           = Options relating to configuring the MCU's clock-tree.
#                              The available options right now is pretty undocumented since
#                              it heavily depends upon the implementation of `SYSTEM_PARAMETERIZE`;
#                              things there are still non-comprehensive and quite experimental.
#                              Nonetheless, some of the stuff should be self-explanatory, like
#                              if you want to change the baud rate of a UART peripheral or
#                              something, then it's pretty easy to do right here; but if you
#                              have a lot of questions, then you should probably see
#                              `SYSTEM_PARAMETERIZE` anyways.
#
#     - gpios                = This is where we define the GPIOs of our target; what
#                              input/outputs it has, which pins are being used for what
#                              particular peripherals, and maybe other stuff too like the
#                              slew rate or pull up/down configuration. This table of GPIOs
#                              is very useful, because later on when we make our PCBs, we can
#                              write a meta-directive to verify that the PCB matches our
#                              GPIO table (TODO).
#
#     - interrupt_priorities = This table defines the interrupts that'll need to be configured
#                              in the NVIC. Any time you're writing a driver for a peripheral,
#                              and that peripheral uses interrupts, you should add an entry to
#                              this table. Once you do so, macros will be created to allow the
#                              interrupt to be enabled in the NVIC. It should be noted that
#                              the priority value of interrupts work on a niceless level, so
#                              the lower the number is, the higher priority it actually is.
#
#     - drivers              = Essentially a table describing the drivers that the target
#                              needs and will be using. To see what this setting actually does,
#                              I suggest diving into some of the drivers' source code.
#
# It's also useful to have a "sandbox" target where it's pretty much
# just a demo program for a Nucleo board; some LEDS blinking, maybe
# reacting to button presses, and printing out to serial port. This
# is just so we can easily test things out whenever we're writing
# some new drivers or something.
#
# As of writing, these are the steps to making a new target:
#
#     1. Jump start by copy-pasting an existing target in `TARGETS`,
#        maybe "SandboxNucleoH533RE". Rename the target to something
#        unique, which for our example, let's say "FooBar".
#
#     2. Change the MCU (if needed) and modify `source_file_paths`
#        to have the main C file you wish to compile with.
#        If your new target name is "FooBar", you should probably
#        make a new C file called "FooBar.c". In this C file,
#        you can copy from <SandboxNucleoBoard.c> but remove all
#        the FreeRTOS task stuff; you should have at least `main`.
#
#     3. Recompile; things should work, but this process I described
#        here might've changed because I added a new feature or
#        something to the build process. In the event that I forgot
#        to update this, read the error message and figure out what
#        is missing.




# @/`Linker Garbage Collection`:
#
# The `-ffunction-sections` makes the compiler generate a section
# for every function. This will allow the linker to later on garbage-collect
# any unused functions via `--gc-sections`. This isn't necessarily for
# space-saving reasons, but for letting us compile with libraries without
# necessarily defining all user-side functions until we use a library function
# that'd depend upon it. An example would be `putchar_` for eyalroz's `printf`.
# library
#
# There's a similar thing for data with the flag `-fdata-sections`,
# but if we do this, then we won't be able to reference any variables
# that end up being garbage-collected when we debug. This isn't a big
# deal, but it is annoying when it happens, so we'll skip out on GC'ing
# data and only do functions.
