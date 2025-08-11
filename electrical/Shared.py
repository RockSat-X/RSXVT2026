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

TARGETS = (

    types.SimpleNamespace(

        name               = 'SandboxNucleoH7S3L8',
        mcu                = 'STM32H7S3L8H6',
        source_file_paths  = root('''
            ./electrical/SandboxNucleoH7S3L8.c
            ./electrical/system/Startup.S
        '''),

        stack_size = 8192, # TODO This might be removed depending on how FreeRTOS works.

        aliases = {
            'UxART_STLINK' : 'USART3',
        },

        clock_tree = {
            'hsi_enable'          : True,
            'hsi48_enable'        : True,
            'csi_enable'          : True,
            'per_ck_source'       : 'hsi_ck',
            'pll1_p_ck'           : 600_000_000,
            'pll2_s_ck'           : 200_000_000,
            'cpu_ck'              : 600_000_000,
            'axi_ahb_ck'          : 300_000_000,
            'apb1_ck'             : 150_000_000,
            'apb2_ck'             : 150_000_000,
            'apb4_ck'             : 150_000_000,
            'apb5_ck'             : 150_000_000,
            'usart3_baud'         : STLINK_BAUD,
            '{UxART_STLINK}_baud' : STLINK_BAUD
        },

        gpios = (
            ('led_red'   , 'B7' , 'output'    , { 'initlvl' : False       }),
            ('led_yellow', 'D13', 'output'    , { 'initlvl' : False       }),
            ('led_green' , 'D10', 'output'    , { 'initlvl' : False       }),
            ('jig_tx'    , 'D8' , 'alternate' , { 'altfunc' : 'USART3_TX' }), # TODO: '{UxART_STLINK}_TX'
            ('jig_rx'    , 'D9' , 'alternate' , { 'altfunc' : 'USART3_RX' }), # TODO: '{UxART_STLINK}_RX'
            ('swdio'     , 'A13', 'reserved'  , {                         }),
            ('swclk'     , 'A14', 'reserved'  , {                         }),
        ),

        interrupt_priorities = (
            ('USART3', 0), # TODO: '{UxART_STLINK}'
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

        aliases = {
            'UxART_STLINK' : 'USART2',
        },

        clock_tree = {
            'hsi_enable'          : True,
            'hsi48_enable'        : True,
            'csi_enable'          : True,
            'pll1_p_ck'           : 250_000_000,
            'cpu_ck'              : 250_000_000,
            'apb1_ck'             : 250_000_000,
            'apb2_ck'             : 250_000_000,
            'apb3_ck'             : 250_000_000,
            'usart2_baud'         : STLINK_BAUD,
            '{UxART_STLINK}_baud' : STLINK_BAUD
        },

        gpios = (
            ('led_green' , 'A5' , 'output'    , { 'initlvl' : False       }),
            ('jig_tx'    , 'A2' , 'alternate' , { 'altfunc' : 'USART2_TX' }), # TODO: '{UxART_STLINK}_TX'
            ('jig_rx'    , 'A3' , 'alternate' , { 'altfunc' : 'USART2_RX' }), # TODO: '{UxART_STLINK}_RX'
            ('swdio'     , 'A13', 'reserved'  , {                         }),
            ('swclk'     , 'A14', 'reserved'  , {                         }),
            ('button'    , 'C13', 'input'     , { 'pull'    : None        }),
        ),

        interrupt_priorities = (
            ('USART2', 0), # TODO: '{UxART_STLINK}'
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
        ('TARGET_NAME'         , target.name                            ),
        ('LINK_stack_size'     , target.stack_size                      ),
        ('STM32_CMSIS_DEVICE_H', f'<{MCUS[target.mcu].cmsis_file_path}>'),
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
            -std=gnu23
            -fmax-errors=1
            -fno-strict-aliasing
            -fno-eliminate-unused-debug-types
            -ffunction-sections
            -fcompare-debug-second
            {'\n'.join(f'-D {name}={repr(value)}' for name, value in defines                  )}
            {'\n'.join(f'-W{name}'                for name        in enabled_warnings .split())}
            {'\n'.join(f'-Wno-{name}'             for name        in disabled_warnings.split())}
            {'\n'.join(f'-I {repr(str(path))}'    for path        in include_file_paths       )}
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
