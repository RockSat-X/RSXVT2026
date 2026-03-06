#define STEPPER_ENABLE_DELAY_US     500'000 // @/`Stepper Enable Delay`.
#define STEPPER_VELOCITY_UPDATE_US   25'000 // @/`Stepper Updating Velocity`.
#define STEPPER_UART_TIME_MARGIN_US   2'000 // @/`Stepper UART Time Margin Window`.
#define STEPPER_RING_BUFFER_LENGTH        8 // @/`Stepper Ring-Buffer Length`.

enum StepperMicrostepResolution : u32 // @/pg 33/tbl 5.5.1/`TMC2209`.
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

// @/`Stepper Microstepping and Step Velocity`.
#define STEPPER_MICROSTEP_RESOLUTION StepperMicrostepResolution_8
#define STEPPER_STEPS_PER_REVOLUTION ((f32) (((1 << (8 - STEPPER_MICROSTEP_RESOLUTION)) * 200) / 0.715f))

static const struct StepperInitializationSequenceEntry { u8 register_address; u32 data; } STEPPER_INITIALIZATION_SEQUENCE[] =
    {
        {
            0x00,      // "GCONF"            : @/pg 23/tbl 5.1/`TMC2209`.
              (1 << 0) // "I_scale_analog"   : Whether or not to use VREF as current reference.
            | (0 << 2) // "en_SpreadCycle"   : Whether or not to only use SpreadCycle (louder and more power, but more torque); otherwise, a mix of StealthChop and SpreadCycle is done.
            | (1 << 6) // "pdn_disable"      : The power-down and UART functionality both share the same pin, so we disable the former.
            | (1 << 7) // "mstep_reg_select" : Microstep resolution determined by a register field rather than the MS1/MS2 pins.
            | (1 << 8) // "multistep_filt"   : Apply filtering to the STEP signal.
        },
        {
            0x03,    // "NODECONF"  : @/pg 24/tbl 5.1/`TMC2209`.
            (2 << 8) // "SENDDELAY" : Amount of delay before the read response is sent back.
        },
        {
            0x10,            // "IHOLD_IRUN" : @/pg 28/tbl 5.2/`TMC2209`.
              ((5 - 1) << 0) // "IHOLD"      : Standstill current (out of 32) for when the motor is not turning.
            | ((5 - 1) << 8) // "IRUN"       : Current scaling (out of 32) for when the motor is turning.
        },
        {
            0x6C,                                  // "CHOPCONF" : @/pg 33/tbl 5.5.1/`TMC2209`.
              (0                            << 31) // "diss2vs"  : "0: Short protection low side is on".
            | (0                            << 30) // "diss2g"   : "0: Short to GND protection is on".
            | (1                            << 28) // "intpol"   : The actual microstep resolution is "extrapolated to 256 microsteps for smoothest motor operation".
            | (STEPPER_MICROSTEP_RESOLUTION << 24) // "MRES"     : Microstep resolution.
            | (5                            <<  4) // "HSTRT"    : Addend to "hysteresis low value HEND".
            | (3                            <<  0) // "TOFF"     : "Off time setting controls duration of slow decay phase".
        },
    };

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

        Meta.enums('StepperUnit', 'u32', (
            name
            for name, address in driver['units']
        ))

        Meta.lut('STEPPER_INSTANCE_TABLE', ((
            ('u8', 'address', address),
        ) for name, address in driver['units']))

        # TODO Inconvenient:
        Meta.define('STEPPER_UXART_HANDLE'                   , f'UXARTHandle_{driver['uxart_handle']}')
        Meta.define('STEPPER_MOTOR_ENABLE'                   , driver['enable_gpio'])
        Meta.define('STEPPER_TIMx'                           ,                         f'{driver['timer_peripheral']}'           )
        Meta.define('STEPPER_TIMx_'                          ,                         f'{driver['timer_peripheral']}_'          )
        Meta.define('STEPPER_TIMx_RESET'                     , CMSIS_TUPLE(target.mcu, f'{driver['timer_peripheral']}_RESET' )   )
        Meta.define('STEPPER_TIMx_ENABLE'                    , CMSIS_TUPLE(target.mcu, f'{driver['timer_peripheral']}_ENABLE')   )
        Meta.define('STEPPER_STPY_TIMx_DIVIDER'              ,                    f'STPY_{driver['timer_peripheral']}_DIVIDER'   )
        Meta.define('STEPPER_STPY_TIMx_MODULATION'           ,                    f'STPY_{driver['timer_peripheral']}_MODULATION')
        Meta.define('NVICInterrupt_STEPPER_TIMx_update_event',          f'NVICInterrupt_{driver['timer_update_event_interrupt']}')
        Meta.define('INTERRUPT_STEPPER_TIMx_update_event'    ,              f'INTERRUPT_{driver['timer_update_event_interrupt']}')
        Meta.define('STEPPER_UPDATE_EVENT_PERIOD_US'         , round(1_000_000 / target.schema[f'{driver['timer_peripheral']}_UPDATE_RATE']))

