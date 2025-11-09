#meta STLINK_BAUD, TARGETS, PER_MCU, PER_TARGET :

import types, collections
from deps.stpy.pxd.utils import root, c_repr
from deps.stpy.mcus      import MCUS



################################################################################



STLINK_BAUD = 1_000_000

BUILD = root('./build')

MCU_SUPPORT = {

    'STM32H7S3L8H6' : {
        'include_paths' : (
            root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM7/r0p1'),
        ),
        'freertos_source_file_paths' : root('''
            ./deps/FreeRTOS_Kernel/tasks.c
            ./deps/FreeRTOS_Kernel/queue.c
            ./deps/FreeRTOS_Kernel/list.c
            ./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM7/r0p1/port.c
        '''),
        'freertos_interrupts' : (
            ('SysTick', None, { 'symbol' : 'xPortSysTickHandler' }),
            ('SVCall' , None, { 'symbol' : 'vPortSVCHandler'     }),
            ('PendSV' , None, { 'symbol' : 'xPortPendSVHandler'  }),
        ),
    },

    'STM32H533RET6' : {
        'include_paths' : (
            root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        ),
        'freertos_source_file_paths' : root('''
            ./deps/FreeRTOS_Kernel/tasks.c
            ./deps/FreeRTOS_Kernel/queue.c
            ./deps/FreeRTOS_Kernel/list.c
            ./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
            ./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
        '''),
        'freertos_interrupts' : (
            ('SysTick', None, { 'symbol' : 'SysTick_Handler' }),
            ('SVCall' , None, { 'symbol' : 'SVC_Handler'     }),
            ('PendSV' , None, { 'symbol' : 'PendSV_Handler'  }),
        ),
    },

    'STM32H533VET6' : {
        'include_paths' : (
            root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        ),
        'freertos_source_file_paths' : root('''
            ./deps/FreeRTOS_Kernel/tasks.c
            ./deps/FreeRTOS_Kernel/queue.c
            ./deps/FreeRTOS_Kernel/list.c
            ./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
            ./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
        '''),
        'freertos_interrupts' : (
            ('SysTick', None, { 'symbol' : 'SysTick_Handler' }),
            ('SVCall' , None, { 'symbol' : 'SVC_Handler'     }),
            ('PendSV' , None, { 'symbol' : 'PendSV_Handler'  }),
        ),
    },

}

