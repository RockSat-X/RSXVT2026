#meta STLINK_BAUD, TARGETS, MCUS :

from deps.pxd.utils import root
import types



################################################################################################################################
#
# Communication speed with the ST-Link.
#

STLINK_BAUD = 1_000_000



################################################################################################################################
#
# Build directory where all build artifacts will be dumped to.
#

BUILD = root('./build')



################################################################################################################################
#
# Supported microcontrollers.
#

MCUS = {
    'STM32H7S3L8H6' : types.SimpleNamespace(
        cmsis_file_path     = root('./deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h'),
        freertos_file_path  = root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM7/r0p1'),
        freertos_interrupts = {
            'SysTick' : 'xPortSysTickHandler',
            'SVCall'  : 'vPortSVCHandler'    ,
            'PendSV'  : 'xPortPendSVHandler' ,
        },
    ),
    'STM32H533RET6' : types.SimpleNamespace(
        cmsis_file_path     = root('./deps/cmsis-device-h5/Include/stm32h533xx.h'),
        freertos_file_path  = root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        freertos_interrupts = {
            'SysTick' : 'SysTick_Handler',
            'SVCall'  : 'SVC_Handler'    ,
            'PendSV'  : 'PendSV_Handler' ,
        },
    ),
}



################################################################################################################################
#
# Basic description of each of our firmware targets.
#

TARGETS = ( # @/`Defining a TARGET`.

    types.SimpleNamespace(

        name               = 'SandboxNucleoH7S3L8',
        mcu                = 'STM32H7S3L8H6',
        source_file_paths  = root('''
            ./electrical/SandboxNucleoH7S3L8.c
            ./electrical/system/Startup.S
        '''),

        stack_size = 8192, # TODO This might be removed depending on how FreeRTOS works.

        aliases = (
            types.SimpleNamespace(
                moniker    = 'UxART_STLINK',
                actual     = 'USART3',
                terms      = ['{}_BRR_BRR_init'],
                interrupts = ['{}'],
                puts       = [('{}_EN', 'uxart_{UNIT}_enable', { 'UNIT' : 3 })]
            ),
        ),

        clock_tree = {
            'hsi_enable'    : True,
            'hsi48_enable'  : True,
            'csi_enable'    : True,
            'per_ck_source' : 'hsi_ck',
            'pll1_p_ck'     : 600_000_000,
            'pll2_s_ck'     : 200_000_000,
            'cpu_ck'        : 600_000_000,
            'axi_ahb_ck'    : 300_000_000,
            'apb1_ck'       : 150_000_000,
            'apb2_ck'       : 150_000_000,
            'apb4_ck'       : 150_000_000,
            'apb5_ck'       : 150_000_000,
            'usart3_baud'   : STLINK_BAUD,
        },

        gpios = (
            ('led_red'   , 'B7' , 'output'    , { 'initlvl' : False       }),
            ('led_yellow', 'D13', 'output'    , { 'initlvl' : False       }),
            ('led_green' , 'D10', 'output'    , { 'initlvl' : False       }),
            ('jig_tx'    , 'D8' , 'alternate' , { 'altfunc' : 'USART3_TX' }),
            ('jig_rx'    , 'D9' , 'alternate' , { 'altfunc' : 'USART3_RX' }),
            ('swdio'     , 'A13', 'reserved'  , {                         }),
            ('swclk'     , 'A14', 'reserved'  , {                         }),
        ),

        interrupt_priorities = (
            ('USART3', 0),
        ),

    ),

    types.SimpleNamespace(

        name               = 'SandboxNucleoH533RE',
        mcu                = 'STM32H533RET6',
        source_file_paths  = root('''
            ./electrical/SandboxNucleoH533RE.c
            ./electrical/system/Startup.S
        '''),

        stack_size = 8192, # TODO This might be removed depending on how FreeRTOS works.

        aliases = (
            types.SimpleNamespace(
                moniker    = 'UxART_STLINK',
                actual     = 'USART2',
                terms      = ['{}_BRR_BRR_init'],
                interrupts = ['{}'],
                puts       = [('{}_EN', 'uxart_{UNIT}_enable', { 'UNIT' : 2 })]
            ),
        ),

        clock_tree = {
            'hsi_enable'   : True,
            'hsi48_enable' : True,
            'csi_enable'   : True,
            'pll1_p_ck'    : 250_000_000,
            'cpu_ck'       : 250_000_000,
            'apb1_ck'      : 250_000_000,
            'apb2_ck'      : 250_000_000,
            'apb3_ck'      : 250_000_000,
            'usart2_baud'  : STLINK_BAUD,
        },

        gpios = (
            ('led_green' , 'A5' , 'output'    , { 'initlvl' : False       }),
            ('jig_tx'    , 'A2' , 'alternate' , { 'altfunc' : 'USART2_TX' }),
            ('jig_rx'    , 'A3' , 'alternate' , { 'altfunc' : 'USART2_RX' }),
            ('swdio'     , 'A13', 'reserved'  , {                         }),
            ('swclk'     , 'A14', 'reserved'  , {                         }),
            ('button'    , 'C13', 'input'     , { 'pull'    : None        }),
        ),

        interrupt_priorities = (
            ('USART2', 0),
        ),

    ),

)