*/

#define STEPPER_TMC2209_IFCNT_ADDRESS   0x02 // @/pg 24/tbl 5.1/`TMC2209`.
#define STEPPER_TMC2209_VACTUAL_ADDRESS 0x22 // @/pg 28/tbl 5.2/`TMC2209`.

static_assert(STEPPER_ENABLE_DELAY_US     % STEPPER_UPDATE_EVENT_PERIOD_US == 0);
static_assert(STEPPER_VELOCITY_UPDATE_US  % STEPPER_UPDATE_EVENT_PERIOD_US == 0);
static_assert(STEPPER_UART_TIME_MARGIN_US % STEPPER_UPDATE_EVENT_PERIOD_US == 0);

typedef f32 StepperAngularVelocities[StepperUnit_COUNT];

enum StepperUARTState : u32
{
    StepperUARTState_standby,
    StepperUARTState_read_scheduled,
    StepperUARTState_read_requested,
    StepperUARTState_write_scheduled,
    StepperUARTState_write_verification_read_scheduled,
    StepperUARTState_write_verification_read_requested,
    StepperUARTState_done,
};

enum StepperDriverState : u32
{
    StepperDriverState_disabled,
    StepperDriverState_initializing_uart_write_sequence_numbers,
    StepperDriverState_doing_initialization_sequences,
    StepperDriverState_delaying_enable,
    StepperDriverState_waiting_on_angular_velocities,
    StepperDriverState_updating_angular_velocities,
    StepperDriverState_no_response_from_unit,
    StepperDriverState_bad_response_from_unit,
};

struct StepperDriver
{

    volatile _Atomic enum StepperDriverState                         atomic_state;
    u32                                                              current_timestamp_us;
    enum StepperUnit                                                 current_unit;
    u8                                                               uart_write_sequence_numbers[StepperUnit_COUNT];
    RingBuffer(StepperAngularVelocities, STEPPER_RING_BUFFER_LENGTH) queued_angular_velocities;
    i32                                                              initialization_sequence_index;
    u32                                                              most_recent_angular_velocity_update_timestamp_us;
    u32                                                              most_recent_uart_transfer_timestamp_us;

    struct StepperUART
    {
        enum StepperUARTState state;
        u8                    register_address; // MSb should always be cleared; it's set automatically if needed.
        u32                   data;
    } uart;

};

static struct StepperDriver _STEPPER_driver = {0};



////////////////////////////////////////////////////////////////////////////////



static void
STEPPER_reinit(void)
{

    // Reset stuff.

    GPIO_INACTIVE(STEPPER_MOTOR_ENABLE);

    NVIC_DISABLE(STEPPER_TIMx_update_event);

    CMSIS_PUT(STEPPER_TIMx_RESET, true );
    CMSIS_PUT(STEPPER_TIMx_RESET, false);

    memzero(&_STEPPER_driver);

    UXART_reinit(STEPPER_UXART_HANDLE);



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
    // ARR and PSC are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(STEPPER_TIMx, CR1, URS, true); // Make it so that setting `UG` doesn't actually make an interrupt event.
    CMSIS_SET(STEPPER_TIMx, EGR, UG , true);



    // Enable the timer's counter.

    CMSIS_SET(STEPPER_TIMx, CR1, CEN, true);



    // Enable interrupt on update events.
    // Once a unit of time has passed, we can begin initializing TMC2209s.

    CMSIS_SET(STEPPER_TIMx, DIER, UIE, true);

    atomic_store_explicit
    (
        &_STEPPER_driver.atomic_state,
        StepperDriverState_initializing_uart_write_sequence_numbers,
        memory_order_relaxed // No synchronization needed.
    );

    NVIC_ENABLE(STEPPER_TIMx_update_event);

}