TARGETS = (



    ########################################



    types.SimpleNamespace(

        name              = 'SandboxNucleoH7S3L8',
        mcu               = 'STM32H7S3L8H6',
        source_file_paths = root('''
            ./electrical/SandboxNucleoBoard.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_red'   , 'B7' , 'OUTPUT'    , { 'initlvl' : False               }),
            ('led_yellow', 'D13', 'OUTPUT'    , { 'initlvl' : False               }),
            ('led_green' , 'D10', 'OUTPUT'    , { 'initlvl' : False               }),
            ('stlink_tx' , 'D8' , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'         }),
            ('stlink_rx' , 'D9' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'         }),
            ('swdio'     , 'A13', None        , {                                 }),
            ('swclk'     , 'A14', None        , {                                 }),
            ('button'    , 'C13', 'INPUT'     , { 'pull' : 'UP', 'active' : False }),
        ),

        interrupts = (
            ('USART3', 0),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART3',
                'handle'     : 'stlink',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'              : True,
            'HSI48_ENABLE'            : True,
            'CSI_ENABLE'              : True,
            'PERIPHERAL_CLOCK_OPTION' : 'HSI_CK',
            'PLL1P_CK'                : 600_000_000,
            'PLL2S_CK'                : 200_000_000,
            'CPU_CK'                  : 600_000_000,
            'AXI_AHB_CK'              : 300_000_000,
            'APB1_CK'                 : 150_000_000,
            'APB2_CK'                 : 150_000_000,
            'APB4_CK'                 : 150_000_000,
            'APB5_CK'                 : 150_000_000,
            'USART3_BAUD'             : STLINK_BAUD,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'SandboxNucleoH533RE',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/SandboxNucleoBoard.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False              }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'        }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'        }),
            ('swdio'     , 'A13', None        , {                                }),
            ('swclk'     , 'A14', None        , {                                }),
            ('button'    , 'C13', 'INPUT'     , { 'pull' : None, 'active' : True }),
        ),

        interrupts = (
            ('USART2', 0),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1P_CK'     : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoI2C',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoI2C.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green'  , 'A5' , 'OUTPUT'   , { 'initlvl' : False                                          }),
            ('stlink_tx'  , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx'  , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'      , 'A13', None       , {                                                            }),
            ('swclk'      , 'A14', None       , {                                                            }),
            ('button'     , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True                          }),
            ('queen_clock', 'B6' , 'ALTERNATE', { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('queen_data' , 'B7' , 'ALTERNATE', { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
            ('bee_clock'  , 'B10', 'ALTERNATE', { 'altfunc' : 'I2C2_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('bee_data'   , 'B12', 'ALTERNATE', { 'altfunc' : 'I2C2_SDA', 'open_drain' : True, 'pull' : 'UP' }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('I2C1_EV', 1),
            ('I2C1_ER', 1),
            ('I2C2_EV', 2),
            ('I2C2_ER', 2),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'queen',
                'role'       : 'master',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C2',
                'handle'     : 'bee',
                'role'       : 'slave',
                'address'    : 0b_001_1110,
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1P_CK'     : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'I2C1_BAUD'    : 1_000,
            'I2C2_BAUD'    : 1_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoTimer',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoTimer.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                 }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'           }),
            ('swdio'     , 'A13', None        , {                                   }),
            ('swclk'     , 'A14', None        , {                                   }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True }),
            ('OC1'       , 'A8' , 'ALTERNATE' , { 'altfunc' : 'TIM1_CH1'            }),
            ('OC1N'      , 'A7' , 'ALTERNATE' , { 'altfunc' : 'TIM1_CH1N'           }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('TIM1_UP', 1),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'       : True,
            'HSI48_ENABLE'     : True,
            'CSI_ENABLE'       : True,
            'PLL1P_CK'         : 250_000_000,
            'CPU_CK'           : 250_000_000,
            'APB1_CK'          : 250_000_000,
            'APB2_CK'          : 250_000_000,
            'APB3_CK'          : 250_000_000,
            'USART2_BAUD'      : STLINK_BAUD,
            'TIM1_UPDATE_RATE' : 16,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoSPI',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoSPI.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                 }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'           }),
            ('swdio'     , 'A13', None        , {                                   }),
            ('swclk'     , 'A14', None        , {                                   }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True }),
            ('nss'       , 'B1' , 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'            }),
            ('sck'       , 'B2' , 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK'            }),
            ('mosi'      , 'B15', 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI'           }),
            ('miso'      , 'C2' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MISO'           }),
        ),

        interrupts = (
            ('USART2', 0),
            ('SPI2'  , 2),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'SPI',
                'peripheral' : 'SPI2',
                'handle'     : 'primary',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1P_CK'     : 250_000_000,
            'PLL2P_CK'     :   1_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'SPI2_BAUD'    : 3_900,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoTimekeeping',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoTimekeeping.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False              }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'        }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'        }),
            ('swdio'     , 'A13', None        , {                                }),
            ('swclk'     , 'A14', None        , {                                }),
            ('button'    , 'C13', 'INPUT'     , { 'pull' : None, 'active' : True }),
        ),

        interrupts = (
            ('USART2', 0),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM1',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'TIM1_COUNTER_RATE' : 1_000_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoSDMMC',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoSDMMC.c
        '''),

        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                 }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'           }),
            ('swdio'     , 'A13', None        , {                                   }),
            ('swclk'     , 'A14', None        , {                                   }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True }),
            ('sd_cmd'    , 'B2' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'          }),
            ('sd_d0'     , 'B13', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'           }),
            ('sd_d1'     , 'C9' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'           }),
            ('sd_d2'     , 'C10', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'           }),
            ('sd_d3'     , 'C11', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'           }),
            ('sd_ck'     , 'C12', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'           }),
        ),

        interrupts = (
            ('USART2', 0),
            ('SDMMC1', 1),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'SD',
                'peripheral' : 'SDMMC1',
                'handle'     : 'primary',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'          : True,
            'HSI48_ENABLE'        : True,
            'CSI_ENABLE'          : True,
            'PLL1P_CK'            : 250_000_000,
            'PLL1Q_CK'            :  10_000_000,
            'CPU_CK'              : 250_000_000,
            'APB1_CK'             : 250_000_000,
            'APB2_CK'             : 250_000_000,
            'APB3_CK'             : 250_000_000,
            'USART2_BAUD'         : STLINK_BAUD,
            'SDMMC1_TIMEOUT'      : 0.010,
            'SDMMC1_INITIAL_BAUD' :   100_000,
            'SDMMC1_FULL_BAUD'    : 1_000_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'SensorShield',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/SensorShield.c
        '''),
        schematic_file_path = None,

        gpios = (
            ('led_green' , 'A5' , 'OUTPUT'    , { 'initlvl' : False                                          }),
            ('stlink_tx' , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx' , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'     , 'A13', None        , {                                                            }),
            ('swclk'     , 'A14', None        , {                                                            }),
            ('button'    , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True                          }),
            ('i2c1_scl'  , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('i2c1_sda'  , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('I2C1_EV', 1),
            ('I2C1_ER', 1),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'primary',
                'role'       : 'master',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1P_CK'     : 250_000_000,
            'CPU_CK'       : 250_000_000,
            'APB1_CK'      : 250_000_000,
            'APB2_CK'      : 250_000_000,
            'APB3_CK'      : 250_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'I2C1_BAUD'    : 1_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'MainFlightComputer',
        mcu               = 'STM32H533VET6',
        source_file_paths = (),

        schematic_file_path = root('./pcb/MainFlightComputer.kicad_sch'),

        gpios = (
            ('led_channel_red'  , 'E2' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('led_channel_green', 'E3' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('led_channel_blue' , 'E4' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('stlink_tx'        , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'             }),
            ('stlink_rx'        , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'             }),
            ('swdio'            , 'A13', None        , {                                     }),
            ('swclk'            , 'A14', None        , {                                     }),
            ('swo'              , 'B3' , None        , {                                     }),
            ('serial_reset'     , 'D10', 'OUTPUT'    , { 'initlvl' : True                    }),
            ('serial_rx'        , 'D11', 'ALTERNATE' , { 'altfunc' : 'UART4_RX'              }),
            ('serial_tx'        , 'D12', 'ALTERNATE' , { 'altfunc' : 'UART4_TX'              }),
            ('user_button'      , 'E14', 'INPUT'     , { 'pull'    : None, 'active' : True   }),
            ('buzzer'           , 'C6' , 'ALTERNATE' , { 'altfunc' : 'TIM8_CH1'              }),
            ('sd_cmd'           , 'D2' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'            }),
            ('sd_data_0'        , 'C8' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'             }),
            ('sd_data_1'        , 'C9' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'             }),
            ('sd_data_2'        , 'C10', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'             }),
            ('sd_data_3'        , 'C11', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'             }),
            ('sd_clock'         , 'C12', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'             }),
            ('general_purpose_A', 'D0' , None        , {                                     }),
            ('general_purpose_B', 'D1' , None        , {                                     }),
            ('spi_nss_A'        , 'B12', 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'              }),
            ('spi_clock_A'      , 'B10', 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK'              }),
            ('spi_mosi_A'       , 'C3' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI'             }),
            ('spi_miso_A'       , 'C2' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MISO'             }),
            ('spi_ready_A'      , 'D5' , 'ALTERNATE' , { 'altfunc' : 'SPI2_RDY'              }),
            ('spi_nss_B'        , 'B8' , 'ALTERNATE' , { 'altfunc' : 'SPI3_NSS'              }),
            ('spi_clock_B'      , 'B1' , 'ALTERNATE' , { 'altfunc' : 'SPI3_SCK'              }),
            ('spi_mosi_B'       , 'B5' , 'ALTERNATE' , { 'altfunc' : 'SPI3_MOSI'             }),
            ('spi_miso_B'       , 'B0' , 'ALTERNATE' , { 'altfunc' : 'SPI3_MISO'             }),
            ('spi_ready_B'      , 'E0' , 'ALTERNATE' , { 'altfunc' : 'SPI3_RDY'              }),
            ('uart_tx_A'        , 'B14', 'ALTERNATE' , { 'altfunc' : 'USART1_TX'             }),
            ('uart_rx_A'        , 'B15', 'ALTERNATE' , { 'altfunc' : 'USART1_RX'             }),
            ('uart_tx_B'        , 'D8' , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'             }),
            ('uart_rx_B'        , 'D9' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'             }),
            ('i2c_data_A'       , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA'              }),
            ('i2c_clock_A'      , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL'              }),
            ('i2c_data_B'       , 'D7' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SDA'              }),
            ('i2c_clock_B'      , 'D6' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SCL'              }),
        ),

        interrupts = None,

        drivers = (),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = None,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoTMC2209',
        mcu               = 'STM32H533RET6',
        source_file_paths = root('''
            ./electrical/DemoTMC2209.c
        '''),
        schematic_file_path = None,

        gpios = (
            ('led_green'       , 'A5' , 'OUTPUT'    , { 'initlvl' : False                 }),
            ('stlink_tx'       , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx'       , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'           }),
            ('swdio'           , 'A13', None        , {                                   }),
            ('swclk'           , 'A14', None        , {                                   }),
            ('button'          , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True }),
            ('driver_direction', 'C3' , 'OUTPUT'    , { 'initlvl' : True                  }),
            ('driver_step'     , 'C2' , 'OUTPUT'    , { 'initlvl' : True                  }),
            ('driver_enable'   , 'B0' , 'OUTPUT'    , { 'initlvl' : True                  }),
        ),

        interrupts = (
            ('USART2' , 0),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM1',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'TIM1_COUNTER_RATE' : 1_000_000,
        },

    ),



    ########################################



)



################################################################################



if duplicate_names := [
    name
    for name, count in collections.Counter(
        target.name
        for target in TARGETS
    ).items()
    if count >= 2
]:

    duplicate_name = duplicate_names[0]

    raise RuntimeError(
        'There are multiple targets by the '
        f'name of {repr(duplicate_name)}!'
    )



for target in TARGETS:




    if target.interrupts is None:

        # Absolutely no interrupts!

        target.interrupts = ()

    else:

        # Some additional interrupts.

        target.interrupts = (
            ('Reset'     , None),
            ('BusFault'  , None),
            ('UsageFault', None),
            *(MCU_SUPPORT[target.mcu]['freertos_interrupts'] if target.use_freertos else ()),
            *target.interrupts
        )



    # Some additional source files.

    target.source_file_paths = (
        *(MCU_SUPPORT[target.mcu]['freertos_source_file_paths'] if target.use_freertos else ()),
        *target.source_file_paths
    )



    # Some include-directive search paths.

    include_paths = (
        root('./electrical/meta'),
        root('./deps/CMSIS_6/CMSIS/Core/Include'),
        root('./deps/FreeRTOS_Kernel/include'),
        root('./deps/printf/src'),
        root('.'),
        root('./electrical'),
        *MCU_SUPPORT[target.mcu]['include_paths'],
    )



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



    # Additional macro defines.

    defines = [
        ('TARGET_NAME'         , target.name           ),
        ('TARGET_MCU'          , target.mcu            ),
        ('TARGET_USES_FREERTOS', target.use_freertos   ),
        ('MAIN_STACK_SIZE'     , target.main_stack_size),
    ]

    for other_target in TARGETS:
        defines += [
            (
                f'TARGET_NAME_IS_{other_target.name}',
                int(other_target.name == target.name)
            ),
        ]

    for other_mcu in MCU_SUPPORT:
        defines += [
            (
                f'TARGET_MCU_IS_{other_mcu}',
                int(other_mcu == target.mcu)
            ),
        ]



    # The target's final set of compiler flags. @/`Linker Garbage Collection`.

    target.compiler_flags = (
        f'''
            {architecture_flags}
            -O0
            -ggdb3
            -std=gnu2x
            -pipe
            -fmax-errors=1
            -fno-strict-aliasing
            -fno-eliminate-unused-debug-types
            -ffunction-sections
            -fcompare-debug-second
            {'\n'.join(f'-D {name}="{c_repr(value)}"' for name, value in defines                  )}
            {'\n'.join(f'-W{name}'                    for name        in enabled_warnings .split())}
            {'\n'.join(f'-Wno-{name}'                 for name        in disabled_warnings.split())}
            {'\n'.join(f'-I "{path.as_posix()}"'      for path        in include_paths            )}
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



def PER_TARGET():

    for target in TARGETS:

        with Meta.enter(f'#if TARGET_NAME_IS_{target.name}'):

            yield target



def PER_MCU():

    for mcu in MCUS:

        if mcu in MCU_SUPPORT:

            with Meta.enter(f'#if TARGET_MCU_IS_{mcu}'):

                yield mcu



################################################################################



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
