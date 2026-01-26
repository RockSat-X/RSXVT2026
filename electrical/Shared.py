#meta global STLINK_BAUD, TARGETS, PER_MCU, PER_TARGET
#meta global OVCAM_DEFAULT_RESOLUTION
#meta global TV_WRITE_BYTE, TV_TOKEN, OVCAM_JPEG_CTRL3_FIELDS

import types, collections
import deps.stpy.pxd.pxd as pxd
from deps.stpy.mcus import MCUS



################################################################################



OVCAM_DEFAULT_RESOLUTION = (800, 480)

OVCAM_RESOLUTIONS = (
    (160, 120),
    (320, 240),
    (480, 272),
    (640, 480),
    (800, 480),
)

OVCAM_JPEG_CTRL3_FIELDS = pxd.SimpleNamespaceTable(
    ('description'                   , 'default', 'configurable'),
    ('Input shift 128 select for Y.' , True     , True          ),
    ('Input shift 128 select for C.' , True     , True          ),
    ('Enable rounding for Y.'        , True     , True          ),
    ('Enable rounding for C.'        , True     , True          ),
    ('Enable Huffman table output.'  , True     , False         ), # Breaking change.
    ('Enable zero stuffing.'         , True     , False         ), # Breaking change.
    ('Enable MPEG.'                  , False    , False         ), # Doesn't seem to do anything.
    ('Use SRAM QT instead of ROM QT.', False    , True          ),
)

PRE_ISP_TEST_SETTING_FIELDS = pxd.SimpleNamespaceTable(
    ('description'              , 'options'),
    ('PRE-ISP test type.'       , ('Color bar', 'Random data', 'Square data', 'Black image')),
    ('PRE-ISP test bar style.'  , ('Standard 8 color bar', 'Gradual change at vertical mode 1', 'Gradual change at horizontal', 'Gradual change at vertical mode 2')),
    ('Black and white squares.' , bool),
    ('Transparent PRE-ISP test.', bool),
    ('Rolling PRE-ISP test.'    , bool),
    ('Enable PRE-ISP test.'     , bool),
)

TV_WRITE_BYTE = 0x01

TV_TOKEN = types.SimpleNamespace(
    START = b'<TV>',
    END   = b'</TV>',
)

STLINK_BAUD = 1_000_000