static useret b32
STEPPER_push_angular_velocities(StepperAngularVelocities* angular_velocities)
{

    b32 pushed =
        RingBuffer_push
        (
            &_STEPPER_driver.queued_angular_velocities,
            angular_velocities
        );

    return pushed;

}



////////////////////////////////////////////////////////////////////////////////



static useret u8
_STEPPER_calculate_crc(u8* data, u8 length)
{

    // @/`Bad TMC2209 CRC Implementation`:
    //
    // The way that the TMC2209 seems to be... wrong.
    // The CRC checksum should have it where, if the CRC
    // value is filled in at the end of the payload, the
    // resulting value will be zero. For some reason,
    // the implementation of TMC2209 forces you to actually
    // do the comparison of the computed CRC and expected CRC,
    // rather than whether or not it's actually non-zero.
    // (@/pg 20/sec 4.2/`TMC2209`).

    // Settings for @/url:`sunshine2k.de/coding/javascript/crc/crc_js.html`.
    //     - CRC8 polynomial: 0x7.
    //     - Input reflected.
    //     - Table reflected.

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



static useret enum StepperUpdateOnceResult : u32
{
    StepperUpdateOnceResult_again,
    StepperUpdateOnceResult_yield,
    StepperUpdateOnceResult_no_response_from_unit,
    StepperUpdateOnceResult_bad_response_from_unit,
    StepperUpdateOnceResult_bug = BUG_CODE,
}
_STEPPER_update_driver_once(void)
{

    enum StepperDriverState observed_state =
        atomic_load_explicit
        (
            &_STEPPER_driver.atomic_state,
            memory_order_relaxed // No synchronization needed.
        );

    switch (_STEPPER_driver.uart.state)
    {


        ////////////////////////////////////////
        //
        // The next UART transfer can be scheduled.
        //
        ////////////////////////////////////////

        case StepperUARTState_standby: switch (observed_state)
        {



            // We need to get the TMC2209's write sequence numbers. This has to be the
            // first thing we do before the configuration of the TMC2209s so that we can
            // know whether or not the first write transfer was successful.

            case StepperDriverState_initializing_uart_write_sequence_numbers:
            {

                _STEPPER_driver.uart =
                    (struct StepperUART)
                    {
                        .state            = StepperUARTState_read_scheduled,
                        .register_address = STEPPER_TMC2209_IFCNT_ADDRESS,
                    };

                return StepperUpdateOnceResult_again;

            } break;



            // We need to configure the TMC2209's registers.

            case StepperDriverState_doing_initialization_sequences:
            {

                const struct StepperInitializationSequenceEntry* entry = &STEPPER_INITIALIZATION_SEQUENCE[_STEPPER_driver.initialization_sequence_index];

                _STEPPER_driver.uart =
                    (struct StepperUART)
                    {
                        .state            = StepperUARTState_write_scheduled,
                        .register_address = entry->register_address,
                        .data             = entry->data,
                    };

                return StepperUpdateOnceResult_again;

            } break;



            // @/`Stepper Enable Delay`:
            //
            // It seems like when the TMC2209 is first initialized (especially after a power-cycle)
            // the motor will induce a large current draw after it is enabled. It quickly settles
            // down, but it's probably not nice to do to the batteries. It seems like by delaying
            // the enabling of the motor that the current spike can be avoided. I can tell this works
            // by the fact that the power supply not going into current-limiting mode after the power-cycle.
            //
            // TODO This is not very scientific however, so we definitely
            //      at some point actually measure the current draw of
            //      the batteries over time.

            case StepperDriverState_delaying_enable:
            {

                u32 elapsed_us = _STEPPER_driver.current_timestamp_us - _STEPPER_driver.most_recent_uart_transfer_timestamp_us;

                if (elapsed_us <= STEPPER_ENABLE_DELAY_US)
                {
                    return StepperUpdateOnceResult_yield;
                }
                else
                {

                    GPIO_ACTIVE(STEPPER_MOTOR_ENABLE);

                    atomic_store_explicit
                    (
                        &_STEPPER_driver.atomic_state,
                        StepperDriverState_waiting_on_angular_velocities,
                        memory_order_relaxed // No synchronization needed.
                    );

                    return StepperUpdateOnceResult_again;

                }

            } break;



            // See if we can update the TMC2209s with new angular velocities.

            case StepperDriverState_waiting_on_angular_velocities:
            {

                u32 elapsed_us = _STEPPER_driver.current_timestamp_us - _STEPPER_driver.most_recent_angular_velocity_update_timestamp_us;

                if (elapsed_us < STEPPER_VELOCITY_UPDATE_US)
                {
                    return StepperUpdateOnceResult_yield; // Let the current angular velocity run for a bit...
                }
                else if (RingBuffer_reading_pointer(&_STEPPER_driver.queued_angular_velocities))
                {

                    _STEPPER_driver.most_recent_angular_velocity_update_timestamp_us = _STEPPER_driver.current_timestamp_us;

                    atomic_store_explicit
                    (
                        &_STEPPER_driver.atomic_state,
                        StepperDriverState_updating_angular_velocities,
                        memory_order_relaxed // No synchronization needed.
                    );

                    return StepperUpdateOnceResult_again;

                }
                else // Under-run condition.
                {
                    return StepperUpdateOnceResult_yield;
                }

            } break;



            // We can update a TMC2209 with a new angular velocity.

            case StepperDriverState_updating_angular_velocities:
            {

                StepperAngularVelocities* angular_velocities = RingBuffer_reading_pointer(&_STEPPER_driver.queued_angular_velocities);

                if (!angular_velocities)
                    bug; // There should've been something queued...

                f32 new_angular_velocity = (*angular_velocities)[_STEPPER_driver.current_unit];
                i32 vactual              = (i32) (new_angular_velocity / (2.0f * PI) * STEPPER_STEPS_PER_REVOLUTION); // @/`Stepper Microstepping and Step Velocity`.

                _STEPPER_driver.uart =
                    (struct StepperUART)
                    {
                        .state            = StepperUARTState_write_scheduled,
                        .register_address = STEPPER_TMC2209_VACTUAL_ADDRESS,
                        .data             = (u32) vactual,
                    };

                return StepperUpdateOnceResult_again;

            } break;



            case StepperDriverState_no_response_from_unit  : return StepperUpdateOnceResult_yield;
            case StepperDriverState_bad_response_from_unit : return StepperUpdateOnceResult_yield;
            case StepperDriverState_disabled               : bug; // The user needs to initialize the stepper driver...
            default                                        : bug;

        } break;



        ////////////////////////////////////////
        //
        // The UART transfer was completed successfully.
        //
        ////////////////////////////////////////

        case StepperUARTState_done:
        {

            _STEPPER_driver.uart.state = StepperUARTState_standby; // Acknowledge the completion of the transfer.

            switch (observed_state)
            {



                // We got the TMC2209's write sequence number; we now move onto the next unit,
                // or if we've gotten all of the units' sequence numbers, we can now finally
                // begin configuring them.

                case StepperDriverState_initializing_uart_write_sequence_numbers:
                {

                    _STEPPER_driver.uart_write_sequence_numbers[_STEPPER_driver.current_unit] = (u8) (_STEPPER_driver.uart.data + 1);

                    _STEPPER_driver.current_unit += 1;
                    _STEPPER_driver.current_unit %= StepperUnit_COUNT;

                    if (_STEPPER_driver.current_unit == (enum StepperUnit) {0})
                    {
                        atomic_store_explicit
                        (
                            &_STEPPER_driver.atomic_state,
                            StepperDriverState_doing_initialization_sequences,
                            memory_order_relaxed // No synchronization needed.
                        );
                    }

                } break;



                // We did a register write to one of the TMC2209 units;
                // we move onto the next register to write, and then the next TMC2209 to configure.

                case StepperDriverState_doing_initialization_sequences:
                {

                    _STEPPER_driver.initialization_sequence_index += 1;
                    _STEPPER_driver.initialization_sequence_index %= countof(STEPPER_INITIALIZATION_SEQUENCE);

                    if (_STEPPER_driver.initialization_sequence_index == 0)
                    {

                        _STEPPER_driver.current_unit += 1;
                        _STEPPER_driver.current_unit %= StepperUnit_COUNT;

                        if (_STEPPER_driver.current_unit == (enum StepperUnit) {0})
                        {
                            atomic_store_explicit
                            (
                                &_STEPPER_driver.atomic_state,
                                StepperDriverState_delaying_enable,
                                memory_order_relaxed // No synchronization needed.
                            );
                        }

                    }

                } break;



                // Move onto the next TMC2209 unit to update, if any.

                case StepperDriverState_updating_angular_velocities:
                {

                    _STEPPER_driver.current_unit += 1;
                    _STEPPER_driver.current_unit %= StepperUnit_COUNT;

                    if (_STEPPER_driver.current_unit == (enum StepperUnit) {0})
                    {

                        if (!RingBuffer_pop(&_STEPPER_driver.queued_angular_velocities, nullptr))
                            bug; // There should've been angular velocities we were using to update with...

                        atomic_store_explicit
                        (
                            &_STEPPER_driver.atomic_state,
                            StepperDriverState_waiting_on_angular_velocities,
                            memory_order_relaxed // No synchronization needed.
                        );

                    }

                } break;



                case StepperDriverState_no_response_from_unit         : return StepperUpdateOnceResult_yield;
                case StepperDriverState_bad_response_from_unit        : return StepperUpdateOnceResult_yield;
                case StepperDriverState_delaying_enable               : bug; // There shouldn't have been any UART transfer...
                case StepperDriverState_waiting_on_angular_velocities : bug; // "
                case StepperDriverState_disabled                      : bug; // The user needs to initialize the stepper driver...
                default                                               : bug;

            }

            return StepperUpdateOnceResult_again;

        } break;



        ////////////////////////////////////////
        //
        // We send a read request packet to the TMC2209.
        //
        ////////////////////////////////////////

        case StepperUARTState_read_scheduled:
        case StepperUARTState_write_verification_read_scheduled:
        {

            u32 elapsed_us       = _STEPPER_driver.current_timestamp_us - _STEPPER_driver.most_recent_uart_transfer_timestamp_us;
            b32 too_soon_to_send = elapsed_us <= STEPPER_UART_TIME_MARGIN_US;

            if (too_soon_to_send)
            {
                return StepperUpdateOnceResult_yield; // @/`Stepper UART Time Margin Window`.
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
                        .sync             = 0b0000'0101, // @/pg 19/sec 4.1.2/`TMC2209`.
                        .node_address     = STEPPER_INSTANCE_TABLE[_STEPPER_driver.current_unit].address,
                        .register_address =
                            _STEPPER_driver.uart.state == StepperUARTState_write_verification_read_scheduled
                                ? STEPPER_TMC2209_IFCNT_ADDRESS
                                : _STEPPER_driver.uart.register_address,
                    };

                request.crc =
                    _STEPPER_calculate_crc
                    (
                        (u8*) &request,
                        sizeof(request) - sizeof(request.crc)
                    );



                // Send the request.

                UXART_tx_bytes(STEPPER_UXART_HANDLE, (u8*) &request, sizeof(request));

                _STEPPER_driver.most_recent_uart_transfer_timestamp_us = _STEPPER_driver.current_timestamp_us;



                // We now wait for the response.

                _STEPPER_driver.uart.state =
                    _STEPPER_driver.uart.state == StepperUARTState_write_verification_read_scheduled
                        ? StepperUARTState_write_verification_read_requested
                        : StepperUARTState_read_requested;

                return StepperUpdateOnceResult_yield;

            }

        } break;



        ////////////////////////////////////////
        //
        // See if the TMC2209 replied back to the read request.
        //
        ////////////////////////////////////////

        case StepperUARTState_read_requested:
        case StepperUARTState_write_verification_read_requested:
        {

            u32 elapsed_us       = _STEPPER_driver.current_timestamp_us - _STEPPER_driver.most_recent_uart_transfer_timestamp_us;
            b32 too_soon_to_read = elapsed_us <= STEPPER_UART_TIME_MARGIN_US;

            if (too_soon_to_read)
            {
                return StepperUpdateOnceResult_yield; // @/`Stepper UART Time Margin Window`.
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

                struct StepperReadResponse response       = {0};
                i32                        bytes_received = 0;

                while (true)
                {

                    u8  byte     = {0};
                    b32 got_byte = UXART_rx(STEPPER_UXART_HANDLE, &byte);

                    if (got_byte)
                    {

                        if (bytes_received < sizeof(response))
                        {
                            ((u8*) &response)[bytes_received] = byte;
                        }

                        bytes_received += 1;

                    }
                    else
                    {
                        break;
                    }

                }

                u8 digest =
                    _STEPPER_calculate_crc
                    (
                        (u8*) &response,
                        sizeof(response) - sizeof(response.crc)
                    );

                u8 expected_register_address =
                    _STEPPER_driver.uart.state == StepperUARTState_write_verification_read_requested
                        ? STEPPER_TMC2209_IFCNT_ADDRESS
                        : _STEPPER_driver.uart.register_address;

                u32 data = __builtin_bswap32(response.data);



                // No response?

                if (!bytes_received)
                {
                    return StepperUpdateOnceResult_no_response_from_unit;
                }



                // Response of wrong length?

                else if (bytes_received != sizeof(response))
                {
                    return StepperUpdateOnceResult_bad_response_from_unit;
                }



                // Corrupted response?

                else if (digest != response.crc) // @/`Bad TMC2209 CRC Implementation`.
                {
                    return StepperUpdateOnceResult_bad_response_from_unit;
                }



                // Bad synchronization?

                else if (response.sync != 0b0000'0101) // @/pg 19/sec 4.1.2/`TMC2209`.
                {
                    return StepperUpdateOnceResult_bad_response_from_unit;
                }



                // Unexpected master address?

                else if (response.master_address != 0xFF) // @/pg 19/sec 4.1.2/`TMC2209`.
                {
                    return StepperUpdateOnceResult_bad_response_from_unit;
                }



                // Echoed wrong register address?

                else if (response.register_address != expected_register_address)
                {
                    return StepperUpdateOnceResult_bad_response_from_unit;
                }



                // Verify that the write transfer was acknowledged by the TMC2209.

                else if (_STEPPER_driver.uart.state == StepperUARTState_write_verification_read_requested)
                {
                    if (_STEPPER_driver.uart_write_sequence_numbers[_STEPPER_driver.current_unit] == data)
                    {
                        _STEPPER_driver.uart_write_sequence_numbers[_STEPPER_driver.current_unit] = (u8) (data + 1);
                        _STEPPER_driver.uart.state = StepperUARTState_done;
                        return StepperUpdateOnceResult_again;
                    }
                    else
                    {
                        return StepperUpdateOnceResult_bad_response_from_unit;
                    }
                }



                // We've successfully did a read transfer!

                else
                {
                    _STEPPER_driver.uart.data  = data;
                    _STEPPER_driver.uart.state = StepperUARTState_done;
                    return StepperUpdateOnceResult_again;
                }

            }

        } break;



        ////////////////////////////////////////
        //
        // We send a write request packet to the TMC2209.
        //
        ////////////////////////////////////////

        case StepperUARTState_write_scheduled:
        {

            u32 elapsed_us       = _STEPPER_driver.current_timestamp_us - _STEPPER_driver.most_recent_uart_transfer_timestamp_us;
            b32 too_soon_to_send = elapsed_us <= STEPPER_UART_TIME_MARGIN_US;

            if (too_soon_to_send)
            {
                return StepperUpdateOnceResult_yield; // @/`Stepper UART Time Margin Window`.
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

                struct StepperWriteRequest request = // @/pg 18/sec 4.1.1/`TMC2209`.
                    {
                        .sync             = 0b0000'0101,
                        .node_address     = STEPPER_INSTANCE_TABLE[_STEPPER_driver.current_unit].address,
                        .register_address = _STEPPER_driver.uart.register_address | (1 << 7),
                        .data             = __builtin_bswap32(_STEPPER_driver.uart.data),
                    };

                request.crc =
                    _STEPPER_calculate_crc
                    (
                        (u8*) &request,
                        sizeof(request) - sizeof(request.crc)
                    );



                // Send the request.

                UXART_tx_bytes(STEPPER_UXART_HANDLE, (u8*) &request, sizeof(request));

                _STEPPER_driver.most_recent_uart_transfer_timestamp_us = _STEPPER_driver.current_timestamp_us;



                // After the write transfer, we read the
                // interface transmission counter register
                // to ensure the write request went through.

                _STEPPER_driver.uart.state = StepperUARTState_write_verification_read_scheduled;

                return StepperUpdateOnceResult_yield;

            }

        } break;



        default: bug;

    }

}



INTERRUPT_STEPPER_TIMx_update_event(void)
{
    if (CMSIS_GET(STEPPER_TIMx, SR, UIF)) // Some unit amount of time passed?
    {

        CMSIS_SET(STEPPER_TIMx, SR, UIF, false); // Acknowledge the timer's interrupt event.

        _STEPPER_driver.current_timestamp_us += STEPPER_UPDATE_EVENT_PERIOD_US;

        for (b32 yield = false; !yield;)
        {

            enum StepperUpdateOnceResult result = _STEPPER_update_driver_once();

            switch (result)
            {

                case StepperUpdateOnceResult_again:
                {
                    // Continue updating the state-machine.
                } break;

                case StepperUpdateOnceResult_yield:
                {
                    yield = true; // Nothing more to do until the next interrupt event.
                } break;



                // Driver stuck in error condition until user reinitializes everything.

                {

                    case StepperUpdateOnceResult_no_response_from_unit:
                    {
                        atomic_store_explicit
                        (
                            &_STEPPER_driver.atomic_state,
                            StepperDriverState_no_response_from_unit,
                            memory_order_relaxed // No synchronization needed.
                        );
                    } goto ERROR;

                    case StepperUpdateOnceResult_bad_response_from_unit:
                    {
                        atomic_store_explicit
                        (
                            &_STEPPER_driver.atomic_state,
                            StepperDriverState_bad_response_from_unit,
                            memory_order_relaxed // No synchronization needed.
                        );
                    } goto ERROR;

                    ERROR:;

                    GPIO_INACTIVE(STEPPER_MOTOR_ENABLE);
                    NVIC_DISABLE(STEPPER_TIMx_update_event);

                    yield = true;

                } break;



                // Something bad really happened; completely disable everything.

                case StepperUpdateOnceResult_bug:
                default:
                {

                    GPIO_INACTIVE(STEPPER_MOTOR_ENABLE);

                    NVIC_DISABLE(STEPPER_TIMx_update_event);

                    CMSIS_PUT(STEPPER_TIMx_RESET, true);

                    memzero(&_STEPPER_driver);

                    UXART_reinit(STEPPER_UXART_HANDLE);

                } break;

            }

        }

    }
}



////////////////////////////////////////////////////////////////////////////////



// @/`Stepper Ring-Buffer Length`:
//
// The length of the ring-buffer should be set to a reasonable
// size such that velocities can be queued up but without implying
// large latency.
//
// For example, if the window length is 256 and the update period
// is 25ms, then if the ring-buffer were to be completely filled,
// then it'd take 6.4 seconds for the entire ring-buffer to be
// processed; this is an impractical size.
//
// However, if the ring-buffer is too small, then there's a risk
// of an underrun where the driver has no value to pop from the
// ring-buffer and doesn't know the velocity to send out to
// the TMC2209. This is unlikely given that the update period is
// on the order of milliseconds, and a lot can be done during that
// time, but it's something to consider.



// @/`Stepper UART Time Margin Window`:
//
// Because the TMC2209 uses half-duplex UART communication,
// and furthermore multiple TMC2209s can share the same line,
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
// UART communication can be resynchronized with the TMC2209(s)
// by leaving the data line idle for long enough.
// Thus in the event of a CRC error, for instance, we're going
// to have to idly wait on the UART line, so we might as well
// always do it to ensure the upmost reliability.
// @/pg 18/sec 4.1.1/`TMC2209`.



// @/`Stepper Microstepping and Step Velocity`:
//
// There are 200 full-steps per rotation.
// (@/pg 67/sec 14/`TMC2209`).
//
// Each full-step can be then divided down into microsteps to
// achieve smaller granularity. For instance, a microstep
// of 4 implies 200 * 4 = 800 microsteps for a full revolution.
//
// The VACTUAL register that controls the step velocity depends
// on the TMC2209's clock frequency; when using the TMC2209's
// internal oscillator, the VACTUAL values are actually scaled by 1/0.715.
// @/pg 67/sec 14/`TMC2209`.
//
// This means if it takes 1600 microsteps to make a full revolution,
// then 1600 / 0.715 = ~2238 should be written into `VACTUAL` to get
// that desired 1 revolution-per-second rotation.
