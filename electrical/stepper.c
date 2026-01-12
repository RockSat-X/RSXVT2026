////////////////////////////////////////////////////////////////////////////////



#define STEPPER_PERIOD_US     25'000
#define STEPPER_WINDOW_LENGTH 32



static_assert(IS_POWER_OF_TWO(STEPPER_WINDOW_LENGTH));



#include "stepper_driver_support.meta"
/* #meta

    # As of writing, the timer's counter
    # frequency must be 1 MHz; this is mostly
    # arbitrary can be subject to customization.

    for target in TARGETS:

        for driver in target.drivers:
            if driver['type'] != 'Stepper':
                continue

            timer = driver['peripheral']

            if target.schema.get(property := f'{timer}_COUNTER_RATE', None) != 1_000_000:
                raise ValueError(
                    f'The stepper driver '
                    f'for target {repr(target.name)} '
                    f'requires {repr(property)} '
                    f'in the schema with value of {1_000_000}.'
                )



    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'Stepper',
        cmsis_name  = 'TIM',
        common_name = 'TIMx',
        terms       = lambda type, peripheral, interrupt, channel, handle, node_address: (
            ('{}'                           , 'expression' ,                                                 ),
            ('NVICInterrupt_{}_update_event', 'expression' , f'NVICInterrupt_{interrupt}'                    ),
            ('STPY_{}_DIVIDER'              , 'expression' ,                                                 ),
            ('{}_ENABLE'                    , 'cmsis_tuple',                                                 ),
            ('{}_CAPTURE_COMPARE_ENABLE_y'  , 'cmsis_tuple', f'{peripheral}_CAPTURE_COMPARE_ENABLE_{channel}'),
            ('{}_CAPTURE_COMPARE_VALUE_y'   , 'cmsis_tuple', f'{peripheral}_CAPTURE_COMPARE_VALUE_{channel}' ),
            ('{}_CAPTURE_COMPARE_MODE_y'    , 'cmsis_tuple', f'{peripheral}_CAPTURE_COMPARE_MODE_{channel}'  ),
            ('INTERRUPT_{}_update_event'    , 'interrupt'  , f'INTERRUPT_{interrupt}'                        ),
            ('STEPPER_NODE_ADDRESS'         , 'expression' , node_address                                    ),
        ),
    )

*/



enum StepperDriverUARTState : u32
{
    StepperDriverUARTState_standby,
    StepperDriverUARTState_read_scheduled,
    StepperDriverUARTState_read_requested,
    StepperDriverUARTState_write_scheduled,
    StepperDriverUARTState_done,
};



struct StepperDriver
{

    i8           deltas[STEPPER_WINDOW_LENGTH];
    volatile u32 reader;
    volatile u32 writer;

    struct StepperDriverUART
    {
        enum StepperDriverUARTState state;
        u8                          register_address;
        u32                         data;
    } uart;

};



static struct StepperDriver _STEPPER_drivers[StepperHandle_COUNT] = {0};



pack_push

    struct StepperReadRequest
    {
        u8 sync;
        u8 node_address;
        u8 register_address;
        u8 crc;
    };

    struct StepperReadResponse
    {
        u8  sync;
        u8  master_address;
        u8  register_address;
        u32 data; // Big-endian.
        u8  crc;
    };

    struct StepperWriteRequest
    {
        u8  sync;
        u8  node_address;
        u8  register_address;
        u32 data; // Big-endian.
        u8  crc;
    };

pack_pop



////////////////////////////////////////////////////////////////////////////////



static useret u8
_STEPPER_calculate_crc(u8* data, u8 length)
{

    // The way that the TMC2209 seems to be... wrong.
    // The CRC checksum should have it where, if the CRC
    // value is filled in at the end of the payload, the
    // resulting value will be zero. For some reason,
    // the implementation of TMC2209 forces you to actually
    // do the comparison of the computed CRC and expected CRC,
    // rather than whether or not it's actually non-zero.

    // CRC8 polynomial: 0x7.
    // Input reflected.
    // Table reflected.

    static const u8 CRC_TABLE[] =
        {
            0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75,
            0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
            0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,
            0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
            0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D,
            0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
            0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51,
            0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
            0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,
            0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
            0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
            0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
            0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D,
            0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
            0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,
            0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
            0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95,
            0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
            0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89,
            0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
            0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,
            0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
            0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1,
            0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
            0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5,
            0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
            0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,
            0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
            0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD,
            0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
            0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1,
            0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF,
        };

    u8 crc = 0;

    for (i32 i = 0; i < length; i += 1)
    {
        crc = CRC_TABLE[data[i] ^ crc];
    }

    crc =
        (((crc >> 7) & 1) << 0) |
        (((crc >> 6) & 1) << 1) |
        (((crc >> 5) & 1) << 2) |
        (((crc >> 4) & 1) << 3) |
        (((crc >> 3) & 1) << 4) |
        (((crc >> 2) & 1) << 5) |
        (((crc >> 1) & 1) << 6) |
        (((crc >> 0) & 1) << 7);

    return crc;

}



