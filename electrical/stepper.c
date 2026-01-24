////////////////////////////////////////////////////////////////////////////////



#define STEPPER_ENABLE_DELAY_MS     500 // @/`Stepper Enable Delay`.
#define STEPPER_VELOCITY_UPDATE_MS   25 // @/`Stepper Updating Velocity`.
#define STEPPER_UART_TIME_BUFFER_MS   2 // @/`Stepper UART Time Buffer Window`.
#define STEPPER_RING_BUFFER_LENGTH   32 // @/`Stepper Ring-Buffer Length`.

static_assert(IS_POWER_OF_TWO(STEPPER_RING_BUFFER_LENGTH));

#define TMC2209_IFCNT_ADDRESS 0x02

#include "stepper_driver_support.meta"
/* #meta

    for target in PER_TARGET():

        # Ensure there's a unique stepper driver.

        driver = [
            driver
            for driver in target.drivers
            if driver['type'] == 'Stepper'
        ]

        if len(driver) != 1:
            Meta.line(f'#error Target {repr(target.name)} needs to define a unique {repr('Stepper')} driver.')
            continue

        driver, = driver



        # Supporting definitions.

        Meta.enums('StepperInstanceHandle', 'u32', (
            name
            for name, address in driver['instances']
        ))

        Meta.lut('STEPPER_INSTANCE_TABLE', ((
            ('address', address),
        ) for name, address in driver['instances']))

        # TODO Inconvenient:
        Meta.define('STEPPER_UXART_HANDLE'                   , f'UXARTHandle_{driver['uxart_handle']}')
        Meta.define('STEPPER_MOTOR_ENABLE_GPIO_NAME'         , driver['enable_gpio'])
        Meta.define('STEPPER_TIMx'                           ,                         f'{driver['timer_peripheral']}'           )
        Meta.define('STEPPER_TIMx_'                          ,                         f'{driver['timer_peripheral']}_'          )
        Meta.define('STEPPER_TIMx_ENABLE'                    , CMSIS_TUPLE(target.mcu, f'{driver['timer_peripheral']}_ENABLE' )  )
        Meta.define('STEPPER_STPY_TIMx_DIVIDER'              ,                    f'STPY_{driver['timer_peripheral']}_DIVIDER'   )
        Meta.define('STEPPER_STPY_TIMx_MODULATION'           ,                    f'STPY_{driver['timer_peripheral']}_MODULATION')
        Meta.define('NVICInterrupt_STEPPER_TIMx_update_event',          f'NVICInterrupt_{driver['timer_update_event_interrupt']}')
        Meta.define('INTERRUPT_STEPPER_TIMx_update_event'    ,              f'INTERRUPT_{driver['timer_update_event_interrupt']}')

*/



enum StepperMicrostepResolution : u32
{
    StepperMicrostepResolution_256 = 0b0000,
    StepperMicrostepResolution_128 = 0b0001,
    StepperMicrostepResolution_64  = 0b0010,
    StepperMicrostepResolution_32  = 0b0011,
    StepperMicrostepResolution_16  = 0b0100,
    StepperMicrostepResolution_8   = 0b0101,
    StepperMicrostepResolution_4   = 0b0110,
    StepperMicrostepResolution_2   = 0b0111,
    StepperMicrostepResolution_1   = 0b1000,
};



static const struct { u8 register_address; u32 data; } STEPPER_INITIALIZATION_SEQUENCE[] =
    {
        {
            0x00,
              (1 << 0) // "I_scale_analog"   : Whether or not to use VREF as current reference.
            | (0 << 2) // "en_SpreadCycle"   : Whether or not to only use SpreadCycle (louder and more power, but more torque); otherwise, a mix of StealthChop and SpreadCycle is done.
            | (1 << 6) // "pdn_disable"      : The power-down and UART functionality both share the same pin, so we disable the former.
            | (1 << 7) // "mstep_reg_select" : Microstep resolution determined by a register field rather than the MS1/MS2 pins.
            | (1 << 8) // "multistep_filt"   : Apply filtering to the STEP signal.
        },
        {
            0x03,
            (2 << 8) // "SENDDELAY" : Amount of delay before the read response is sent back.
        },
        {
            0x10,
              ((5 - 1) << 0) // "IHOLD" : Standstill current (out of 32) for when the motor is not turning.
            | ((5 - 1) << 8) // "IRUN"  : Current scaling (out of 32) for when the motor is turning.
        },
        {
            0x6C,
              (0                            << 31) // "diss2vs" : "0: Short protection low side is on".
            | (0                            << 30) // "diss2g"  : "0: Short to GND protection is on".
            | (1                            << 28) // "intpol"  : The actual microstep resolution is "extrapolated to 256 microsteps for smoothest motor operation".
            | (StepperMicrostepResolution_8 << 24) // "MRES"    : Microstep resolution. TODO Subject to change?
            | (5                            <<  4) // "HSTRT"   : Addend to "hysteresis low value HEND".
            | (3                            <<  0) // "TOFF"    : "Off time setting controls duration of slow decay phase".
        },
    };



