////////////////////////////////////////////////////////////////////////////////
// TODO make driver_disable be active-low instead.



#define STEPPER_ENABLE_DELAY_US    500'000 // @/`Stepper Enable Delay`.
#define STEPPER_RING_BUFFER_LENGTH      32 // @/`Stepper Ring-Buffer Length`.

static_assert(IS_POWER_OF_TWO(STEPPER_RING_BUFFER_LENGTH));

#define TMC2209_IFCNT_ADDRESS 0x02

#include "stepper_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'Stepper',
        cmsis_name  = 'TIM',
        common_name = 'TIMx',
        terms       = lambda type, handle, node_address: (
            ('STEPPER_NODE_ADDRESS', 'expression', node_address),
        ),
    )

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
            (15 << 8) // "SENDDELAY" : Amount of delay before the read response is sent back.
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



enum StepperDriverUARTTransferState : u32
{
    StepperDriverUARTTransferState_standby,
    StepperDriverUARTTransferState_read_scheduled,
    StepperDriverUARTTransferState_read_requested,
    StepperDriverUARTTransferState_write_scheduled,
    StepperDriverUARTTransferState_write_verification_read_scheduled,
    StepperDriverUARTTransferState_write_verification_read_requested,
    StepperDriverUARTTransferState_done,
};



enum StepperDriverState : u32
{
    StepperDriverState_setting_uart_write_sequence_number,
    StepperDriverState_doing_initialization_sequence,
    StepperDriverState_enable_delay,
    StepperDriverState_working,
};



struct StepperDriver
{

    volatile enum StepperDriverState state;
    i32                              initialization_sequence_index;
    i32                              enable_delay_counter;
    i32                              velocities[STEPPER_RING_BUFFER_LENGTH];
    volatile u32                     reader;
    volatile u32                     writer;

    struct StepperDriverUARTTransfer
    {
        enum StepperDriverUARTTransferState state;
        u8                                  register_address;
        u32                                 data;
    }  uart_transfer;
    u8 uart_write_sequence_number;

};



static struct StepperDriver _STEPPER_drivers[StepperHandle_COUNT] = {0};



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
STEPPER_push_velocity(enum StepperHandle handle, i32 velocity)
{

    _EXPAND_HANDLE

    b32 can_be_pushed =
        driver->state == StepperDriverState_working &&
        (driver->writer - driver->reader < countof(driver->velocities));

    if (can_be_pushed)
    {
        driver->velocities[driver->writer % countof(driver->velocities)] = velocity;
        driver->writer += 1;
    }

    return can_be_pushed;

}



