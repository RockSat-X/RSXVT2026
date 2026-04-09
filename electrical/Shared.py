#meta global STLINK_BAUD, TARGETS, PER_MCU, PER_TARGET
#meta global OVCAM_DEFAULT_RESOLUTION
#meta global STACK_SIZE, TV_WRITE_BYTE, TV_TOKEN, OVCAM_JPEG_CTRL3_FIELDS
#meta global LoRaPacket, ESP32Packet, VehicleInterfacePayload, MainFlightComputerLogEntry, PlotSnapshot, ImageMetadata

import types, collections, ctypes
import deps.stpy.pxd.pxd as pxd
from deps.stpy.mcus import MCUS



################################################################################



# @/`ESP32 Sequence Numbers`:
#
# The `.rolling_sequence_number` field is automatically filled out by the
# vehicle ESP32, thus the vehicle FC should leave it empty. This is
# because the ESP32 will handle the buffering of ESP-NOW and LoRa packets,
# and based on when it can queue up packets for those buffers, it'll
# automatically increment the rolling sequence number.
#
# In other words, the `rolling_sequence_number` is how the main FC can
# tell whether or not an ESP-NOW packet has been dropped, and likewise
# with LoRa packets.
#
# The `.timestamp_ms` field should be used to determine the elapsed time
# since the last received packet, but it can also be used to determine if
# a LoRa packet and ESP-NOW packet are the same (when their timestamps are
# also equal).
#
# The `.image_sequence_number` field is just to make it easier to
# determine the start of the OpenMV image data, although with how JPEG
# works, this could be omitted. If the field is zero, this means no image
# data; otherwise the first image chunk begins with sequence number of 1.

class LoRaPacket(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('QuatX'                     , ctypes.c_float ),
        ('QuatY'                     , ctypes.c_float ),
        ('QuatZ'                     , ctypes.c_float ),
        ('QuatS'                     , ctypes.c_float ),
        ('AccelX'                    , ctypes.c_float ),
        ('AccelY'                    , ctypes.c_float ),
        ('AccelZ'                    , ctypes.c_float ),
        ('GyroX'                     , ctypes.c_float ),
        ('GyroY'                     , ctypes.c_float ),
        ('GyroZ'                     , ctypes.c_float ),
        ('timestamp_ms'              , ctypes.c_uint16), # @/`ESP32 Sequence Numbers`.
        ('rolling_sequence_number'   , ctypes.c_uint16), # @/`ESP32 Sequence Numbers`.
        ('computer_vision_confidence', ctypes.c_uint8 ),
        ('crc'                       , ctypes.c_uint8 ),
    )

class ESP32Packet(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('MagX'                 , ctypes.c_float      ),
        ('MagY'                 , ctypes.c_float      ),
        ('MagZ'                 , ctypes.c_float      ),
        ('image_sequence_number', ctypes.c_uint16     ), # @/`ESP32 Sequence Numbers`.
        ('image_bytes'          , ctypes.c_uint8 * 190),
        ('nonredundant'         , LoRaPacket          ),
    )

class VehicleInterfacePayload(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('timestamp_us'  , ctypes.c_uint32),
        ('stepper_issues', ctypes.c_uint8 ),
        ('vn100_issues'  , ctypes.c_uint8 ),
        ('openmv_issues' , ctypes.c_uint8 ),
        ('esp32_issues'  , ctypes.c_uint8 ),
        ('crc'           , ctypes.c_uint8 ),
    )

class UnpaddedMainFlightComputerLogEntry(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('magic'                           , ctypes.c_char * 4      ), # As a precaution; currently not used for synchronization.
        ('esp32_packet_exist'              , ctypes.c_uint8         ),
        ('lora_packet_exist'               , ctypes.c_uint8         ),
        ('vehicle_interface_payload_exist' , ctypes.c_uint8         ),
        ('padding_'                        , ctypes.c_uint8         ),
        ('esp32_packet_data'               , ESP32Packet            ),
        ('lora_packet_data'                , LoRaPacket             ),
        ('vehicle_interface_payload_data'  , VehicleInterfacePayload),
    )

class MainFlightComputerLogEntry(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        *(UnpaddedMainFlightComputerLogEntry._fields_),
        ('sector_padding_', ctypes.c_uint8 * (512 - ctypes.sizeof(UnpaddedMainFlightComputerLogEntry))),
    )

PLOT_SNAPSHOT_TOKEN = b'BARK'