MCU_SUPPORT = {

    'STM32H533RET6' : {
        'cpu' : 'cortex-m33',
        'include_paths' : (
            pxd.make_main_relative_path('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        ),
        'freertos_interrupts' : (
            ('SysTick', None, { 'symbol' : 'SysTick_Handler' }),
            ('SVCall' , None, { 'symbol' : 'SVC_Handler'     }),
            ('PendSV' , None, { 'symbol' : 'PendSV_Handler'  }),
        ),
    },

    'STM32H533VET6' : {
        'cpu' : 'cortex-m33',
        'include_paths' : (
            pxd.make_main_relative_path('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure'),
        ),
        'freertos_interrupts' : (
            ('SysTick', None, { 'symbol' : 'SysTick_Handler' }),
            ('SVCall' , None, { 'symbol' : 'SVC_Handler'     }),
            ('PendSV' , None, { 'symbol' : 'PendSV_Handler'  }),
        ),
    },

}

TARGETS = ( # @/`Defining Targets`.



    ########################################



    types.SimpleNamespace(

        name              = 'SandboxNucleoH533RE',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/SandboxNucleoBoard.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green', 'A5' , 'OUTPUT'    , { 'initlvl' : False              }),
            ('stlink_tx', 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'        }),
            ('stlink_rx', 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'        }),
            ('swdio'    , 'A13', None        , {                                }),
            ('swclk'    , 'A14', None        , {                                }),
            ('button'   , 'C13', 'INPUT'     , { 'pull' : None, 'active' : True }),
            ('debug'    , 'C10', 'OUTPUT'    , { 'initlvl' : False              }),
        ),

        interrupts = (
            ('USART2', 0),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoI2C.c'),
        ),

        kicad_project = None,

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
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'queen',
                'role'       : 'master_blocking',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoTimer.c'),
        ),

        kicad_project = None,

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
                'mode'       : 'full_duplex',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoSPI.c'),
        ),

        kicad_project = None,

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
                'mode'       : 'full_duplex',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoTimekeeping.c'),
        ),

        kicad_project = None,

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
                'mode'       : 'full_duplex',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoSDMMC.c'),
        ),

        kicad_project = None,

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
                'mode'       : 'full_duplex',
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
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/SensorShield.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'         , 'A5' , 'OUTPUT'    , { 'initlvl' : False                                          }),
            ('stlink_tx'         , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx'         , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'             , 'A13', None        , {                                                            }),
            ('swclk'             , 'A14', None        , {                                                            }),
            ('button'            , 'C13', 'INPUT'     , { 'pull'    : None      , 'active'     : True                }),
            ('lis2mdl_scl'       , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('lis2mdl_sda'       , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
            ('lis2mdl_data_ready', 'B1' , 'INPUT'     , { 'pull'    : None      , 'interrupt'  : 'RISING'            }),
            ('debug'             , 'C12', 'OUTPUT'    , { 'initlvl' : False                                          }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('I2C1_EV', 1),
            ('I2C1_ER', 1),
            ('EXTI1'  , 2),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'primary',
                'role'       : 'master_callback',
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
            'I2C1_BAUD'    : 200_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'MainFlightComputer',
        mcu               = 'STM32H533VET6',
        source_file_paths = (),

        kicad_project = 'MainFlightComputer',

        gpios = (
            ('led_channel_red'                , 'E2' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('led_channel_green'              , 'E3' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('led_channel_blue'               , 'E4' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False }),
            ('stlink_tx'                      , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'             }),
            ('stlink_rx'                      , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'             }),
            ('swdio'                          , 'A13', None        , {                                     }),
            ('swclk'                          , 'A14', None        , {                                     }),
            ('swo'                            , 'B3' , None        , {                                     }),
            ('serial_reset'                   , 'D10', 'OUTPUT'    , { 'initlvl' : True                    }),
            ('serial_rx'                      , 'D11', 'ALTERNATE' , { 'altfunc' : 'UART4_RX'              }),
            ('serial_tx'                      , 'D12', 'ALTERNATE' , { 'altfunc' : 'UART4_TX'              }),
            ('user_button'                    , 'E14', 'INPUT'     , { 'pull'    : None, 'active' : True   }),
            ('logic_gse_1'                    , 'A7' , 'INPUT'     , { 'pull'    : None                    }),
            ('logic_timer_event_1'            , 'A8' , 'INPUT'     , { 'pull'    : None                    }),
            ('logic_timer_event_x'            , 'A9' , 'INPUT'     , { 'pull'    : None                    }),
            ('buzzer'                         , 'C6' , 'ALTERNATE' , { 'altfunc' : 'TIM8_CH1'              }),
            ('sd_cmd'                         , 'D2' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'            }),
            ('sd_data_0'                      , 'C8' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'             }),
            ('sd_data_1'                      , 'C9' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'             }),
            ('sd_data_2'                      , 'C10', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'             }),
            ('sd_data_3'                      , 'C11', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'             }),
            ('sd_clock'                       , 'C12', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'             }),
            ('esp32_spi_nss'                  , 'B12', 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'              }),
            ('esp32_spi_clock'                , 'B10', 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK'              }),
            ('esp32_spi_mosi'                 , 'C3' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI'             }),
            ('esp32_spi_miso'                 , 'C2' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MISO'             }),
            ('esp32_spi_ready'                , 'D5' , 'ALTERNATE' , { 'altfunc' : 'SPI2_RDY'              }),
            ('spi_nss_B'                      , 'A4' , 'ALTERNATE' , { 'altfunc' : 'SPI1_NSS'              }),
            ('spi_clock_B'                    , 'A5' , 'ALTERNATE' , { 'altfunc' : 'SPI1_SCK'              }),
            ('spi_mosi_B'                     , 'B5' , 'ALTERNATE' , { 'altfunc' : 'SPI1_MOSI'             }),
            ('spi_miso_B'                     , 'A6' , 'ALTERNATE' , { 'altfunc' : 'SPI1_MISO'             }),
            ('spi_ready_B'                    , 'B2' , 'ALTERNATE' , { 'altfunc' : 'SPI1_RDY'              }),
            ('esp32_reset'                    , 'D0' , 'OUTPUT'    , { 'initlvl' : False                   }),
            ('vehicle_interface_i2c_data'     , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA'              }),
            ('vehicle_interface_i2c_clock'    , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL'              }),
            ('flight_computer_debug_i2c_data' , 'D7' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SDA'              }),
            ('flight_computer_debug_i2c_clock', 'D6' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SCL'              }),
            ('uart_tx_A'                      , 'B14', 'ALTERNATE' , { 'altfunc' : 'USART1_TX'             }),
            ('uart_rx_A'                      , 'B15', 'ALTERNATE' , { 'altfunc' : 'USART1_RX'             }),
            ('uart_tx_B'                      , 'D8' , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'             }),
            ('uart_rx_B'                      , 'B1' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'             }),
            ('solarboard_analog_A'            , 'D14', None        , {                                     }),
            ('solarboard_analog_B'            , 'D15', None        , {                                     }),
            ('general_purpose_B'              , 'E15', None        , {                                     }),
            ('general_purpose_C'              , 'E5' , None        , {                                     }),
            ('general_purpose_D'              , 'E6' , None        , {                                     }),
            ('general_purpose_E'              , 'E7' , None        , {                                     }),
            ('general_purpose_F'              , 'E8' , None        , {                                     }),
            ('general_purpose_G'              , 'E9' , None        , {                                     }),
            ('general_purpose_H'              , 'E10', None        , {                                     }),
            ('general_purpose_I'              , 'E11', None        , {                                     }),
            ('general_purpose_J'              , 'E12', None        , {                                     }),
            ('general_purpose_K'              , 'E13', None        , {                                     }),
        ),

        interrupts = None,

        drivers = (),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = None,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'VehicleFlightComputer',
        mcu               = 'STM32H533VET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/VehicleFlightComputer.c'),
        ),

        kicad_project = 'VehicleFlightComputer',

        gpios = (
            ('led_channel_red'            , 'C0'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False     }),
            ('led_channel_green'          , 'C1'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False     }),
            ('led_channel_blue'           , 'A0'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False     }),
            ('stlink_tx'                  , 'A2'  , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                 }),
            ('stlink_rx'                  , 'A3'  , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                 }),
            ('swdio'                      , 'A13' , None        , {                                         }),
            ('swclk'                      , 'A14' , None        , {                                         }),
            ('swo'                        , 'B3'  , None        , {                                         }),
            ('serial_reset'               , 'E10' , 'OUTPUT'    , { 'initlvl' : True                        }),
            ('serial_rx'                  , 'D11' , 'ALTERNATE' , { 'altfunc' : 'UART4_RX'                  }),
            ('serial_tx'                  , 'D12' , 'ALTERNATE' , { 'altfunc' : 'UART4_TX'                  }),
            ('buzzer'                     , 'A5'  , 'ALTERNATE' , { 'altfunc' : 'TIM8_CH1N'                 }),
            ('sd_cmd'                     , 'D2'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD', 'pull' : 'UP' }),
            ('sd_data_0'                  , 'C8'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0' , 'pull' : 'UP' }),
            ('sd_data_1'                  , 'C9'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'                 }),
            ('sd_data_2'                  , 'C10' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'                 }),
            ('sd_data_3'                  , 'C11' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'                 }),
            ('sd_clock'                   , 'C12' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'                 }),
            ('battery_allowed'            , 'D0'  , 'OUTPUT'    , { 'initlvl' : False                       }),
            ('external_detected'          , 'D1'  , None        , {                                         }),
            ('openmv_spi_nss'             , 'B12' , 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'                  }),
            ('openmv_spi_clock'           , 'B10' , 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK'                  }),
            ('openmv_spi_mosi'            , 'C3'  , 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI'                 }),
            ('openmv_spi_miso'            , 'C2'  , 'ALTERNATE' , { 'altfunc' : 'SPI2_MISO'                 }),
            ('openmv_spi_ready'           , 'D5'  , 'ALTERNATE' , { 'altfunc' : 'SPI2_RDY'                  }),
            ('openmv_reset'               , 'B2'  , 'OUTPUT'    , { 'initlvl' : False                       }),
            ('esp32_spi_nss'              , 'B8'  , 'ALTERNATE' , { 'altfunc' : 'SPI3_NSS'                  }),
            ('esp32_spi_clock'            , 'B1'  , 'ALTERNATE' , { 'altfunc' : 'SPI3_SCK'                  }),
            ('esp32_spi_mosi'             , 'B5'  , 'ALTERNATE' , { 'altfunc' : 'SPI3_MOSI'                 }),
            ('esp32_spi_miso'             , 'B0'  , 'ALTERNATE' , { 'altfunc' : 'SPI3_MISO'                 }),
            ('esp32_spi_ready'            , 'E0'  , 'ALTERNATE' , { 'altfunc' : 'SPI3_RDY'                  }),
            ('esp32_reset'                , 'E8'  , 'OUTPUT'    , { 'initlvl' : False                       }),
            ('motor_uart_tx'              , 'B14' , 'ALTERNATE' , { 'altfunc' : 'USART1_TX'                 }),
            ('motor_uart_rx'              , 'B15' , None        , {                                         }),
            ('sensor_i2c_data'            , 'B7'  , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA'                  }),
            ('sensor_i2c_clock'           , 'B6'  , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL'                  }),
            ('vehicle_interface_i2c_data' , 'D7'  , 'ALTERNATE' , { 'altfunc' : 'I2C3_SDA'                  }),
            ('vehicle_interface_i2c_clock', 'D6'  , 'ALTERNATE' , { 'altfunc' : 'I2C3_SCL'                  }),
            ('motor_enable'               , 'A4'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False     }),
            ('motor_step_x'               , 'E9'  , None        , { 'altfunc' : 'TIM1_CH1'                  }),
            ('motor_step_y'               , 'C6'  , None        , { 'altfunc' : 'TIM8_CH1'                  }),
            ('motor_step_z'               , 'E5'  , None        , { 'altfunc' : 'TIM15_CH1'                 }),
            ('motor_direction_x'          , 'A9'  , None        , { 'initlvl' : False                       }),
            ('motor_direction_y'          , 'A8'  , None        , { 'initlvl' : False                       }),
            ('motor_direction_z'          , 'A10' , None        , { 'initlvl' : False                       }),
            ('lsm6dsv32x_interrupt_1'     , 'D13' , None        , {                                         }),
            ('lsm6dsv32x_interrupt_2'     , 'D14' , None        , {                                         }),
            ('lsm6dsv32x_chip_select'     , 'D15' , 'OUTPUT'    , { 'initlvl': True                         }),
            ('lis2mdl_data_ready'         , 'B4'  , None        , {                                         }),
            ('lis2mdl_chip_select'        , 'C15' , None        , {                                         }),
            ('vn100_uart_tx'              , 'D8'  , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'                 }),
            ('vn100_uart_rx'              , 'D9'  , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'                 }),
            ('vn100_tare_restore'         , 'C13' , None        , {                                         }),
            ('vn100_sync_out'             , 'H0'  , None        , {                                         }),
            ('vn100_sync_in'              , 'C5'  , None        , {                                         }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('SDMMC1' , 1),
            ('USART1' , 1),
            ('TIM1_UP', 2),
            ('TIM8_UP', 3),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'SD',
                'peripheral' : 'SDMMC1',
                'handle'     : 'primary',
            },
            {
                'type'       : 'UXART',
                'peripheral' : 'USART1',
                'handle'     : 'stepper_uart',
                'mode'       : 'half_duplex',
            },
            {
                'type'                         : 'Stepper',
                'uxart_handle'                 : 'stepper_uart',
                'enable_gpio'                  : 'motor_enable',
                'timer_peripheral'             : 'TIM1',
                'timer_update_event_interrupt' : 'TIM1_UP',
                'instances'                    : (
                    ('axis_x', 0),
                    ('axis_y', 1),
                    ('axis_z', 2),
                ),
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos    = True,
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
            'USART1_BAUD'         :   200_000,
            'TIM1_UPDATE_RATE'    : 1 / 0.001,
            'TIM2_COUNTER_RATE'   : 1_000_000,
            'TIM8_COUNTER_RATE'   : 1_000_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'MainCameraSystem',
        mcu               = 'STM32H533VET6',
        source_file_paths = (),

        kicad_project = 'MainCameraSystem',

        gpios = (
            ('led_channel_red'  , 'E7' , 'OUTPUT'   , { 'initlvl' : False, 'active' : False          }),
            ('led_channel_green', 'B1' , 'OUTPUT'   , { 'initlvl' : False, 'active' : False          }),
            ('led_channel_blue' , 'B2' , 'OUTPUT'   , { 'initlvl' : False, 'active' : False          }),
            ('stlink_tx'        , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                      }),
            ('stlink_rx'        , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                      }),
            ('swdio'            , 'A13', None       , {                                              }),
            ('swclk'            , 'A14', None       , {                                              }),
            ('swo'              , 'B3' , None       , {                                              }),
            ('debug_testpoint_a', 'D7' , None       , {                                              }),
            ('debug_testpoint_b', 'A9' , None       , {                                              }),
            ('sd_cmd'           , 'D2' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_CMD'                     }),
            ('sd_data_0'        , 'C8' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D0'                      }),
            ('sd_data_1'        , 'C9' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D1'                      }),
            ('sd_data_2'        , 'C10', 'ALTERNATE', { 'altfunc' : 'SDMMC1_D2'                      }),
            ('sd_data_3'        , 'C11', 'ALTERNATE', { 'altfunc' : 'SDMMC1_D3'                      }),
            ('sd_clock'         , 'C12', 'ALTERNATE', { 'altfunc' : 'SDMMC1_CK'                      }),
            ('ovcam_y2'         , 'C6' , 'ALTERNATE', { 'altfunc' : 'DCMI_D0'                        }),
            ('ovcam_y3'         , 'C7' , 'ALTERNATE', { 'altfunc' : 'DCMI_D1'                        }),
            ('ovcam_y4'         , 'B15', 'ALTERNATE', { 'altfunc' : 'DCMI_D2'                        }),
            ('ovcam_y5'         , 'E2' , 'ALTERNATE', { 'altfunc' : 'DCMI_D3'                        }),
            ('ovcam_y6'         , 'E4' , 'ALTERNATE', { 'altfunc' : 'DCMI_D4'                        }),
            ('ovcam_y7'         , 'B6' , 'ALTERNATE', { 'altfunc' : 'DCMI_D5'                        }),
            ('ovcam_y8'         , 'B8' , 'ALTERNATE', { 'altfunc' : 'DCMI_D6'                        }),
            ('ovcam_y9'         , 'B4' , 'ALTERNATE', { 'altfunc' : 'DCMI_D7'                        }),
            ('ovcam_pixel_clock', 'A6' , 'ALTERNATE', { 'altfunc' : 'DCMI_PIXCLK'                    }),
            ('ovcam_vsync'      , 'B7' , 'ALTERNATE', { 'altfunc' : 'DCMI_VSYNC'                     }),
            ('ovcam_hsync'      , 'A4' , 'ALTERNATE', { 'altfunc' : 'DCMI_HSYNC'                     }),
            ('ovcam_power_down' , 'A5' , 'OUTPUT'   , { 'initlvl' : True      , 'active'     : True, }),
            ('ovcam_reset'      , 'E6' , 'OUTPUT'   , { 'initlvl' : True      , 'active'     : False }),
            ('ovcam_i2c_clock'  , 'B10', 'ALTERNATE', { 'altfunc' : 'I2C2_SCL', 'open_drain' : True  }),
            ('ovcam_i2c_data'   , 'B12', 'ALTERNATE', { 'altfunc' : 'I2C2_SDA', 'open_drain' : True  }),
        ),

        interrupts = None,

        drivers = (),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = None,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoStepper',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoStepper.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'       , 'A5' , 'OUTPUT'    , { 'initlvl' : False                            }),
            ('stlink_tx'       , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                      }),
            ('stlink_rx'       , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                      }),
            ('swdio'           , 'A13', None        , {                                              }),
            ('swclk'           , 'A14', None        , {                                              }),
            ('button'          , 'C13', 'INPUT'     , { 'pull'    : None, 'active' : True            }),
            ('driver_direction', 'C3' , 'OUTPUT'    , { 'initlvl' : True                             }),
            ('driver_step'     , 'A9' , 'ALTERNATE' , { 'altfunc' : 'TIM1_CH2'                       }),
            ('driver_enable'   , 'B0' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False          }),
            ('driver_uart'     , 'B10', 'ALTERNATE' , { 'altfunc' : 'USART3_TX', 'open_drain' : True }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('USART3' , 1),
            ('TIM1_UP', 2),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'UXART',
                'peripheral' : 'USART3',
                'handle'     : 'stepper_uart',
                'mode'       : 'half_duplex',
            },
            {
                'type'                         : 'Stepper',
                'uxart_handle'                 : 'stepper_uart',
                'enable_gpio'                  : 'driver_enable',
                'timer_peripheral'             : 'TIM1',
                'timer_update_event_interrupt' : 'TIM1_UP',
                'instances'                    : (
                    ('primary', 0),
                ),
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
            'USART3_BAUD'      : 200_000,
            'TIM1_UPDATE_RATE' : 1 / 0.001,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoOVCAM',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoOVCAM.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'        , 'A5' , 'OUTPUT'   , { 'initlvl' : False                                          }),
            ('stlink_tx'        , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx'        , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'            , 'A13', None       , {                                                            }),
            ('swclk'            , 'A14', None       , {                                                            }),
            ('button'           , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True                          }),
            ('ovcam_y2'         , 'C6' , 'ALTERNATE', { 'altfunc' : 'DCMI_D0'                                      }),
            ('ovcam_y3'         , 'C7' , 'ALTERNATE', { 'altfunc' : 'DCMI_D1'                                      }),
            ('ovcam_y4'         , 'B15', 'ALTERNATE', { 'altfunc' : 'DCMI_D2'                                      }),
            ('ovcam_y5'         , 'C9' , 'ALTERNATE', { 'altfunc' : 'DCMI_D3'                                      }),
            ('ovcam_y6'         , 'C11', 'ALTERNATE', { 'altfunc' : 'DCMI_D4'                                      }),
            ('ovcam_y7'         , 'B6' , 'ALTERNATE', { 'altfunc' : 'DCMI_D5'                                      }),
            ('ovcam_y8'         , 'B8' , 'ALTERNATE', { 'altfunc' : 'DCMI_D6'                                      }),
            ('ovcam_y9'         , 'B4' , 'ALTERNATE', { 'altfunc' : 'DCMI_D7'                                      }),
            ('ovcam_pixel_clock', 'A6' , 'ALTERNATE', { 'altfunc' : 'DCMI_PIXCLK'                                  }),
            ('ovcam_vsync'      , 'B7' , 'ALTERNATE', { 'altfunc' : 'DCMI_VSYNC'                                   }),
            ('ovcam_hsync'      , 'A4' , 'ALTERNATE', { 'altfunc' : 'DCMI_HSYNC'                                   }), # Note that SB22 should be shorted, SB3 and SB7 open.
            ('ovcam_power_down' , 'A10', 'OUTPUT'   , { 'initlvl' : True      , 'active'     : True,               }),
            ('ovcam_reset'      , 'C5' , 'OUTPUT'   , { 'initlvl' : True      , 'active'     : False,              }),
            ('ovcam_i2c_clock'  , 'B10', 'ALTERNATE', { 'altfunc' : 'I2C2_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('ovcam_i2c_data'   , 'B12', 'ALTERNATE', { 'altfunc' : 'I2C2_SDA', 'open_drain' : True, 'pull' : 'UP' }),
        ),

        interrupts = (
            ('USART2'         , 0),
            ('GPDMA1_Channel7', 1),
            ('DCMI_PSSI'      , 2),
            ('I2C2_EV'        , 3),
            ('I2C2_ER'        , 3),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C2',
                'handle'     : 'ovcam_sccb',
                'role'       : 'master_blocking',
            },
        ),

        use_freertos    = False,
        main_stack_size = 8192,
        schema          = {
            'HSI_ENABLE'   : True,
            'HSI48_ENABLE' : True,
            'CSI_ENABLE'   : True,
            'PLL1P_CK'     : 240_000_000,
            'CPU_CK'       : 240_000_000,
            'APB1_CK'      : 240_000_000,
            'APB2_CK'      : 240_000_000,
            'APB3_CK'      : 240_000_000,
            'USART2_BAUD'  : STLINK_BAUD,
            'I2C2_BAUD'    : 10_000,
        },

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'TestESP32s',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/TestESP32s.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green', 'A5' , 'OUTPUT'    , { 'initlvl' : False              }),
            ('stlink_tx', 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'        }),
            ('stlink_rx', 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'        }),
            ('swdio'    , 'A13', None        , {                                }),
            ('swclk'    , 'A14', None        , {                                }),
            ('button'   , 'C13', 'INPUT'     , { 'pull' : None, 'active' : True }),
            ('esp32_tx' , 'B10', 'ALTERNATE' , { 'altfunc' : 'USART3_TX'        }),
            ('esp32_rx' , 'C4' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'        }),
            ('debug'    , 'C2' , 'OUTPUT'    , { 'initlvl' : False              }),
        ),

        interrupts = (
            ('USART2', 0),
            ('USART3', 1),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'UXART',
                'peripheral' : 'USART3',
                'handle'     : 'esp32',
                'mode'       : 'full_duplex',
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
            'APB2_CK'           : 250_000_000 / 8,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'USART3_BAUD'       : 400_000,
            'TIM1_COUNTER_RATE' : 1_000,
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



    # Some include-directive search paths.

    include_paths = (
        pxd.make_main_relative_path('./electrical/meta'),
        pxd.make_main_relative_path('./deps/CMSIS_6/CMSIS/Core/Include'),
        pxd.make_main_relative_path('./deps/FreeRTOS_Kernel/include'),
        pxd.make_main_relative_path('./deps/printf/src'),
        pxd.make_main_relative_path('.'),
        pxd.make_main_relative_path('./electrical'),
        *MCU_SUPPORT[target.mcu]['include_paths'],
    )



    # Flags for both the compiler and linker.

    architecture_flags = f'''
        -mcpu={MCU_SUPPORT[target.mcu]['cpu']}
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
        conversion
    '''

    disabled_warnings = '''
        unused-function
        main
        unused-variable
        unused-parameter
        comment
        unused-but-set-variable
        format-zero-length
    '''



    # Additional macro defines.

    defines = [
        ('TARGET_NAME'         , target.name           ),
        ('TARGET_MCU'          , target.mcu            ),
        ('TARGET_USES_FREERTOS', target.use_freertos   ),
        ('MAIN_STACK_SIZE'     , target.main_stack_size),
        ('COMPILING_ESP32'     , False                 ),
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
            -fdiagnostics-color=always
            {'\n'.join(f'-D {name}="{pxd.c_repr(value)}"' for name, value in defines                  )}
            {'\n'.join(f'-W{name}'                        for name        in enabled_warnings .split())}
            {'\n'.join(f'-Wno-{name}'                     for name        in disabled_warnings.split())}
            {'\n'.join(f'-I "{path.as_posix()}"'          for path        in include_paths            )}
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



# @/`Defining Targets`:
#
# Targets are defined in this file in the tuple `TARGETS`.
# The easiest way to create a new target is to copy-paste
# an existing one and insert it at the end of the tuple.
#
# There are several settings that configure a target:
#
#   - `name`
#       The name of the target; one example of
#       where this is used this would be `./cli.py build <name>`.
#
#   - `mcu`
#       The full commerical part number of the STM32 microcontroller
#       being used. The valid options are the keys of `MCU_SUPPORT`.
#       An example of why this information is needed is for GPIO reasons;
#       different MCUs have different sets of GPIOs available and
#       functionalities assigned to them.
#
#   - `source_file_paths`
#       The list of sources that the build system will call the compiler
#       to make object files for. Because a jumbo build approach is used
#       throughout the codebase, this is typically just a single file path
#       to the file that has your `main` function.
#       If no source file paths are given, then the target will not be built.
#       This is useful if there's no code for the target yet, but you still
#       want to do things like verify the GPIOs.
#
#   - `kicad_project`
#       If `None`, does nothing.
#       Otherwise, the target will be associated with `<kicad_project>.kicad_pro`.
#       This is so PCB cross-verification can be done (e.g. checking GPIOs are consistent).
#
#   - `gpios`
#       A table where each row describes a GPIO associated with the target.
#       The format is as follows: (<name>, <pin>, <type>, { <options>... }).
#
#           - <name>
#               The unique name of the GPIO.
#               If the target is associated with a Kicad project,
#               then the GPIO name must match the name of the net
#               that the pin is connected to in the schematic.
#
#           - <pin>
#               The port and number of the GPIO.
#               The pin can also be just `None`;
#               this is useful for when the location of the pin
#               is not yet determined, but you still want to reserve it
#               so no other GPIO can conflict with it.
#
#           - <type>
#               Can be 'INPUT', 'OUTPUT', 'ANALOG', 'ALTERNATE', or `None`.
#               The GPIO type being `None` is useful for when the
#               purpose of the pin is reserved and you don't want any
#               other GPIO to use that specific pin.
#               The types 'INPUT', 'OUTPUT', and 'ANALOG' are self-explanatory;
#               the type 'ALTERNATE' refers to peripheral functionalities like UART.
#
#           - { <options>... }
#               A dictionary of additional configurations for the GPIO.
#
#               - 'initlvl'
#                   The initial logic level after the GPIO is initialized;
#                   that is, if the GPIO is active-low and 'initlvl' is `True`,
#                   then the GPIO will be initialized to 0V.
#
#               - 'active'
#                   The polarity of an GPIO's logic level.
#                   For example, if `True`, then `GPIO_READ(<name>)` be truthy if the pin is reading +3.3V;
#                   otherwise if `False`, then `GPIO_READ(<name>)` be truthy if the pin is reading +0V.
#                   This also applies to the other GPIO macros like `GPIO_SET`.
#                   Active-high is assumed by default.
#
#               - 'altfunc'
#                   When the GPIO is 'ALTERNATE', this field states the alternate functionality.
#                   See `./deps/stpy/databases/<mcu>.csv` for a spreadsheet of all GPIOs and
#                   their available alternate functionalities.
#
#               - 'pull'
#                   Either 'DOWN', 'UP', or None for having a pull-down, pull-up, or no pull resistor.
#
#               - 'open_drain'
#                   Literally whether or not the GPIO is open-drained.
#
#               - 'interrupt'
#                   When the GPIO is 'INPUT', this field defines whether or not an interrupt will be
#                   configured to have triggered when an edge transition happens;
#                   the value of 'RISING' or 'FALLING' determining which.
#                   This is useful for something like processing a button press
#                   using an interrupt-based approach rather than polling.
#
#   - `interrupts`
#       Table of interrupts defined for the target where each row is (<name>, <priority>).
#       Note that priority levels are where lower numbers are higher priority.
#       You'd only typically interact with this field whenever
#       you're working with a peripheral (e.g. UXART) that is
#       implemented using interrupts somewhere.
#       The priority of the interrupt is only subtly important,
#       where you just have to make sure that peripherals that
#       invoke other peripherals have the priorities set in such
#       a way that a dead-lock does not happen.
#
#   - `drivers`
#       List of driver information that is used so that code can be generated
#       to have the target be supported for that particular driver.
#       For instance, if the target firmware needs to use a UART peripheral,
#       a UXART driver would be defined in the `drivers` list with information
#       like the handle name of the peripheral, which peripheral is being used,
#       and so on.
#       The exact list of available drivers and their corresponding settings
#       can be found elsewhere (e.g. `./electrical/uxart.c`).
#
#   - `use_freertos`
#       Whether or not the FreeRTOS task scheduler is compiled as a part
#       of the firmware binary and is enabled.
#       Example usage on FreeRTOS can be found elsewhere.
#
#   - `main_stack_size`
#       Amount of bytes reserved for the stack for the `main` function.
#       Without FreeRTOS, this stack size is all there is for the entire program.
#       With FreeRTOS, each task will have their own stack size defined,
#       so this configuration matters slightly less.
#
#   - `schema`
#       Essentially a set of configurations for defining the clock-tree of the MCU.
#       Most settings are, as of right now, pretty obscure.
#       Most of the time, the reason you're modifying this target
#       configuration is because a new peripheral has as a clock
#       frequency associated with it (e.g. baud). I will not go into
#       much detail about it here, as this setting is pretty experimental,
#       and the other existing targets should give good enough examples to infer from.