enum StepperInstanceUARTTransferState : u32
{
    StepperInstanceUARTTransferState_standby,
    StepperInstanceUARTTransferState_read_scheduled,
    StepperInstanceUARTTransferState_read_requested,
    StepperInstanceUARTTransferState_write_scheduled,
    StepperInstanceUARTTransferState_write_verification_read_scheduled,
    StepperInstanceUARTTransferState_write_verification_read_requested,
    StepperInstanceUARTTransferState_done,
};



enum StepperInstanceState : u32
{
    StepperInstanceState_setting_uart_write_sequence_number,
    StepperInstanceState_doing_initialization_sequence,
    StepperInstanceState_delaying_enable,
    StepperInstanceState_working,
};



struct StepperInstance
{

    volatile enum StepperInstanceState state;
    u32                                incremental_timestamp_ms;
    i32                                initialization_sequence_index;
    i32                                velocities[STEPPER_RING_BUFFER_LENGTH];
    volatile u32                       velocity_reader;
    volatile u32                       velocity_writer;

    struct StepperInstanceUARTTransfer
    {
        enum StepperInstanceUARTTransferState state;
        u8                                  register_address;
        u32                                 data;
    }   uart_transfer;
    u8  uart_write_sequence_number;
    u32 uart_previous_transfer_timestamp_ms;

};



struct StepperDriver
{
    struct StepperInstance     instances[StepperInstanceHandle_COUNT];
    enum StepperInstanceHandle current_instance_handle;
};

static struct StepperDriver _STEPPER_driver = {0};



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



static useret b32
STEPPER_push_velocity(enum StepperInstanceHandle handle, i32 velocity)
{

    struct StepperInstance* instance = &_STEPPER_driver.instances[handle];

    b32 can_be_pushed =
        instance->state == StepperInstanceState_working &&
        (instance->velocity_writer - instance->velocity_reader < countof(instance->velocities));

    if (can_be_pushed)
    {
        instance->velocities[instance->velocity_writer % countof(instance->velocities)] = velocity;
        instance->velocity_writer += 1;
    }

    return can_be_pushed;

}