static void
STEPPER_blocking_read_request(enum StepperHandle handle, u8 register_address)
{

    _EXPAND_HANDLE

    if (register_address & (1 << 7))
        panic;



    // Set up the request.

    struct StepperReadRequest request =
        {
            .sync             = 0b0000'0101,
            .node_address     = STEPPER_NODE_ADDRESS,
            .register_address = register_address,
            .crc              = 0,
        };

    request.crc =
        _STEPPER_calculate_crc
        (
            (u8*) &request,
            sizeof(request) - sizeof(request.crc)
        );



    // Send the request.

    _UXART_tx_raw_nonreentrant
    (
        UXARTHandle_stepper_uart,
        (u8*) &request,
        sizeof(request)
    );



    // Flush the RX-FIFO.
    // TODO Don't use char.

    while (UXART_rx(UXARTHandle_stepper_uart, &(char) {0}));



    // TODO Return error code?

}



static void
STEPPER_blocking_read_response(enum StepperHandle handle, u8 register_address, u32* dst)
{

    if (register_address & (1 << 7))
        panic;

    if (!dst)
        panic;

    *dst = 0;



    // Get the response.

    struct StepperReadResponse response = {0};

    for (i32 i = 0; i < sizeof(response); i += 1)
    {
        while (!UXART_rx(UXARTHandle_stepper_uart, &((char*) &response)[i]));
    }



    // Verify integrity of response.

    u8 digest = _STEPPER_calculate_crc((u8*) &response, sizeof(response) - sizeof(response.crc));

    if (digest != response.crc)
        panic;

    if (response.sync != 0b0000'0101)
        panic;

    if (response.master_address != 0xFF)
        panic;

    if (response.register_address != register_address)
        panic;



    // Got the register data intact!

    *dst = __builtin_bswap32(response.data);



    // TODO Return error code.

}



static void
STEPPER_blocking_write_request(enum StepperHandle handle, u8 register_address, u32 data)
{

    _EXPAND_HANDLE

    if (register_address & (1 << 7))
        panic;



    // Set up the request.

    struct StepperWriteRequest request =
        {
            .sync             = 0b0000'0101,
            .node_address     = STEPPER_NODE_ADDRESS,
            .register_address = register_address | (1 << 7),
            .data             = __builtin_bswap32(data),
            .crc              = 0,
        };

    request.crc =
        _STEPPER_calculate_crc
        (
            (u8*) &request,
            sizeof(request) - sizeof(request.crc)
        );



    // Send the request.

    _UXART_tx_raw_nonreentrant
    (
        UXARTHandle_stepper_uart,
        (u8*) &request,
        sizeof(request)
    );



    // TODO Return error code?

}



static void
STEPPER_partial_init(enum StepperHandle handle)
{

    _EXPAND_HANDLE



    // Enable the peripheral.

    CMSIS_PUT(TIMx_ENABLE, true);



    // Channel output in PWM mode so we can generate a pulse.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_MODE_y, 0b0111);



    // The comparison channel output is inactive
    // when the counter is below this value.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_VALUE_y, 1);



    // Enable the comparison channel output.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_ENABLE_y, true);



    // Master enable for the timer's outputs.

    CMSIS_SET(TIMx, BDTR, MOE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMx, PSC, PSC, STPY_TIMx_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMx, EGR, UG, true);



    CMSIS_SET
    (
        TIMx, CR1 ,
        URS , 0b1 , // So that the UG bit doesn't set the update event interrupt.
        OPM , true, // Timer's counter stops incrementing after an update event.
    );



    // Enable interrupt on update events.

    CMSIS_SET(TIMx, DIER, UIE, true);
    NVIC_ENABLE(TIMx_update_event);

}



static useret b32
STEPPER_push_delta(enum StepperHandle handle, i8 delta)
{

    _EXPAND_HANDLE

    b32 has_space = driver->writer - driver->reader < countof(driver->deltas);

    if (has_space)
    {
        driver->deltas[driver->writer % countof(driver->deltas)]  = delta;
        driver->writer                                           += 1;
    }

    return has_space;

}



static void
_STEPPER_driver_interrupt(enum StepperHandle handle)
{

    _EXPAND_HANDLE



    // See if the timer has finished pulsing steps;
    // if so, the next sequence of steps can be queued up now.

    if (CMSIS_GET(TIMx, SR, UIF))
    {

        CMSIS_SET(TIMx, SR, UIF, false); // Acknowledge timer's update flag.



        // Determine the amount of steps we'll
        // need to take now and the direction.

        i32 steps = {0};

        if (driver->reader == driver->writer)
        {
            // Underflow condition.
            // The step ring-buffer was exhausted and
            // no steps will be taken during this period.
            // TODO Indicate this situation somehow.
            steps = 0;
        }
        else
        {
            // Pop from the ring-buffer the next
            // signed amount of steps to take.
            i32 read_index  = driver->reader % countof(driver->deltas);
            steps           = driver->deltas[read_index];
            driver->reader += 1;
        }

        i32 abs_steps = steps < 0 ? -steps : steps;



        // Set the direction arbitrarily.

        GPIO_SET(driver_direction, steps > 0);



        // Set the amount of step pulses to be done.
        // Note that this field has it so a value of
        // 0 encodes a repetition of 1, so in the event
        // of no steps to be done, there still needs to
        // be a repetition.

        CMSIS_SET(TIMx, RCR, REP, abs_steps ? (abs_steps - 1) : 0);



        // So to prevent a pulse in the case of zero steps,
        // the comparison value is changed to the largest
        // value so the pulse won't occur.
        // In any other case, the comparison value is non-zero
        // so that the waveform has its rising edge slighly
        // delayed from when the DIR signal is updated.

        CMSIS_PUT(TIMx_CAPTURE_COMPARE_VALUE_y, (abs_steps ? 1 : -1));



        // The amount of time between each step pulse
        // is set so that after N steps, a fixed amount
        // of time has passed. This allows us to update
        // the direction and rate of stepping at fixed
        // intervals.

        CMSIS_SET(TIMx, ARR, ARR, (STEPPER_PERIOD_US - 1) / (abs_steps ? abs_steps : 1));



        // Generate an update event so the above configuration
        // will be loaded into the timer's shadow registers.

        CMSIS_SET(TIMx, EGR, UG, true);



        // Enable the timer's counter; the counter will
        // become disabled after the end of step repetitions.

        CMSIS_SET(TIMx, CR1, CEN, true);



        // TODO.

        for (b32 yield = false; !yield;)
        {

            static i32 TMP = 0;
            GPIO_SET(debug, TMP == 0);

            switch (driver->uart.state)
            {

                case StepperDriverUARTState_standby:
                {

                    switch (TMP)
                    {

                        case 0:
                        {
                            driver->uart =
                                (struct StepperDriverUART)
                                {
                                    .state            = StepperDriverUARTState_read_scheduled,
                                    .register_address = 0x00,
                                };
                        } break;

                        case 1:
                        {
                            driver->uart =
                                (struct StepperDriverUART)
                                {
                                    .state            = StepperDriverUARTState_write_scheduled,
                                    .register_address = 0x00,
                                    .data             = (driver->uart.data | (1 << 6)) ^ (1 << 3),
                                };
                        } break;

                        case 2:
                        {
                            driver->uart =
                                (struct StepperDriverUART)
                                {
                                    .state            = StepperDriverUARTState_read_scheduled,
                                    .register_address = 0x00,
                                };
                        } break;

                        default: panic;

                    }

                } break;

                case StepperDriverUARTState_done:
                {

                    driver->uart.state = StepperDriverUARTState_standby;

                    TMP += 1;
                    TMP %= 3;

                } break;

                case StepperDriverUARTState_read_scheduled:
                {

                    STEPPER_blocking_read_request(handle, driver->uart.register_address);

                    driver->uart.state = StepperDriverUARTState_read_requested;

                    yield = true;

                } break;

                case StepperDriverUARTState_read_requested:
                {

                    STEPPER_blocking_read_response(handle, driver->uart.register_address, &driver->uart.data);

                    driver->uart.state = StepperDriverUARTState_done;

                    yield = true;

                } break;

                case StepperDriverUARTState_write_scheduled:
                {

                    STEPPER_blocking_write_request(handle, driver->uart.register_address, driver->uart.data);

                    driver->uart.state = StepperDriverUARTState_done;

                    yield = true;

                } break;

                default: panic;

            }

        }

    }

}