static useret enum StepperUpdateResult : u32
{
    StepperUpdateResult_relinquished,
    StepperUpdateResult_busy,
}
_STEPPER_update(enum StepperHandle handle, enum UXARTHandle uxart_handle)
{

    _EXPAND_HANDLE

    while (true)
    {
        switch (driver->uart_transfer.state)
        {

            ////////////////////////////////////////////////////////////////////////////////
            //
            // The next UART transfer can be scheduled.
            //

            case StepperDriverUARTTransferState_standby: switch (driver->state)
            {



                // This has to be the first thing we do
                // for the configuration of the TMC2209
                // so that we can know whether or not the
                // first write request is successful.

                case StepperDriverState_setting_uart_write_sequence_number:
                {

                    GPIO_ACTIVE(driver_disable); // Ensure the motor can't draw current.

                    driver->uart_transfer =
                        (struct StepperDriverUARTTransfer)
                        {
                            .state            = StepperDriverUARTTransferState_read_scheduled,
                            .register_address = TMC2209_IFCNT_ADDRESS,
                        };

                } break;



                // We then do a series of register writes to configure the TMC2209.

                case StepperDriverState_doing_initialization_sequence:
                {

                    driver->uart_transfer =
                        (struct StepperDriverUARTTransfer)
                        {
                            .state            = StepperDriverUARTTransferState_write_scheduled,
                            .register_address = STEPPER_INITIALIZATION_SEQUENCE[driver->initialization_sequence_index].register_address,
                            .data             = STEPPER_INITIALIZATION_SEQUENCE[driver->initialization_sequence_index].data,
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

                case StepperDriverState_enable_delay:
                {

                    driver->enable_delay_counter += 1;

                    if (driver->enable_delay_counter < STEPPER_ENABLE_DELAY_US / 25'000) // TODO Coupled.
                    {
                        return StepperUpdateResult_relinquished;
                    }
                    else
                    {
                        GPIO_INACTIVE(driver_disable); // The motor can draw current now.
                        driver->state = StepperDriverState_working;
                    }

                } break;



                // The TMC2209 is all ready; let's update the step velocity now.

                case StepperDriverState_working:
                {

                    i32 velocity = {0};

                    if (driver->reader == driver->writer)
                    {
                        // Underflow condition.
                        // TODO Indicate this situation somehow?
                        // TODO Perhaps keep the same velocity instead.
                        velocity = 0;
                    }
                    else
                    {
                        i32 read_index  = driver->reader % countof(driver->velocities);
                        velocity        = driver->velocities[read_index];
                        driver->reader += 1;
                    }

                    driver->uart_transfer =
                        (struct StepperDriverUARTTransfer)
                        {
                            .state            = StepperDriverUARTTransferState_write_scheduled,
                            .register_address = 0x22, // TODO.
                            .data             = velocity,
                        };

                } break;



                default: panic;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We just finished a UART transfer.
            //

            case StepperDriverUARTTransferState_done:
            {

                switch (driver->state)
                {



                    // We now know what the first write
                    // request's sequence number will be.

                    case StepperDriverState_setting_uart_write_sequence_number:
                    {
                        driver->uart_write_sequence_number = driver->uart_transfer.data + 1;
                        driver->state                      = StepperDriverState_doing_initialization_sequence;
                    } break;



                    // See if we're done doing the series of
                    // register writes to configure the TMC2209.

                    case StepperDriverState_doing_initialization_sequence:
                    {

                        driver->initialization_sequence_index += 1;

                        if (driver->initialization_sequence_index < countof(STEPPER_INITIALIZATION_SEQUENCE))
                        {
                            // We still have more registers to write to.
                        }
                        else
                        {
                            driver->state = StepperDriverState_enable_delay;
                        }

                    } break;



                    // We're done updating the motor's step velocity.

                    case StepperDriverState_working:
                    {
                        // Don't care.
                    } break;



                    case StepperDriverState_enable_delay : panic;
                    default                              : panic;

                }

                driver->uart_transfer.state = StepperDriverUARTTransferState_standby;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We send a read request packet to the TMC2209.
            //

            case StepperDriverUARTTransferState_read_scheduled:
            case StepperDriverUARTTransferState_write_verification_read_scheduled:
            {

                if (driver->uart_transfer.register_address & (1 << 7))
                    panic;



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
                        .node_address     = STEPPER_NODE_ADDRESS,
                        .register_address =
                            driver->uart_transfer.state == StepperDriverUARTTransferState_write_verification_read_scheduled
                                ? TMC2209_IFCNT_ADDRESS
                                : driver->uart_transfer.register_address,
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
                    uxart_handle,
                    (u8*) &request,
                    sizeof(request)
                );



                // Flush the RX-FIFO.
                // TODO Don't use char.

                while (UXART_rx(uxart_handle, &(char) {0}));



                // We now wait for the response.

                driver->uart_transfer.state =
                    driver->uart_transfer.state == StepperDriverUARTTransferState_write_verification_read_scheduled
                        ? StepperDriverUARTTransferState_write_verification_read_requested
                        : StepperDriverUARTTransferState_read_requested;

                return StepperUpdateResult_busy;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // See if the TMC2209 replied back to the read request.
            //

            case StepperDriverUARTTransferState_read_requested:
            case StepperDriverUARTTransferState_write_verification_read_requested:
            {

                if (driver->uart_transfer.register_address & (1 << 7))
                    panic;



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
                    if (!UXART_rx(uxart_handle, &((char*) &response)[i])) // TODO Not use char.
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
                    driver->uart_transfer.state == StepperDriverUARTTransferState_write_verification_read_requested
                        ? TMC2209_IFCNT_ADDRESS
                        : driver->uart_transfer.register_address;

                if (response.register_address != expected_register_address)
                    sorry



                // Got the register data intact!

                u32 data = __builtin_bswap32(response.data);



                // Handle the data.

                if (driver->uart_transfer.state == StepperDriverUARTTransferState_write_verification_read_requested)
                {

                    if (driver->uart_write_sequence_number != data)
                        sorry

                    driver->uart_write_sequence_number = data + 1;

                }
                else
                {
                    driver->uart_transfer.data = data;
                }



                // We're now done reading the register
                // (or maybe verifying that the write request was successful).

                driver->uart_transfer.state = StepperDriverUARTTransferState_done;

            } break;



            ////////////////////////////////////////////////////////////////////////////////
            //
            // We send a write request packet to the TMC2209.
            //

            case StepperDriverUARTTransferState_write_scheduled:
            {

                if (driver->uart_transfer.register_address & (1 << 7))
                    panic;



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
                        .node_address     = STEPPER_NODE_ADDRESS,
                        .register_address = driver->uart_transfer.register_address | (1 << 7),
                        .data             = __builtin_bswap32(driver->uart_transfer.data),
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
                    uxart_handle,
                    (u8*) &request,
                    sizeof(request)
                );



                // After the write transfer, we read the
                // interface transmission counter register
                // to ensure the write request went through.

                driver->uart_transfer.state = StepperDriverUARTTransferState_write_verification_read_scheduled;

                return StepperUpdateResult_busy;

            } break;



            default: panic;

        }
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