################################################################################################################################
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
        root('.'),            # For <deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h> and such.
        root('./electrical'), # For <FreeRTOSConfig.h>.
        MCUS[target.mcu].freertos_file_path,
    )



    # Additional macro defines.

    defines = [
        ('TARGET_NAME'         , target.name                                       ),
        ('LINK_stack_size'     , target.stack_size                                 ),
        ('STM32_CMSIS_DEVICE_H', f'<{MCUS[target.mcu].cmsis_file_path.as_posix()}>'),
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



################################################################################################################################



# This meta-directive typically contains definitions for things that'll be used both in other meta-directives and also in `cli.py`.
# Stuff like the ST-Link baud rate is defined here because the clock-tree is automated by a meta-directive in order to
# configure the UART for the right clock speed and also so that `cli.py talk` can open the serial port to the right baud.



# @/`Defining a TARGET`:
#
# A target is essentially a program for a specific microcontroller in mind.
# For this project, we'll have a target for the flight computer, camera subsystems, and maybe some other stuff.
# As of writing, this is what defines a target:
#
#     - name                 = Self-explanatory; the unique name of the target.
#
#     - mcu                  = The full commerical part number of an STM32 microcontroller as seen in `MCUS`.
#                              Technically, the last few suffixes are not necessary because they just indicate the
#                              flash size and operating temperature and stuff, but I think it's a hassle to deal with
#                              different variations of MCU "names", so just might as well use the whole thing even if
#                              it ends up being redundant later on.
#
#     - source_file_paths    = Source files that the build system will compile each of and then link all together.
#
#     - stack_size           = The amount of bytes to reserve for the main stack,
#                              although I think this might be deprecated once I do
#                              more research into FreeRTOS' configurations (TODO).
#
#     - aliases              = The purpose of this field is to make it easy to refer to a peripheral "generically"
#                              by using a custom name like "UxART_STLINK" instead of "USART3".
#                              While this makes it more clear what a particular peripheral is meant to be used for,
#                              its true purpose is to make code that uses this peripheral more applicable to different targets.
#                              In the example of "UxART_STLINK", some targets might have it be "USART3" while others be "UART2".
#                              Rather than reimplement code to use slightly different peripheral names and numberings,
#                              all code can just refer to "UxART_STLINK", and a bunch of macros and global constants will do the
#                              mapping magically. This is a very experimental feature right now, however, so you can just completely
#                              ignore this. I'm planning to make it much simpler to use and less contrived.
#
#     - clock_tree           = Options relating to configuring the MCU's clock-tree.
#                              The available options right now is pretty undocumented since it
#                              heavily depends upon the implementation of `SYSTEM_PARAMETERIZE`;
#                              things there are still non-comprehensive and quite experimental.
#                              Nonetheless, some of the stuff should be self-explanatory, like
#                              if you want to change the baud rate of a UART peripheral or something,
#                              then it's pretty easy to do right here; but if you have a lot of questions,
#                              then you should probably see `SYSTEM_PARAMETERIZE` anyways.
#
#     - gpios                = This is where we define the GPIOs of our target; what input/outputs it has,
#                              which pins are being used for what particular peripherals, and maybe other
#                              stuff too like the slew rate or pull up/down configuration.
#                              This table of GPIOs is very useful, because later on when we make our PCBs,
#                              we can write a meta-directive to verify that the PCB matches our GPIO table (TODO).
#
#     - interrupt_priorities = This table defines the interrupts that'll need to be configured in the NVIC.
#                              Any time you're writing a driver for a peripheral,
#                              and that peripheral uses interrupts, you should add an entry to this table.
#                              Once you do so, macros will be created to allow the interrupt to be enabled
#                              in the NVIC.
#                              It should be noted that the priority value of interrupts work on a niceless level,
#                              so the lower the numbre is, the higher priority it actually is.
#
# It's also useful to have a "sandbox" target where it's pretty much just a demo program for a NUCLEO board;
# some LEDS blinking, maybe reacting to button presses, and printing out to serial port.
# This is just so we can easily test things out whenever we're writing some new drivers or something.



# @/`Linker Garbage Collection`:
#
# The `-ffunction-sections` makes the compiler generate a section for every function.
# This will allow the linker to later on garbage-collect any unused functions via `--gc-sections`.
# This isn't necessarily for space-saving reasons, but for letting us compile with libraries without
# necessarily defining all user-side functions until we use a library function that'd depend upon it.
# An example would be `putchar_` for eyalroz's `printf` library.
#
# There's a similar thing for data with the flag `-fdata-sections`, but if we do this, then we won't be
# able to reference any variables that end up being garbage-collected when we debug. This isn't a big
# deal, but it is annoying when it happens, so we'll skip out on GC'ing data and only do functions.