class UnpaddedPlotSnapshot(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('magic'                 , ctypes.c_char * len(PLOT_SNAPSHOT_TOKEN)),
        ('timestamp_us'          , ctypes.c_uint32                         ),
        ('QuatX'                 , ctypes.c_float                          ),
        ('QuatY'                 , ctypes.c_float                          ),
        ('QuatZ'                 , ctypes.c_float                          ),
        ('QuatS'                 , ctypes.c_float                          ),
        ('MagX'                  , ctypes.c_float                          ),
        ('MagY'                  , ctypes.c_float                          ),
        ('MagZ'                  , ctypes.c_float                          ),
        ('AccelX'                , ctypes.c_float                          ),
        ('AccelY'                , ctypes.c_float                          ),
        ('AccelZ'                , ctypes.c_float                          ),
        ('GyroX'                 , ctypes.c_float                          ),
        ('GyroY'                 , ctypes.c_float                          ),
        ('GyroZ'                 , ctypes.c_float                          ),
        ('angular_velocity_x'    , ctypes.c_float                          ),
        ('angular_velocity_y'    , ctypes.c_float                          ),
        ('angular_velocity_z'    , ctypes.c_float                          ),
        ('angular_acceleration_x', ctypes.c_float                          ),
        ('angular_acceleration_y', ctypes.c_float                          ),
        ('angular_acceleration_z', ctypes.c_float                          ),
    )

class PlotSnapshot(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        *(UnpaddedPlotSnapshot._fields_),
        ('sector_padding_', ctypes.c_uint8 * (128 - ctypes.sizeof(UnpaddedPlotSnapshot))),
    )

TV_TOKEN = types.SimpleNamespace(
    START = b'<TV>',
    END   = b'</TV>',
)

class UnpaddedImageMetadata(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        ('ending_token'      , ctypes.c_uint8 * len(TV_TOKEN.END)  ),
        ('image_index'       , ctypes.c_uint32                     ),
        ('image_size'        , ctypes.c_uint32                     ),
        ('image_timestamp_us', ctypes.c_uint32                     ),
        ('cpu_cycle_counter' , ctypes.c_uint32                     ),
        ('padding'           , ctypes.c_uint8 * 0                  ), # To be padded out.
        ('starting_token'    , ctypes.c_uint8 * len(TV_TOKEN.START)),
    )

class ImageMetadata(ctypes.Structure):
    _pack_   = True
    _fields_ = (
        *(
            (field_name, ctypes.c_uint8 * (512 - ctypes.sizeof(UnpaddedImageMetadata)))
            if field_name == 'padding' else
            (field_name, field_type)
            for field_name, field_type in UnpaddedImageMetadata._fields_
        ),
    )



################################################################################



STACK_SIZE = 8192

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

STLINK_BAUD = 1_000_000
ESP32_BAUD  = 1_000_000 # @/`Coupled Baud Rate between STM32 and ESP32`.
VN100_BAUD  =   115_200

VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS = 0x12
VEHICLE_INTERFACE_BAUD              = 10_000