static useret enum StepperUpdateInstanceResult : u32
{
    StepperUpdateInstanceResult_relinquished,
    StepperUpdateInstanceResult_busy,
}
_STEPPER_update_instance(enum StepperInstanceHandle handle, u32 current_timestamp_ms)
{

    struct StepperInstance* instance = &_STEPPER_driver.instances[handle];

    while (true)
    {
        switch (instance->uart_transfer.state)
        {

            ////////////////////////////////////////////////////////////////////////////////
            //
            // The next UART transfer can be scheduled.
            //

            case StepperInstanceUARTTransferState_standby: switch (instance->state)
            {



                // This has to be the first thing we do
                // for the configuration of the TMC2209
                // so that we can know whether or not the
                // first write request is successful.

                case StepperInstanceState_setting_uart_write_sequence_number:
                {

                    instance->uart_transfer =
                        (struct StepperInstanceUARTTransfer)
                        {
                            .state            = StepperInstanceUARTTransferState_read_scheduled,
                            .register_address = TMC2209_IFCNT_ADDRESS,
                        };

                } break;



                // We then do a series of register writes to configure the TMC2209.

                case StepperInstanceState_doing_initialization_sequence:
                {

                    instance->uart_transfer =
                        (struct StepperInstanceUARTTransfer)
                        {
                            .state            = StepperInstanceUARTTransferState_write_scheduled,
                            .register_address = STEPPER_INITIALIZATION_SEQUENCE[instance->initialization_sequence_index].register_address,
                            .data             = STEPPER_INITIALIZATION_SEQUENCE[instance->initialization_sequence_index].data,
                        };

                } break;



                // @/`Stepper Enable Delay`:
                //
                // It seems like when the TMC2209 is first
                // initialized (especially after a power-cycle)
                // the motor will induce a large current draw
                // after it is enabled. It quickly settles down,
                // but it's probably not nice to do to the batteries.
                // It seems like by delaying the enabling of the motor
                // that the current spike can be avoided. I can tell
                // this works by the fact that the power supply not
                // going into current-limiting mode after the power-cycle.

                case StepperInstanceState_delaying_enable:
                {

                    b32 delaying = (current_timestamp_ms - instance->incremental_timestamp_ms) < STEPPER_ENABLE_DELAY_MS;

                    if (delaying)
                    {
                        return StepperUpdateInstanceResult_relinquished;
                    }
                    else
                    {
                        instance->state                    = StepperInstanceState_working;
                        instance->incremental_timestamp_ms = current_timestamp_ms;
                    }

                } break;



                // @/`Stepper Updating Velocity`:
                //
                // The TMC2209 is fully configured now and step
                // velocities can be set. We try our best to
                // update the step velocity as consistently as
                // possible based on how much time has passed
                // since the previous velocity update.
                //
                // The user should also try their best to have
                // the velocity ring buffer as full as possible
                // to avoid an underrun situation.

                case StepperInstanceState_working:
                {

                    b32 should_update = (current_timestamp_ms - instance->incremental_timestamp_ms) >= STEPPER_VELOCITY_UPDATE_MS;

                    if (should_update)
                    {

                        instance->incremental_timestamp_ms += STEPPER_VELOCITY_UPDATE_MS;

                        i32 velocity = {0};

                        if (instance->velocity_reader == instance->velocity_writer)
                        {
                            // Underflow condition.
                            // TODO Indicate this situation somehow?
                            // TODO Perhaps keep the same velocity instead.
                            velocity = 0;
                        }
                        else
                        {
                            i32 read_index             = instance->velocity_reader % countof(instance->velocities);
                            velocity                   = instance->velocities[read_index];
                            instance->velocity_reader += 1;
                        }

                        instance->uart_transfer =
                            (struct StepperInstanceUARTTransfer)
                            {
                                .state            = StepperInstanceUARTTransferState_write_scheduled,
                                .register_address = 0x22, // TODO.
                                .data             = velocity,
                            };

                    }
                    else
                    {
                        return StepperUpdateInstanceResult_relinquished;
                    }

                } break;



                default: panic;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We just finished a UART transfer.
            //

            case StepperInstanceUARTTransferState_done:
            {

                switch (instance->state)
                {



                    // We now know what the first write
                    // request's sequence number will be.

                    case StepperInstanceState_setting_uart_write_sequence_number:
                    {
                        instance->uart_write_sequence_number = instance->uart_transfer.data + 1;
                        instance->state                      = StepperInstanceState_doing_initialization_sequence;
                    } break;



                    // See if we're done doing the series of
                    // register writes to configure the TMC2209.

                    case StepperInstanceState_doing_initialization_sequence:
                    {

                        instance->initialization_sequence_index += 1;

                        if (instance->initialization_sequence_index < countof(STEPPER_INITIALIZATION_SEQUENCE))
                        {
                            // We still have more registers to write to.
                        }
                        else
                        {
                            instance->state                    = StepperInstanceState_delaying_enable;
                            instance->incremental_timestamp_ms = current_timestamp_ms;
                        }

                    } break;



                    // We're done updating the motor's step velocity.

                    case StepperInstanceState_working:
                    {
                        // Don't care.
                    } break;



                    case StepperInstanceState_delaying_enable : panic;
                    default                                   : panic;

                }

                instance->uart_transfer.state = StepperInstanceUARTTransferState_standby;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We send a read request packet to the TMC2209.
            //

            case StepperInstanceUARTTransferState_read_scheduled:
            case StepperInstanceUARTTransferState_write_verification_read_scheduled:
            {

                if (instance->uart_transfer.register_address & (1 << 7))
                    panic;

                if (current_timestamp_ms - instance->uart_previous_transfer_timestamp_ms < STEPPER_UART_TIME_BUFFER_MS)
                {
                    return StepperUpdateInstanceResult_busy; // @/`Stepper UART Time Buffer Window`.
                }
                else
                {

                    // Set up the request.

                    pack_push
                        struct StepperReadRequest
                        {
                            u8 sync;
                            u8 node_address;
                            u8 register_address;
                            u8 crc;
                        };
                    pack_pop

                    struct StepperReadRequest request =
                        {
                            .sync             = 0b0000'0101,
                            .node_address     = STEPPER_INSTANCE_TABLE[handle].address,
                            .register_address =
                                instance->uart_transfer.state == StepperInstanceUARTTransferState_write_verification_read_scheduled
                                    ? TMC2209_IFCNT_ADDRESS
                                    : instance->uart_transfer.register_address,
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
                        STEPPER_UXART_HANDLE,
                        (u8*) &request,
                        sizeof(request)
                    );



                    // Flush the RX-FIFO.
                    // TODO Don't use char.

                    while (UXART_rx(STEPPER_UXART_HANDLE, &(char) {0}));



                    // We now wait for the response.

                    instance->uart_transfer.state =
                        instance->uart_transfer.state == StepperInstanceUARTTransferState_write_verification_read_scheduled
                            ? StepperInstanceUARTTransferState_write_verification_read_requested
                            : StepperInstanceUARTTransferState_read_requested;

                    instance->uart_previous_transfer_timestamp_ms = current_timestamp_ms;

                    return StepperUpdateInstanceResult_busy;

                }

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // See if the TMC2209 replied back to the read request.
            //

            case StepperInstanceUARTTransferState_read_requested:
            case StepperInstanceUARTTransferState_write_verification_read_requested:
            {

                if (instance->uart_transfer.register_address & (1 << 7))
                    panic;

                if (current_timestamp_ms - instance->uart_previous_transfer_timestamp_ms < STEPPER_UART_TIME_BUFFER_MS)
                {
                    return StepperUpdateInstanceResult_busy; // @/`Stepper UART Time Buffer Window`.
                }
                else
                {

                    // Get the response.

                    pack_push
                        struct StepperReadResponse
                        {
                            u8  sync;
                            u8  master_address;
                            u8  register_address;
                            u32 data; // Big-endian.
                            u8  crc;
                        };
                    pack_pop

                    struct StepperReadResponse response = {0};

                    for (i32 i = 0; i < sizeof(response); i += 1)
                    {
                        if (!UXART_rx(STEPPER_UXART_HANDLE, &((char*) &response)[i])) // TODO Not use char.
                            sorry
                    }



                    // Verify integrity of response.

                    u8 digest = _STEPPER_calculate_crc((u8*) &response, sizeof(response) - sizeof(response.crc));

                    if (digest != response.crc)
                        sorry

                    if (response.sync != 0b0000'0101)
                        sorry

                    if (response.master_address != 0xFF)
                        sorry

                    u8 expected_register_address =
                        instance->uart_transfer.state == StepperInstanceUARTTransferState_write_verification_read_requested
                            ? TMC2209_IFCNT_ADDRESS
                            : instance->uart_transfer.register_address;

                    if (response.register_address != expected_register_address)
                        sorry



                    // Got the register data intact!

                    u32 data = __builtin_bswap32(response.data);



                    // Handle the data.

                    if (instance->uart_transfer.state == StepperInstanceUARTTransferState_write_verification_read_requested)
                    {

                        if (instance->uart_write_sequence_number != data)
                            sorry

                        instance->uart_write_sequence_number = data + 1;

                    }
                    else
                    {
                        instance->uart_transfer.data = data;
                    }



                    // We're now done reading the register
                    // (or maybe verifying that the write request was successful).

                    instance->uart_transfer.state = StepperInstanceUARTTransferState_done;

                }

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We send a write request packet to the TMC2209.
            //

            case StepperInstanceUARTTransferState_write_scheduled:
            {

                if (instance->uart_transfer.register_address & (1 << 7))
                    panic;


                if (current_timestamp_ms - instance->uart_previous_transfer_timestamp_ms < STEPPER_UART_TIME_BUFFER_MS)
                {
                    return StepperUpdateInstanceResult_busy; // @/`Stepper UART Time Buffer Window`.
                }
                else
                {

                    // Set up the request.

                    pack_push
                        struct StepperWriteRequest
                        {
                            u8  sync;
                            u8  node_address;
                            u8  register_address;
                            u32 data; // Big-endian.
                            u8  crc;
                        };
                    pack_pop

                    struct StepperWriteRequest request =
                        {
                            .sync             = 0b0000'0101,
                            .node_address     = STEPPER_INSTANCE_TABLE[handle].address,
                            .register_address = instance->uart_transfer.register_address | (1 << 7),
                            .data             = __builtin_bswap32(instance->uart_transfer.data),
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
                        STEPPER_UXART_HANDLE,
                        (u8*) &request,
                        sizeof(request)
                    );



                    // After the write transfer, we read the
                    // interface transmission counter register
                    // to ensure the write request went through.

                    instance->uart_transfer.state = StepperInstanceUARTTransferState_write_verification_read_scheduled;

                    instance->uart_previous_transfer_timestamp_ms = current_timestamp_ms;

                    return StepperUpdateInstanceResult_busy;

                }

            } break;



            default: panic;

        }
    }

}



static void
STEPPER_update_all(u32 current_timestamp_ms)
{

    // Update the motor that's currently in control of the UXART handle.

    enum StepperUpdateInstanceResult result =
        _STEPPER_update_instance
        (
            _STEPPER_driver.current_instance_handle,
            current_timestamp_ms
        );

    switch (result)
    {

        case StepperUpdateInstanceResult_relinquished:
        {

            // Move onto the next motor round-robin style.

            _STEPPER_driver.current_instance_handle += 1;
            _STEPPER_driver.current_instance_handle %= StepperInstanceHandle_COUNT;

        } break;

        case StepperUpdateInstanceResult_busy:
        {
            // TMC2209 is currently in control of the UXART handle.
        } break;

        default: panic;

    }



    // We'll only enable the motors once all
    // of the TMC2209s are done initializing.

    b32 all_motors_ready = true;

    for (enum StepperInstanceHandle handle = {0}; handle < StepperInstanceHandle_COUNT; handle += 1)
    {
        if (_STEPPER_driver.instances[handle].state != StepperInstanceState_working)
        {
            all_motors_ready = false;
            break;
        }
    }

    GPIO_SET(STEPPER_MOTOR_ENABLE_GPIO_NAME, all_motors_ready);

    GPIO_SET(debug, result == StepperUpdateInstanceResult_busy);

}



static void
STEPPER_partial_init(void)
{

    // Enable the peripheral.

    CMSIS_PUT(STEPPER_TIMx_ENABLE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(STEPPER_TIMx, PSC, PSC, STEPPER_STPY_TIMx_DIVIDER);



    // Set the value at which the timer's counter
    // will reach and then reset; this is when an
    // update event happens.

    CMSIS_SET(STEPPER_TIMx, ARR, ARR, STEPPER_STPY_TIMx_MODULATION);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(STEPPER_TIMx, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(STEPPER_TIMx, CR1, CEN, true);



    // Enable interrupt on update events.

    CMSIS_SET(STEPPER_TIMx, DIER, UIE, true);
    NVIC_ENABLE(STEPPER_TIMx_update_event);

}



INTERRUPT_STEPPER_TIMx_update_event(void)
{
    if (CMSIS_GET(STEPPER_TIMx, SR, UIF))
    {

        CMSIS_SET(STEPPER_TIMx, SR, UIF, false); // Acknowledge timer's update flag.

        static u32 current_timestamp_ms = 0;
        current_timestamp_ms += 1;

        STEPPER_update_all(current_timestamp_ms);

    }
}



////////////////////////////////////////////////////////////////////////////////



// @/`Stepper Ring-Buffer Length`: TODO.
//
// The length of the ring-buffer should be set to a reasonable
// size such that steps can be queued up but without implying
// large latency.
//
// For example, if the window length is 256 and the update period
// is 25ms, then if the ring-buffer were to be completely filled,
// then it'd take 6.4 seconds for the entire ring-buffer to be
// processed; this is an impractical size.
//
// However, if the ring-buffer is too small, then there's a risk
// of an underrun where the driver has no value to pop from the
// ring-buffer and doesn't know how many pulses to send out to
// the TMC2209. This is unlikely given that the update period is
// on the order of milliseconds, and a lot can be done during that
// time, but it's something to consider.



// @/`Stepper UART Time Buffer Window`:
//
// Because the TMC2209 uses half-duplex UART communication,
// we have to be careful about not expecting data from the
// TMC2209 too soon or sending commands too fast.
//
// The former is obvious: after a read request is made, the
// TMC2209 takes control of the single data line to send the
// data back. We have to wait long enough to ensure that the
// TMC2209 successfully interprets and sends the entire payload
// back.
//
// The latter situation of sending commands too fast is a bit
// more subtle. In the event of communication failure, the
// UART communication can be resynchronized with the TMC2209
// by leaving the data line idle for long enough.
//
// Thus in the event of a CRC error, for instance, we're going
// to have to idly wait on the UART line, so we might as well
// always do it to ensure the upmost reliability.
//
// Because of that, it's also important to note that during the
// window of time that the stepper driver is waiting to send the
// next UART transfer or waiting for the TMC2209's response,
// that no other users of the UXART handle can take control of
// the UART; otherwise, the UART data line won't be idle for
// long enough.