MFC_DEBUG_BOARD_BAUD              = 1_000
MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS = 0x1E

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

        use_freertos = True,
        schema       = {
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

        flight_ready = False,

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
                'mode'       : 'master',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C2',
                'handle'     : 'bee',
                'mode'       : 'slave',
                'address'    : 0b_001_1110,
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'I2C1_BAUD'         : 1_000,
            'I2C2_BAUD'         : 1_000,
            'I2C1_TIMEOUT'      : 2,
            'I2C2_TIMEOUT'      : 2,
            'TIM2_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

    ),



    ################################################################################



    types.SimpleNamespace(

        name              = 'DemoSSD1306',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoSSD1306.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'    , 'A5' , 'OUTPUT'   , { 'initlvl' : False                                          }),
            ('stlink_tx'    , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                                    }),
            ('stlink_rx'    , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                                    }),
            ('swdio'        , 'A13', None       , {                                                            }),
            ('swclk'        , 'A14', None       , {                                                            }),
            ('button'       , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True                          }),
            ('ssd1306_clock', 'B6' , 'ALTERNATE', { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('ssd1306_data' , 'B7' , 'ALTERNATE', { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
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
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'ssd1306',
                'mode'       : 'master',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'I2C1_BAUD'         : 2_000_000,
            'I2C1_TIMEOUT'      : 0.030,
            'TIM2_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

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
            ('led_green'  , 'A5' , 'OUTPUT'   , { 'initlvl' : False                 }),
            ('stlink_tx'  , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx'  , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'           }),
            ('swdio'      , 'A13', None       , {                                   }),
            ('swclk'      , 'A14', None       , {                                   }),
            ('button'     , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True }),
            ('spi_nss'    , 'B1' , 'ALTERNATE', { 'altfunc' : 'SPI2_NSS'            }),
            ('spi_clock'  , 'B10', 'ALTERNATE', { 'altfunc' : 'SPI2_SCK'            }),
            ('spi_mosi'   , 'C3' , 'ALTERNATE', { 'altfunc' : 'SPI2_MOSI'           }),
        ),

        interrupts = (
            ('USART2'         , 0),
            ('SPI2'           , 1),
            ('GPDMA1_Channel7', 2),
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
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'PLL3P_CK'          : 600_000 * 4,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'SPI2_BAUD'         : 600_000,
            'TIM2_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoBuzzer',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoBuzzer.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'        , 'A5' , 'OUTPUT'   , { 'initlvl' : False                 }),
            ('stlink_tx'        , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx'        , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'           }),
            ('swdio'            , 'A13', None       , {                                   }),
            ('swclk'            , 'A14', None       , {                                   }),
            ('button'           , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True }),
            ('buzzer'           , 'B10', 'ALTERNATE', { 'altfunc' : 'TIM8_CH1'            }),
            ('buzzer_complement', 'A7' , 'ALTERNATE', { 'altfunc' : 'TIM8_CH1N'           }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('TIM8_UP', 1),
        ),

        drivers = (
            {
                'type'       : 'UXART',
                'peripheral' : 'USART2',
                'handle'     : 'stlink',
                'mode'       : 'full_duplex',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'TIM8_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoGNC',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoGNC.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green', 'A5' , 'OUTPUT'   , { 'initlvl' : False                 }),
            ('stlink_tx', 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'           }),
            ('stlink_rx', 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'           }),
            ('swdio'    , 'A13', None       , {                                   }),
            ('swclk'    , 'A14', None       , {                                   }),
            ('button'   , 'C13', 'INPUT'    , { 'pull'    : None, 'active' : True }),
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

        use_freertos = False,
        schema       = {
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

        flight_ready = False,

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

        use_freertos = False,
        schema       = {
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

        flight_ready = False,

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
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 250_000_000,
            'CPU_CK'            : 250_000_000,
            'APB1_CK'           : 250_000_000,
            'APB2_CK'           : 250_000_000,
            'APB3_CK'           : 250_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'TIM2_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

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
            ('sd_cmd'    , 'B2' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'          , 'speed' : 'VERY_HIGH' }),
            ('sd_d0'     , 'B13', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'           , 'speed' : 'VERY_HIGH' }),
            ('sd_d1'     , 'C9' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'           , 'speed' : 'VERY_HIGH' }),
            ('sd_d2'     , 'C10', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'           , 'speed' : 'VERY_HIGH' }),
            ('sd_d3'     , 'C11', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'           , 'speed' : 'VERY_HIGH' }),
            ('sd_ck'     , 'C12', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'           , 'speed' : 'VERY_HIGH' }),
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
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = True,
        schema       = {
            'HSI_ENABLE'          : True,
            'HSI48_ENABLE'        : True,
            'CSI_ENABLE'          : True,
            'PLL1P_CK'            : 240_000_000,
            'PLL2R_CK'            : 240_000_000,
            'CPU_CK'              : 240_000_000,
            'APB1_CK'             : 240_000_000,
            'APB2_CK'             : 240_000_000,
            'APB3_CK'             : 240_000_000,
            'USART2_BAUD'         : STLINK_BAUD,
            'TIM2_COUNTER_RATE'   :   1_000_000,
            'SDMMC1_TIMEOUT'      : 0.050,
            'SDMMC1_INITIAL_BAUD' :    400_000,
            'SDMMC1_FULL_BAUD'    : 24_000_000,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'MainFlightComputer',
        mcu               = 'STM32H533VET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/MainFlightComputer.c'),
        ),

        kicad_project = 'MainFlightComputer',

        gpios = (
            ('led_channel_red'                , 'E2' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                     }),
            ('led_channel_green'              , 'E3' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                     }),
            ('led_channel_blue'               , 'E4' , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                     }),
            ('stlink_tx'                      , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                 }),
            ('stlink_rx'                      , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                 }),
            ('swdio'                          , 'A13', None        , {                                                         }),
            ('swclk'                          , 'A14', None        , {                                                         }),
            ('swo'                            , 'B3' , None        , {                                                         }),
            ('logic_timer_event_1'            , 'B9' , 'INPUT'     , { 'pull'    : None                                        }),
            ('buzzer'                         , 'C6' , 'ALTERNATE' , { 'altfunc' : 'TIM8_CH1'                                  }),
            ('sd_cmd'                         , 'D2' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'                                }),
            ('sd_data_0'                      , 'C8' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'                                 }),
            ('sd_data_1'                      , 'C9' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'                                 }),
            ('sd_data_2'                      , 'C10', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'                                 }),
            ('sd_data_3'                      , 'C11', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'                                 }),
            ('sd_clock'                       , 'C12', 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'                                 }),
            ('spi_nss_A'                      , 'A11', 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'                                  }),
            ('spi_clock_A'                    , 'B13', 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK'                                  }),
            ('spi_mosi_A'                     , 'C1' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI'                                 }),
            ('spi_miso_A'                     , 'C2' , 'ALTERNATE' , { 'altfunc' : 'SPI2_MISO'                                 }),
            ('spi_ready_A'                    , 'D5' , 'ALTERNATE' , { 'altfunc' : 'SPI2_RDY'                                  }),
            ('spi_nss_B'                      , 'A4' , 'ALTERNATE' , { 'altfunc' : 'SPI1_NSS'                                  }),
            ('spi_clock_B'                    , 'A5' , 'ALTERNATE' , { 'altfunc' : 'SPI1_SCK'                                  }),
            ('spi_mosi_B'                     , 'B5' , 'ALTERNATE' , { 'altfunc' : 'SPI1_MOSI'                                 }),
            ('spi_miso_B'                     , 'A6' , 'ALTERNATE' , { 'altfunc' : 'SPI1_MISO'                                 }),
            ('spi_ready_B'                    , 'B2' , 'ALTERNATE' , { 'altfunc' : 'SPI1_RDY'                                  }),
            ('vehicle_interface_i2c_data'     , 'B7' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SDA', 'open_drain' : True, 'pull' : 'UP' }),
            ('vehicle_interface_i2c_clock'    , 'B6' , 'ALTERNATE' , { 'altfunc' : 'I2C1_SCL', 'open_drain' : True, 'pull' : 'UP' }),
            ('sensor_i2c_data'                , 'B12', 'ALTERNATE' , { 'altfunc' : 'I2C2_SDA', 'open_drain' : True             }),
            ('sensor_i2c_clock'               , 'B10', 'ALTERNATE' , { 'altfunc' : 'I2C2_SCL', 'open_drain' : True             }),
            ('flight_computer_debug_i2c_data' , 'D7' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SDA', 'open_drain' : True             }),
            ('flight_computer_debug_i2c_clock', 'D6' , 'ALTERNATE' , { 'altfunc' : 'I2C3_SCL', 'open_drain' : True             }),
            ('esp32_reset'                    , 'D10', 'OUTPUT'    , { 'initlvl' : True, 'active' : False, 'open_drain' : True }),
            ('esp32_uart_tx'                  , 'B14', None        , {                                                         }),
            ('esp32_uart_rx'                  , 'B15', 'ALTERNATE' , { 'altfunc' : 'USART1_RX'                                 }),
            ('uart_tx_B'                      , 'D8' , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'                                 }),
            ('uart_rx_B'                      , 'B1' , 'ALTERNATE' , { 'altfunc' : 'USART3_RX'                                 }),
            ('lis2mdl_chip_select'            , 'D14', None        , {                                                         }),
            ('lis2mdl_data_ready'             , 'E13', None        , {                                                         }),
            ('lsm6dsv32x_interrupt_1'         , 'A12', None        , {                                                         }),
            ('lsm6dsv32x_interrupt_2'         , 'C7' , None        , {                                                         }),
            ('lsm6dsv32x_chip_select'         , 'E14', None        , {                                                         }),
            ('solarboard_analog_A'            , 'A0' , 'ANALOG'    , {                                                         }),
            ('solarboard_analog_B'            , 'A1' , 'ANALOG'    , {                                                         }),
            ('logic_gse_1'                    , 'D4' , 'INPUT'     , { 'pull'    : None                                        }),
            ('general_purpose_B'              , 'D12', None        , {                                                         }),
            ('general_purpose_C'              , 'C0' , None        , {                                                         }),
            ('general_purpose_D'              , 'C15', None        , {                                                         }),
            ('general_purpose_G'              , 'C14', None        , {                                                         }),
            ('general_purpose_H'              , 'C13', None        , {                                                         }),
            ('general_purpose_I'              , 'E6' , None        , {                                                         }),
            ('general_purpose_J'              , 'D3' , None        , {                                                         }),
            ('general_purpose_K'              , 'D1' , None        , {                                                         }),
            ('general_purpose_N'              , 'E0' , None        , {                                                         }),
            ('testpoint_A'                    , 'E11', None        , {                                                         }),
            ('testpoint_B'                    , 'B0' , None        , {                                                         }),
            ('testpoint_C'                    , 'C5' , None        , {                                                         }),
            ('testpoint_D'                    , 'H0' , None        , {                                                         }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('USART1' , 1),
            ('SDMMC1' , 2),
            ('I2C1_EV', 3),
            ('I2C1_ER', 3),
            ('I2C3_EV', 4),
            ('I2C3_ER', 4),
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
                'peripheral' : 'USART1',
                'handle'     : 'esp32',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C1',
                'handle'     : 'vehicle_interface',
                'mode'       : 'master',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C3',
                'handle'     : 'debug_board',
                'mode'       : 'master',
            },
            {
                'type'       : 'SD',
                'peripheral' : 'SDMMC1',
                'handle'     : 'primary',
            },
        ),

        use_freertos = True,
        schema       = {
            'HSI_ENABLE'                   : True,
            'HSI48_ENABLE'                 : True,
            'CSI_ENABLE'                   : True,
            'PLL1P_CK'                     : 250_000_000,
            'PLL2R_CK'                     : 240_000_000,
            'CPU_CK'                       : 250_000_000,
            'APB1_CK'                      : 250_000_000,
            'APB2_CK'                      : 250_000_000,
            'APB3_CK'                      : 250_000_000,
            'USART1_BAUD'                  : ESP32_BAUD,
            'USART2_BAUD'                  : STLINK_BAUD,
            'TIM1_UPDATE_RATE'             : 1 / 0.001,
            'TIM2_COUNTER_RATE'            : 1_000_000,
            'ANALOG_POSTDIVIDER_KERNEL_CK' : 32_000_000,
            'I2C1_BAUD'                    : VEHICLE_INTERFACE_BAUD,
            'I2C1_TIMEOUT'                 : 2,
            'I2C3_BAUD'                    : MFC_DEBUG_BOARD_BAUD,
            'I2C3_TIMEOUT'                 : 2,
            'SDMMC1_TIMEOUT'               : 0.250,
            'SDMMC1_INITIAL_BAUD'          :    400_000,
            'SDMMC1_FULL_BAUD'             : 24_000_000,
            'WATCHDOG_DURATION'            : 10,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'VehicleFlightComputer',
        mcu               = 'STM32H533VET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/VehicleFlightComputer.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_channel_red'            , 'C0'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                      }),
            ('led_channel_green'          , 'C1'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                      }),
            ('led_channel_blue'           , 'A6'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                      }),
            ('stlink_tx'                  , 'A2'  , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'                                  }),
            ('stlink_rx'                  , 'A3'  , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'                                  }),
            ('swdio'                      , 'A13' , None        , {                                                          }),
            ('swclk'                      , 'A14' , None        , {                                                          }),
            ('swo'                        , 'B3'  , None        , {                                                          }),
            ('buzzer'                     , 'A5'  , 'ALTERNATE' , { 'altfunc' : 'TIM8_CH1N'                                  }),
            ('sd_cmd'                     , 'D2'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CMD'                                 }),
            ('sd_data_0'                  , 'C8'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D0'                                  }),
            ('sd_data_1'                  , 'C9'  , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D1'                                  }),
            ('sd_data_2'                  , 'C10' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D2'                                  }),
            ('sd_data_3'                  , 'C11' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_D3'                                  }),
            ('sd_clock'                   , 'C12' , 'ALTERNATE' , { 'altfunc' : 'SDMMC1_CK'                                  }),
            ('battery_allowed'            , 'D0'  , 'OUTPUT'    , { 'initlvl' : False                                        }),
            ('external_detected'          , 'D1'  , 'INPUT'     , { 'pull'    : 'DOWN'                                       }),
            ('openmv_spi_nss'             , 'B12' , 'ALTERNATE' , { 'altfunc' : 'SPI2_NSS'                                   }),
            ('openmv_spi_clock'           , 'B10' , 'ALTERNATE' , { 'altfunc' : 'SPI2_SCK' , 'pull' : 'UP'                   }),
            ('openmv_spi_mosi'            , 'C3'  , 'ALTERNATE' , { 'altfunc' : 'SPI2_MOSI', 'pull' : 'UP'                   }),
            ('openmv_spi_miso'            , 'C2'  , None        , { 'altfunc' : 'SPI2_MISO'                                  }),
            ('openmv_spi_ready'           , 'D5'  , None        , { 'altfunc' : 'SPI2_RDY'                                   }),
            ('openmv_reset'               , 'C15' , 'OUTPUT'    , { 'initlvl' : True, 'active' : False, 'open_drain' : True  }),
            ('motor_uart'                 , 'B14' , 'ALTERNATE' , { 'altfunc' : 'USART1_TX'                                  }),
            ('vehicle_interface_i2c_data' , 'D7'  , 'ALTERNATE' , { 'altfunc' : 'I2C3_SDA', 'open_drain' : True              }),
            ('vehicle_interface_i2c_clock', 'D6'  , 'ALTERNATE' , { 'altfunc' : 'I2C3_SCL', 'open_drain' : True              }),
            ('motor_enable'               , 'A4'  , 'OUTPUT'    , { 'initlvl' : False, 'active' : False                      }),
            ('vn100_uart_tx'              , 'D8'  , 'ALTERNATE' , { 'altfunc' : 'USART3_TX'                                  }),
            ('vn100_uart_rx'              , 'D9'  , 'ALTERNATE' , { 'altfunc' : 'USART3_RX', 'pull' : 'UP'                   }),
            ('esp32_uart_tx'              , 'A0'  , 'ALTERNATE' , { 'altfunc' : 'UART4_TX'                                   }),
            ('esp32_uart_rx'              , 'A1'  , 'ALTERNATE' , { 'altfunc' : 'UART4_RX'                                   }),
            ('esp32_reset'                , 'E8'  , 'OUTPUT'    , { 'initlvl' : True, 'active' : False, 'open_drain' : True  }),
            ('testpoint_A'                , 'A10' , None        , {                                                          }),
            ('testpoint_B'                , 'A11' , None        , {                                                          }),
        ),

        interrupts = (
            ('USART2'         , 0),
            ('UART4'          , 0),
            ('USART1'         , 1),
            ('USART3'         , 1),
            ('SPI2'           , 1),
            ('GPDMA1_Channel7', 2),
            ('TIM1_UP'        , 2),
            ('TIM8_UP'        , 3),
            ('SDMMC1'         , 4),
            ('I2C3_EV'        , 5),
            ('I2C3_ER'        , 5),
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
                'type'       : 'UXART',
                'peripheral' : 'USART3',
                'handle'     : 'vn100',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'UXART',
                'peripheral' : 'UART4',
                'handle'     : 'esp32',
                'mode'       : 'full_duplex',
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C3',
                'handle'     : 'vehicle_interface',
                'mode'       : 'slave',
                'address'    : VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS,
            },
            {
                'type'                         : 'Stepper',
                'uxart_handle'                 : 'stepper_uart',
                'enable_gpio'                  : 'motor_enable',
                'timer_peripheral'             : 'TIM1',
                'timer_update_event_interrupt' : 'TIM1_UP',
                'units'                        : (
                    ('axis_x', 0),
                    ('axis_y', 1),
                    ('axis_z', 2),
                ),
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
            {
                'type'       : 'SPI',
                'peripheral' : 'SPI2',
                'handle'     : 'openmv',
            },
        ),

        use_freertos = True,
        schema       = {
            'HSI_ENABLE'          : True,
            'HSI48_ENABLE'        : True,
            'CSI_ENABLE'          : True,
            'PLL1P_CK'            : 250_000_000,
            'PLL1Q_CK'            :  10_000_000,
            'PLL2R_CK'            : 240_000_000,
            'PLL3P_CK'            :   9_600_000,
            'CPU_CK'              : 250_000_000,
            'APB1_CK'             : 250_000_000,
            'APB2_CK'             : 250_000_000,
            'APB3_CK'             : 250_000_000,
            'USART2_BAUD'         : STLINK_BAUD,
            'SDMMC1_TIMEOUT'      : 0.250,
            'SDMMC1_INITIAL_BAUD' :    400_000,
            'SDMMC1_FULL_BAUD'    : 24_000_000,
            'USART1_BAUD'         :    200_000,
            'USART3_BAUD'         : VN100_BAUD,
            'UART4_BAUD'          : ESP32_BAUD,
            'I2C3_BAUD'           : VEHICLE_INTERFACE_BAUD,
            'I2C3_TIMEOUT'        : 2,
            'SPI2_BAUD'           : 600_000, # @/`OpenMV SPI Baud`.
            'TIM1_UPDATE_RATE'    : 1 / 0.001,
            'TIM2_COUNTER_RATE'   : 1_000_000,
            'TIM8_COUNTER_RATE'   : 1_000_000,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DebugBoard',
        mcu               = 'STM32H533VET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DebugBoard.c'),
        ),

        kicad_project = 'DebugBoard',

        gpios = (
            ('stlink_tx'                  , 'A2'  , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                     }),
            ('stlink_rx'                  , 'A3'  , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                     }),
            ('swdio'                      , 'A13' , None       , {                                             }),
            ('swclk'                      , 'A14' , None       , {                                             }),
            ('swo'                        , 'B3'  , None       , {                                             }),
            ('led_channel_red_A'          , 'E3'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_green_A'        , 'E4'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_blue_A'         , 'E5'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_red_B'          , 'D5'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_green_B'        , 'D4'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_blue_B'         , 'D6'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_red_C'          , 'C4'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_green_C'        , 'A7'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_blue_C'         , 'C5'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_red_D'          , 'A9'  , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_green_D'        , 'A10' , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('led_channel_blue_D'         , 'A11' , 'OUTPUT'   , { 'initlvl' : False, 'active' : False         }),
            ('button_A'                   , 'E14' , 'INPUT'    , { 'pull' : 'UP'                               }),
            ('button_B'                   , 'E12' , 'INPUT'    , { 'pull' : 'UP'                               }),
            ('sd_cmd'                     , 'D2'  , 'ALTERNATE', { 'altfunc' : 'SDMMC1_CMD'                    }),
            ('sd_data_0'                  , 'C8'  , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D0'                     }),
            ('sd_data_1'                  , 'C9'  , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D1'                     }),
            ('sd_data_2'                  , 'C10' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D2'                     }),
            ('sd_data_3'                  , 'C11' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D3'                     }),
            ('sd_clock'                   , 'C12' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_CK'                     }),
            ('buzzer'                     , 'A5'  , 'ALTERNATE', { 'altfunc' : 'TIM8_CH1N'                     }),
            ('communication_i2c_clock'    , 'B6'  , 'ALTERNATE', { 'altfunc' : 'I2C1_SCL', 'open_drain' : True }),
            ('communication_i2c_data'     , 'B7'  , 'ALTERNATE', { 'altfunc' : 'I2C1_SDA', 'open_drain' : True }),
            ('communication_uart_rx_A'    , 'B15' , 'ALTERNATE', { 'altfunc' : 'USART1_RX'                     }),
            ('communication_uart_rx_B'    , 'B1'  , 'ALTERNATE', { 'altfunc' : 'USART3_RX'                     }),
            ('display_i2c_clock'          , 'B10' , 'ALTERNATE', { 'altfunc' : 'I2C2_SCL', 'open_drain' : True }),
            ('display_i2c_data'           , 'B12' , 'ALTERNATE', { 'altfunc' : 'I2C2_SDA', 'open_drain' : True }),
            ('testpoint_A'                , 'E10' , None       , {                                             }),
            ('testpoint_B'                , 'D3'  , None       , {                                             }),
        ),

        interrupts = (
            ('USART2' , 0),
            ('SDMMC1' , 1),
            ('I2C1_EV', 2),
            ('I2C1_ER', 2),
            ('I2C2_EV', 3),
            ('I2C2_ER', 3),
            ('TIM8_UP', 4),
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
                'handle'     : 'communication',
                'mode'       : 'slave',
                'address'    : MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS,
            },
            {
                'type'       : 'I2C',
                'peripheral' : 'I2C2',
                'handle'     : 'ssd1306',
                'mode'       : 'master',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
            {
                'type'       : 'SD',
                'peripheral' : 'SDMMC1',
                'handle'     : 'primary',
            },
        ),

        use_freertos = True,
        schema       = {
            'HSI_ENABLE'          : True,
            'HSI48_ENABLE'        : True,
            'CSI_ENABLE'          : True,
            'PLL1P_CK'            : 250_000_000,
            'PLL2R_CK'            : 240_000_000,
            'CPU_CK'              : 250_000_000,
            'APB1_CK'             : 250_000_000,
            'APB2_CK'             : 250_000_000,
            'APB3_CK'             : 250_000_000,
            'USART2_BAUD'         : STLINK_BAUD,
            'I2C1_BAUD'           : MFC_DEBUG_BOARD_BAUD,
            'I2C1_TIMEOUT'        : 2,
            'I2C2_BAUD'           : 1_000_000,
            'I2C2_TIMEOUT'        : 0.030,
            'TIM2_COUNTER_RATE'   : 1_000_000,
            'TIM8_COUNTER_RATE'   : 1_000_000,
            'SDMMC1_TIMEOUT'      : 0.250,
            'SDMMC1_INITIAL_BAUD' :    400_000,
            'SDMMC1_FULL_BAUD'    : 24_000_000,
            'WATCHDOG_DURATION'   : 10,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'MainCameraSystem',
        mcu               = 'STM32H533VET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/MainCameraSystem.c'),
        ),

        kicad_project = 'MainCameraSystem',

        gpios = (
            ('led_channel_red'  , 'E7' , 'OUTPUT'   , { 'initlvl' : True, 'active' : False           }),
            ('led_channel_green', 'B1' , 'OUTPUT'   , { 'initlvl' : True, 'active' : False           }),
            ('led_channel_blue' , 'B2' , 'OUTPUT'   , { 'initlvl' : True, 'active' : False           }),
            ('stlink_tx'        , 'A2' , 'ALTERNATE', { 'altfunc' : 'USART2_TX'                      }),
            ('stlink_rx'        , 'A3' , 'ALTERNATE', { 'altfunc' : 'USART2_RX'                      }),
            ('swdio'            , 'A13', None       , {                                              }),
            ('swclk'            , 'A14', None       , {                                              }),
            ('swo'              , 'B3' , None       , {                                              }),
            ('testpoint_A'      , 'D7' , None       , {                                              }),
            ('testpoint_B'      , 'A9' , None       , {                                              }),
            ('sd_cmd'           , 'D2' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_CMD', 'speed' : 'LOW'    }),
            ('sd_data_0'        , 'C8' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D0' , 'speed' : 'LOW'    }),
            ('sd_data_1'        , 'C9' , 'ALTERNATE', { 'altfunc' : 'SDMMC1_D1' , 'speed' : 'LOW'    }),
            ('sd_data_2'        , 'C10', 'ALTERNATE', { 'altfunc' : 'SDMMC1_D2' , 'speed' : 'LOW'    }),
            ('sd_data_3'        , 'C11', 'ALTERNATE', { 'altfunc' : 'SDMMC1_D3' , 'speed' : 'LOW'    }),
            ('sd_clock'         , 'C12', 'ALTERNATE', { 'altfunc' : 'SDMMC1_CK' , 'speed' : 'LOW'    }),
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

        interrupts = (
            ('USART2'         , 0),
            ('SDMMC1'         , 1),
            ('DCMI_PSSI'      , 1),
            ('GPDMA1_Channel7', 2),
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
                'mode'       : 'master',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
            {
                'type'       : 'SD',
                'peripheral' : 'SDMMC1',
                'handle'     : 'primary',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'          : True,
            'HSI48_ENABLE'        : True,
            'CSI_ENABLE'          : True,
            'PLL1P_CK'            : 250_000_000,
            'PLL2R_CK'            : 240_000_000,
            'CPU_CK'              : 250_000_000,
            'APB1_CK'             : 250_000_000,
            'APB2_CK'             : 250_000_000,
            'APB3_CK'             : 250_000_000,
            'USART2_BAUD'         : STLINK_BAUD,
            'I2C2_BAUD'           : 10_000,
            'I2C2_TIMEOUT'        : 2,
            'TIM2_COUNTER_RATE'   : 1_000_000,
            'SDMMC1_TIMEOUT'      : 0.250,
            'SDMMC1_INITIAL_BAUD' :    400_000,
            'SDMMC1_FULL_BAUD'    : 24_000_000,
            'WATCHDOG_DURATION'   : 10, # Must be long enough so that the filesystem can be formatted in time.
        },

        flight_ready = True,

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
            ('DCMI_PSSI'      , 1),
            ('GPDMA1_Channel7', 2),
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
                'mode'       : 'master',
            },
            {
                'type'       : 'TIMEKEEPING',
                'peripheral' : 'TIM2',
            },
        ),

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'        : True,
            'HSI48_ENABLE'      : True,
            'CSI_ENABLE'        : True,
            'PLL1P_CK'          : 240_000_000,
            'CPU_CK'            : 240_000_000,
            'APB1_CK'           : 240_000_000,
            'APB2_CK'           : 240_000_000,
            'APB3_CK'           : 240_000_000,
            'USART2_BAUD'       : STLINK_BAUD,
            'I2C2_BAUD'         : 10_000,
            'I2C2_TIMEOUT'      : 2,
            'TIM2_COUNTER_RATE' : 1_000_000,
        },

        flight_ready = False,

    ),



    ########################################



    types.SimpleNamespace(

        name              = 'DemoAnalog',
        mcu               = 'STM32H533RET6',
        source_file_paths = (
            pxd.make_main_relative_path('./electrical/DemoAnalog.c'),
        ),

        kicad_project = None,

        gpios = (
            ('led_green'     , 'A5' , 'OUTPUT'    , { 'initlvl' : False              }),
            ('stlink_tx'     , 'A2' , 'ALTERNATE' , { 'altfunc' : 'USART2_TX'        }),
            ('stlink_rx'     , 'A3' , 'ALTERNATE' , { 'altfunc' : 'USART2_RX'        }),
            ('swdio'         , 'A13', None        , {                                }),
            ('swclk'         , 'A14', None        , {                                }),
            ('button'        , 'C13', 'INPUT'     , { 'pull' : None, 'active' : True }),
            ('analog_input_A', 'A6' , 'ANALOG'    , {                                }),
            ('analog_input_B', 'A7' , 'ANALOG'    , {                                }),
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

        use_freertos = False,
        schema       = {
            'HSI_ENABLE'                   : True,
            'HSI48_ENABLE'                 : True,
            'CSI_ENABLE'                   : True,
            'PLL1P_CK'                     : 250_000_000,
            'PLL2R_CK'                     : 50_000_000,
            'CPU_CK'                       : 250_000_000,
            'APB1_CK'                      : 250_000_000,
            'APB2_CK'                      : 250_000_000,
            'APB3_CK'                      : 250_000_000,
            'USART2_BAUD'                  : STLINK_BAUD,
            'ANALOG_POSTDIVIDER_KERNEL_CK' : 50_000_000,
        },

        flight_ready = False,

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
        pxd.make_main_relative_path('./deps'),
        pxd.make_main_relative_path('./deps/CMSIS-DSP/Include'),
        pxd.make_main_relative_path('./deps/CMSIS-DSP/PrivateInclude'),
        pxd.make_main_relative_path('./deps/CMSIS-DSP'),
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
        ('TARGET_NAME'                        , target.name                                        ),
        ('TARGET_MCU'                         , target.mcu                                         ),
        ('TARGET_USES_FREERTOS'               , target.use_freertos                                ),
        ('STACK_SIZE'                         , STACK_SIZE                                         ),
        ('COMPILING_ESP32'                    , False                                              ),
        ('VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS', VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS                ),
        ('ESP32_BAUD'                         , ESP32_BAUD                                         ),
        ('PLOT_SNAPSHOT_TOKEN'                , f'STRINGIFY({PLOT_SNAPSHOT_TOKEN.decode('UTF-8')})'),
        ('TV_TOKEN_START'                     , f'STRINGIFY({TV_TOKEN.START     .decode('UTF-8')})'),
        ('TV_TOKEN_END'                       , f'STRINGIFY({TV_TOKEN.END       .decode('UTF-8')})'),
        ('TV_WRITE_BYTE'                      , f'0x{TV_WRITE_BYTE :02X}'                          ),
        ('MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS'  , f'0x{MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS :02X}'      ),
        ('FLIGHT_READY'                       , target.flight_ready                                ),
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
            {'-O3' if target.flight_ready else '-O0'}
            -ggdb3
            -std=gnu2x
            -pipe
            -fmax-errors=1
            -fno-strict-aliasing
            -fno-eliminate-unused-debug-types
            -ffunction-sections
            -fdata-sections
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
#   - `schema`
#       Essentially a set of configurations for defining the clock-tree of the MCU.
#       Most settings are, as of right now, pretty obscure.
#       Most of the time, the reason you're modifying this target
#       configuration is because a new peripheral has as a clock
#       frequency associated with it (e.g. baud). I will not go into
#       much detail about it here, as this setting is pretty experimental,
#       and the other existing targets should give good enough examples to infer from.
#
#   - `flight_ready`
#       A boolean that is a switch for disabling certain code paths that were only for
#       development purposes. Things like `sorry` will now result in a compilation error.
