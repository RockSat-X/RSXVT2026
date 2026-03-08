#define STEPPER_ENABLE_DELAY_US     500'000
#define STEPPER_VELOCITY_UPDATE_US  25'000 // @/`Sequence Angular Accelerations Delta Time`.
#define STEPPER_UART_TIME_MARGIN_US 2'000
#define STEPPER_RING_BUFFER_LENGTH  8       // TODO Determine latency.
#define AUTOMATIC_SHUTDOWN_TIME_US  0       // TODO Once finalized, we should use (10 * 60'000'000).
#define MAX_ANGULAR_ACCELERATION    (200.0f)
#define MAX_ANGULAR_VELOCITY        (2000.0f * 2.0f * PI / 60.0f)
#define DEMONSTRATE_STEPPER         true
#define CONTROLLER_ENABLE           true

#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"
#include "gnc.c"



////////////////////////////////////////////////////////////////////////////////



enum DiagnosticMask : u32 // Lower the bit-index, higher the priority.
{
    DiagnosticMask_stepper_driver_issue       = 1 << 0,
    DiagnosticMask_angular_velocity_saturated = 1 << 1,
    DiagnosticMask_wiping_filesystem          = 1 << 2,
    DiagnosticMask_vn100_mishap               = 1 << 3,
    DiagnosticMask_logging_mishap             = 1 << 4,
    DiagnosticMask_vn100_heartbeat            = 1 << 5,
    DiagnosticMask_logging_heartbeat          = 1 << 6,
};

static struct
{
    RingBuffer(struct VN100Packet, 4) vn100_packets;
    volatile struct StepperTuple      current_angular_accelerations;
    volatile struct StepperTuple      current_angular_velocities;
} CONTROLLER;

static struct
{
    RingBuffer(struct VN100Packet, 4) vn100_packets;
} LOGGER = {0};



////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // More peripheral initializations that depend on the above initializations.

    BUZZER_partial_init();



    // When the vehicle becomes powered on, it's typically because
    // of the external power supply through the vehicle interface.
    // To make sure that the vehicle should stay powered on with
    // the battery supply, we play a tune that'll act as the delay.

    BUZZER_play(BuzzerTune_waking_up);

    while (BUZZER_current_tune());

    GPIO_ACTIVE(battery_allowed);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

#if CONTROLLER_ENABLE

    STEPPER_reinit();



    // For diagnostic purposes, we immediately set angular velocities to
    // something non-zero so we can easily tell if something is wrong.

    #if DEMONSTRATE_STEPPER
    {
        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
        {
            CONTROLLER.current_angular_velocities.values[unit] = 1.0f;
        }
    }
    #endif



    for (;;)
    {



        // For demonstration and diagnostics purposes,
        // we can directly control the stepper motors.

        #if DEMONSTRATE_STEPPER
        {

            // Interpret user's input, if any.

            static b32 replay_sequence_number = false;

            u8 input = {0};

            while (stlink_rx(&input))
            {

                switch (input)
                {

                    case 'j':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] -= 0.1f;
                        }
                    } break;

                    case 'J':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] -= 1.0f;
                        }
                    } break;

                    case 'k':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] += 0.1f;
                        }
                    } break;

                    case 'K':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] += 1.0f;
                        }
                    } break;

                    case '0':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] = 0.0f;
                            CONTROLLER.current_angular_velocities   .values[unit] = 0.0f;
                        }
                    } break;

                    case '<':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] -= 200.0f;
                        }
                    } break;

                    case '>':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] += 200.0f;
                        }
                    } break;

                    case '-':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit]  =  0.0f;
                            CONTROLLER.current_angular_velocities   .values[unit] *= -1.0f;
                        }
                    } break;

                    case 'b':
                    {
                        GPIO_TOGGLE(battery_allowed);
                    } break;

                    case 'x':
                    {

                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            CONTROLLER.current_angular_accelerations.values[unit] = 0.0f;
                            CONTROLLER.current_angular_velocities   .values[unit] = 0.0f;
                        }

                        replay_sequence_number = 1; break;

                    } break;

                    default:
                    {
                        // Don't care.
                    } break;

                }

            }



            // If requested, we play back a sequence of angular accelerations for simulation purposes.

            if (replay_sequence_number)
            {

                #include "SEQUENCE_ANGULAR_ACCELERATIONS.meta"
                /* #meta

                    import math

                    def impulse(t, slope):

                        try:
                            u = math.e**(4 * slope * t)
                            return (16 * u * (-1 + u) * slope**2) / (1 + u)**3
                        except OverflowError:
                            return 0

                    IMPULSES = {
                        'axis_x' : (
                            (1 + 0 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 1 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 2 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 3 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (2 + 1       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 2       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 3       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 4       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 8       , -1.00 * 2 * math.pi, 10),
                            (2 + 9       ,  1.00 * 2 * math.pi, 10),
                            (2 + 10      ,  1.00 * 2 * math.pi, 10),
                            (2 + 11      ,  1.00 * 2 * math.pi, 10),
                            (2 + 12      ,  1.00 * 2 * math.pi, 10),
                        ),
                        'axis_y' : (
                            (1 + 0 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 1 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 2 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 3 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (2 + 2       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 3       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 4       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 5       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 8       , -1.00 * 2 * math.pi, 10),
                            (2 + 9       ,  1.00 * 2 * math.pi, 2 ),
                            (2 + 12      , -1.00 * 2 * math.pi, 2 ),
                        ),
                        'axis_z' : (
                            (1 + 0 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 1 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 2 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (1 + 3 * 0.25,  0.25 * 2 * math.pi, 5 ),
                            (2 + 3       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 4       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 5       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 6       ,  0.25 * 2 * math.pi, 5 ),
                            (2 + 8       , -1.00 * 2 * math.pi, 10),
                            (2 + 9       ,  2.00 * 2 * math.pi, 2 ),
                            (2 + 12      , -2.00 * 2 * math.pi, 2 ),
                        ),
                    }

                    with Meta.enter('static const struct StepperTuple SEQUENCE_ANGULAR_ACCELERATIONS[] ='):

                        DURATION = 16
                        dt       = 0.025 # @/`Sequence Angular Accelerations Delta Time`: Coupled.

                        for i in range(0, round(DURATION / dt)):

                            t = i * dt

                            Meta.line(f'''
                                {{ {{ {', '.join(

                                    f'[StepperUnit_{unit}] = {sum(impulse(t - center, slope) * radians for center, radians, slope in impulses) :12f}f'
                                    for unit, impulses in IMPULSES.items()

                                )} }} }}, // t = {t :f} s.
                            ''')

                */

                CONTROLLER.current_angular_accelerations  = SEQUENCE_ANGULAR_ACCELERATIONS[replay_sequence_number - 1];
                replay_sequence_number                   += 1;
                replay_sequence_number                   %= countof(SEQUENCE_ANGULAR_ACCELERATIONS) + 1;

                if (!replay_sequence_number)
                {
                    for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                    {
                        CONTROLLER.current_angular_accelerations.values[unit] = 0.0f;
                        CONTROLLER.current_angular_velocities   .values[unit] = 0.0f;
                    }
                }

            }

        }
        #endif



        // Perform GNC calculations.

        {
            // TODO.
        }



        // Update each stepper unit's angular velocities.

        b32 max_angular_velocity_reached = false;

        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
        {



            // Limit the acceleration.

            if (CONTROLLER.current_angular_accelerations.values[unit] >= MAX_ANGULAR_ACCELERATION)
            {
                CONTROLLER.current_angular_accelerations.values[unit] = MAX_ANGULAR_ACCELERATION;
            }
            else if (CONTROLLER.current_angular_accelerations.values[unit] <= -MAX_ANGULAR_ACCELERATION)
            {
                CONTROLLER.current_angular_accelerations.values[unit] = -MAX_ANGULAR_ACCELERATION;
            }



            // Find new angular velocity.

            CONTROLLER.current_angular_velocities.values[unit] +=
                CONTROLLER.current_angular_accelerations.values[unit]
                * (f32) STEPPER_VELOCITY_UPDATE_US / 1'000'000.0f;



            // Limit the angular velocity.

            if (CONTROLLER.current_angular_velocities.values[unit] >= MAX_ANGULAR_VELOCITY)
            {
                CONTROLLER.current_angular_velocities.values[unit] = MAX_ANGULAR_VELOCITY;
                max_angular_velocity_reached = true;
            }
            else if (CONTROLLER.current_angular_velocities.values[unit] <= -MAX_ANGULAR_VELOCITY)
            {
                CONTROLLER.current_angular_velocities.values[unit] = -MAX_ANGULAR_VELOCITY;
                max_angular_velocity_reached = true;
            }

        }



        // Indicate if a stepper motor has reached saturation;
        // shouldn't really happen in practice.

        if (max_angular_velocity_reached)
        {
            xTaskNotify
            (
                diagnostics_handle,
                DiagnosticMask_angular_velocity_saturated,
                eSetBits
            );
        }



        // Queue up the new angular velocity.

        for (b32 yield = false; !yield;)
        {

            enum StepperPushAngularVelocitiesResult result =
                STEPPER_push_angular_velocities
                (
                    (struct StepperTuple*) &CONTROLLER.current_angular_velocities
                );

            switch (result)
            {

                case StepperPushAngularVelocitiesResult_pushed:
                {
                    yield = true;
                } break;

                case StepperPushAngularVelocitiesResult_full:
                case StepperPushAngularVelocitiesResult_still_initializing:
                {
                    FREERTOS_delay_ms(1);
                } break;

                case StepperPushAngularVelocitiesResult_no_response_from_unit:  // TODO Collect statistics?
                case StepperPushAngularVelocitiesResult_bad_response_from_unit:
                case StepperPushAngularVelocitiesResult_bug:
                default:
                {

                    xTaskNotify
                    (
                        diagnostics_handle,
                        DiagnosticMask_stepper_driver_issue,
                        eSetBits
                    );

                    memzero((struct StepperTuple*) &CONTROLLER.current_angular_accelerations);
                    memzero((struct StepperTuple*) &CONTROLLER.current_angular_velocities   );

                    STEPPER_reinit();

                    yield = true;

                } break;

            }

        }

    }

#else

    for (;;)
    {
        FREERTOS_delay_ms(1'000);
    }

#endif

}



////////////////////////////////////////////////////////////////////////////////



static useret u16 // If checksum is 0x0A, then the hexadecimal value is 0x3041 ('0' = 0x30, 'A' = 0x41).
vn100_make_hexadecimal_checksum(u8* bytes, i32 length) // Starting character ($) and checksum field (*xx) itself should not be included.
{

    u8 checksum = 0;

    for (i32 i = 0; i < length; i += 1)
    {
        checksum ^= bytes[i];
    }

    u16 result = 0;

    result |=
        ((checksum >> 4) & 0x0F) < 10
            ? (u16) ('0' + (((checksum >> 4) & 0x0F) -  0)) << 8
            : (u16) ('A' + (((checksum >> 4) & 0x0F) - 10)) << 8;

    result |=
        ((checksum >> 0) & 0x0F) < 10
            ? (u16) ('0' + (((checksum >> 0) & 0x0F) -  0)) << 0
            : (u16) ('A' + (((checksum >> 0) & 0x0F) - 10)) << 0;

    return result;

}

static enum VN100AwaitResponseResult : u32
{
    VN100AwaitResponseResult_successful,
    VN100AwaitResponseResult_timeout,
    VN100AwaitResponseResult_checksum_mismatch,
}
vn100_await_response(u8* dst_response_buffer, i32 dst_response_capacity, i32* dst_response_length)
{

    if (!dst_response_buffer)
        sus;

    if (dst_response_capacity <= -1)
        sus;

    if (!dst_response_length)
        sus;



    // Get the response.

    *dst_response_length = 0;

    i32 checksum_indicator_index = 0;
    i32 stalls                   = 0;

    while (true)
    {

        u8 received_byte = {0};

        while (true)
        {
            if (UXART_rx(UXARTHandle_vn100, &received_byte))
            {
                break; // Got data!
            }
            else if (stalls < 1'000)
            {
                FREERTOS_delay_ms(1); // Do something else for a bit.
                stalls += 1;
            }
            else // We waited too long.
            {
                return VN100AwaitResponseResult_timeout;
            }
        }

        b32 append_byte = false;

        if (received_byte == '$')
        {
            *dst_response_length     = 0;    // Discard whatever we got so far and start over.
            checksum_indicator_index = 0;    // "
            append_byte              = true; // The starting token shall be the first character.
        }
        else
        {
            append_byte = !!*dst_response_length; // Append if we've found the starting token.
        }

        if (append_byte)
        {
            if (*dst_response_length >= dst_response_capacity)
            {
                *dst_response_length     = 0; // Buffer over-run; invalidate the data we got so far.
                checksum_indicator_index = 0; // "
            }
            else
            {

                dst_response_buffer[*dst_response_length]  = received_byte;
                *dst_response_length                      += 1;

                if (checksum_indicator_index && *dst_response_length == checksum_indicator_index + 3)
                {
                    break; // We reached the end of the VN-100 register read payload.
                }
                else if (received_byte == '*')
                {
                    checksum_indicator_index = *dst_response_length - 1; // We're near the end of the response.
                }

            }
        }

    }



    // Verify integrity of the response.

    u16 expected_hexadecimal_checksum =
        vn100_make_hexadecimal_checksum
        (
            dst_response_buffer      + 1,
            checksum_indicator_index - 1
        );

    u16 received_hexadecimal_checksum =
        (dst_response_buffer[checksum_indicator_index + 1] << 8) |
        (dst_response_buffer[checksum_indicator_index + 2] << 0);

    if (expected_hexadecimal_checksum == received_hexadecimal_checksum)
    {
        return VN100AwaitResponseResult_successful;
    }
    else
    {
        return VN100AwaitResponseResult_checksum_mismatch;
    }

}

FREERTOS_TASK(vn100, 8192, 0)
{

    UXART_reinit(UXARTHandle_vn100);

    i32 valid_packet_count = 0;

    for (;;)
    {

        u8  payload_buffer[256] = {0};
        i32 payload_length      = {0};

        enum VN100AwaitResponseResult response_result =
            vn100_await_response
            (
                payload_buffer,
                countof(payload_buffer),
                &payload_length
            );

        switch (response_result)
        {



            // Parse the register fields.

            case VN100AwaitResponseResult_successful:
            {

                struct VN100Packet packet               = {0};
                b32                valid_fields         = true;
                i32                field_position_index = 0;
                i32                field_start_index    = 0;
                i32                field_length         = 0;

                while (true)
                {

                    b32 found_comma    = payload_buffer[field_start_index + field_length] == ',';
                    b32 end_of_payload = field_start_index + field_length >= payload_length - 3;

                    if (!found_comma && !end_of_payload) // Haven't found end of field yet?
                    {
                        field_length += 1;
                    }
                    else // Ready to parse the field now.
                    {



                        // The very first field should be the expected magic starting token and register name.

                        if (field_position_index == 0)
                        {
                            valid_fields =
                                !strncmp
                                (
                                    (char*) (payload_buffer + field_start_index),
                                    "$VNQMR",
                                    (u32) field_length
                                );
                        }



                        // The remaining fields are the values of the register.

                        else if (field_position_index - 1 < sizeof(struct VN100Packet) / sizeof(f32))
                        {

                            // Parse the field as a float, if possible.

                            f32 parsed_value   = 0.0f;
                            b32 negative       = false;
                            f32 decimal_factor = {0};

                            for
                            (
                                i32 index = 0;
                                index < field_length && valid_fields;
                                index += 1
                            )
                            {
                                if (payload_buffer[field_start_index + index] == '+' && index == 0)
                                {
                                    negative = false;
                                }
                                else if (payload_buffer[field_start_index + index] == '-' && index == 0)
                                {
                                    negative = true;
                                }
                                else if (payload_buffer[field_start_index + index] == '.' && !decimal_factor)
                                {
                                    decimal_factor = 0.1f;
                                }
                                else if ('0' <= payload_buffer[field_start_index + index] && payload_buffer[field_start_index + index] <= '9')
                                {
                                    f32 digit = (f32) (payload_buffer[field_start_index + index] - '0');

                                    if (decimal_factor)
                                    {
                                        parsed_value   += decimal_factor * digit;
                                        decimal_factor *= 0.1f;
                                    }
                                    else
                                    {
                                        parsed_value = parsed_value * 10.0f + digit;
                                    }
                                }
                                else
                                {
                                    valid_fields = false; // Not a digit...?
                                }
                            }

                            if (valid_fields)
                            {

                                if (negative)
                                {
                                    parsed_value = -parsed_value;
                                }

                                ((f32*) &packet)[field_position_index - 1] = parsed_value;

                            }

                        }



                        // Extraneous fields...?

                        else
                        {
                            valid_fields = false;
                        }



                        if (!valid_fields)
                        {
                            break; // Abort parsing the received data.
                        }
                        else if (end_of_payload)
                        {
                            break; // No more data to process.
                        }
                        else // Move onto next field.
                        {
                            field_position_index += 1;
                            field_start_index    += field_length + 1; // +1 to skip the comma.
                            field_length          = 0;
                        }

                    }

                }

                if (valid_fields)
                {

                    if (!RingBuffer_push(&CONTROLLER.vn100_packets, &packet))
                    {
                        // VN-100 data overrun!
                    }

                    if (!RingBuffer_push(&LOGGER.vn100_packets, &packet))
                    {
                        // VN-100 data overrun, but it's for the logger; who cares.
                    }

                    valid_packet_count += 1;

                    if (valid_packet_count % 256 == 0)
                    {
                        xTaskNotify
                        (
                            diagnostics_handle,
                            DiagnosticMask_vn100_heartbeat,
                            eSetBits
                        );
                    }

                }

            } break;



            // Whoops...

            case VN100AwaitResponseResult_timeout:
            case VN100AwaitResponseResult_checksum_mismatch:
            {
                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_vn100_mishap,
                    eSetBits
                );
            } break;



            default: sus;

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(logger, 8192, 0)
{

    static struct Sector working_sectors[8]         = {0};
    b32                  completely_wipe_filesystem = false;

    for (;;)
    {

        REINITIALIZE:;



        // If need be, we can format the SD card. Data saved on the vehicle
        // SD card is not critical as it's only for diagnostic purposes for
        // things like sequence testing.

        if (completely_wipe_filesystem)
        {
            xTaskNotify
            (
                diagnostics_handle,
                DiagnosticMask_wiping_filesystem,
                eSetBits
            );
        }



        // Try setting up the file-system.

        enum FileSystemReinitResult reinit_result =
            FILESYSTEM_reinit
            (
                SDHandle_primary,
                working_sectors,
                completely_wipe_filesystem ? countof(working_sectors) : 0
            );

        completely_wipe_filesystem = false;

        switch (reinit_result)
        {



            case FileSystemReinitResult_success:
            {
                // Ready to go!
            } break;



            // Couldn't successfully communicate with the SD card,
            // likely because of poor connection or there's no card mounted.
            // Either way, we indicate this as a soft error.

            case FileSystemReinitResult_couldnt_ready_card:
            case FileSystemReinitResult_transfer_error:
            {

                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_logging_mishap,
                    eSetBits
                );

                FREERTOS_delay_ms(5'000);

                goto REINITIALIZE;

            } break;



            // Something went wrong trying to mount the file-system;
            // best thing we can do is format the card and hope it'll work out.

            case FileSystemReinitResult_invalid_filesystem:
            case FileSystemReinitResult_no_more_space_for_new_file:
            case FileSystemReinitResult_fatfs_internal_error:
            case FileSystemReinitResult_bug:
            default:
            {

                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_logging_mishap,
                    eSetBits
                );

                FREERTOS_delay_ms(5'000);

                completely_wipe_filesystem = true;

                goto REINITIALIZE;

            } break;

        }



        // Alright, file-system is ready. Let's start logging!

        u32 most_recent_heartbeat_timestamp_us  = 0;
        u32 most_recent_stlink_log_timestamp_us = 0;

        while (true)
        {

            // Make the log entry.

            u32                current_timestamp_us = TIMEKEEPING_microseconds();
            struct VN100Packet vn100_packet_data    = {0};
            b32                vn100_packet_exist   = RingBuffer_pop_to_latest(&LOGGER.vn100_packets, &vn100_packet_data);

            i32 log_entry_length =
                snprintf_
                (
                    (char*) working_sectors[0].bytes,
                    countof(working_sectors[0].bytes),
                    "[%u us]"                             "\n"
                    "Log file index : %d"                 "\n"
                    "Ang. accel.    : <%.3f, %.3f, %.3f>" "\n"
                    "Ang. velocity  : <%.3f, %.3f, %.3f>" "\n"
                    "RPM            : <%.3f, %.3f, %.3f>" "\n"
                    "Stepper issues : %d"                 "\n"
                    "OpenMV issues  : %d"                 "\n"
                    "ESP32 issues   : %d"                 "\n"
                    "Quaternion?    : <%f, %f, %f, %f>"   "\n" // TODO We should probably attach a timestamp to received VN-100 data.
                    "Magnetometer?  : <%f, %f, %f>"       "\n" // TODO "
                    "Accelerometer? : <%f, %f, %f>"       "\n" // TODO "
                    "Gyroscope?     : <%f, %f, %f>"       "\n" // TODO "
                    "Ext. power     : %s"                 "\n"
                    "\n",
                    current_timestamp_us,
                    0, // TODO.
                    CONTROLLER.current_angular_accelerations.values[StepperUnit_axis_x],
                    CONTROLLER.current_angular_accelerations.values[StepperUnit_axis_y],
                    CONTROLLER.current_angular_accelerations.values[StepperUnit_axis_z],
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_x],
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_y],
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_z],
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_x] / (2.0f * PI) * 60.0f,
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_y] / (2.0f * PI) * 60.0f,
                    CONTROLLER.current_angular_velocities.values[StepperUnit_axis_z] / (2.0f * PI) * 60.0f,
                    0, // TODO.
                    0, // TODO.
                    0, // TODO.
                    vn100_packet_exist ? vn100_packet_data.QuatX  : NAN,
                    vn100_packet_exist ? vn100_packet_data.QuatY  : NAN,
                    vn100_packet_exist ? vn100_packet_data.QuatZ  : NAN,
                    vn100_packet_exist ? vn100_packet_data.QuatS  : NAN,
                    vn100_packet_exist ? vn100_packet_data.MagX   : NAN,
                    vn100_packet_exist ? vn100_packet_data.MagY   : NAN,
                    vn100_packet_exist ? vn100_packet_data.MagZ   : NAN,
                    vn100_packet_exist ? vn100_packet_data.AccelX : NAN,
                    vn100_packet_exist ? vn100_packet_data.AccelY : NAN,
                    vn100_packet_exist ? vn100_packet_data.AccelZ : NAN,
                    vn100_packet_exist ? vn100_packet_data.GyroX  : NAN,
                    vn100_packet_exist ? vn100_packet_data.GyroY  : NAN,
                    vn100_packet_exist ? vn100_packet_data.GyroZ  : NAN,
                    false ? "Yes" : "No" // TODO.
                );



            // Formatting error, if for some reason.

            if (log_entry_length <= -1)
            {
                log_entry_length = 0;
            }



            // Log entry too long; just truncate.

            if (log_entry_length > sizeof(working_sectors[0].bytes))
            {
                log_entry_length = sizeof(working_sectors[0].bytes);
            }



            // The remaining space in the log buffer will be used for the divider.

            memset
            (
                working_sectors[0].bytes + log_entry_length,
                '.',
                (u32) (sizeof(working_sectors[0].bytes) - log_entry_length)
            );

            working_sectors[0].bytes[countof(working_sectors[0].bytes) - 2] = '\n'; // Don't care if this stamps over the string.
            working_sectors[0].bytes[countof(working_sectors[0].bytes) - 1] = '\n'; // "



            // Also send the log out through the ST-Link periodically for convenience.

            if (current_timestamp_us - most_recent_stlink_log_timestamp_us >= 250'000)
            {

                most_recent_stlink_log_timestamp_us = current_timestamp_us;

                stlink_tx
                (
                    "%.*s",
                    log_entry_length,
                    working_sectors[0].bytes
                );

            }



            // Save onto SD card for us to verify the vehicle
            // is operating as intended after sequence testing.

            enum FileSystemSaveResult save_result =
                FILESYSTEM_save
                (
                    SDHandle_primary,
                    &working_sectors[0],
                    1
                );

            switch (save_result)
            {



                // Successfully saved the log entry;
                // periodically indicate that we're logging data too.

                case FileSystemSaveResult_success:
                {
                    if (current_timestamp_us - most_recent_heartbeat_timestamp_us >= 5'000'000)
                    {

                        most_recent_heartbeat_timestamp_us = current_timestamp_us;

                        xTaskNotify
                        (
                            diagnostics_handle,
                            DiagnosticMask_logging_heartbeat,
                            eSetBits
                        );

                    }
                } break;



                // Small error occured; let's just reinitialize the file-system
                // as quickly as possible to be able to continue logging, if still possible.

                case FileSystemSaveResult_transfer_error:
                {

                    xTaskNotify
                    (
                        diagnostics_handle,
                        DiagnosticMask_logging_mishap,
                        eSetBits
                    );

                    goto REINITIALIZE;

                } break;



                // Uh oh, let's just wipe the SD card so we can continue logging new data.

                case FileSystemSaveResult_no_more_space_for_data:
                case FileSystemSaveResult_fatfs_internal_error:
                {
                    completely_wipe_filesystem = true;
                    goto REINITIALIZE;
                } break;



                // Something weird happened. Let's just reinitialize
                // the file-system and hope for the best...

                case FileSystemSaveResult_bug:
                default:
                {
                    goto REINITIALIZE;
                } break;

            }

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



static b32
diagnostics_delay_ms(u32* current_flags, u32 milliseconds)
{

    u32 new_flags = 0;

    BaseType_t got_notification =
        xTaskNotifyWait
        (
            0,
            (u32) -1, // Immediately acknowledge the new diagnostics.
            (uint32_t*) &new_flags,
            0
        );

    enum DiagnosticMask old_diagnostic = *current_flags & -*current_flags;

    if (got_notification)
    {
        *current_flags |= new_flags; // Add the new diagnostics to the list of existing ones to process.
    }

    enum DiagnosticMask new_diagnostic = *current_flags & -*current_flags;

    if (new_diagnostic == old_diagnostic)
    {

        // Although `xTaskNotifyWait` has a time-out argument that could be used
        // as a delay that can also be interrupted early when there's a notification,
        // it's possible that a task set the same diagnostic repeatedly over and
        // over again to which a delay never really happens. By using a regular
        // task delay here, we're making things slightly less responsive because
        // the delay won't allow for the `diagnostics` task to handle a new
        // diagnostic of higher priority, but since this is literally just for
        // diagnostics, it's okay.

        FREERTOS_delay_ms(milliseconds);

        return false;

    }
    else
    {
        return true; // There's a new diagnostic of higher priority that we should handle.
    }

}

FREERTOS_TASK(diagnostics, 512, 1)
{

    u32 current_flags = 0;

    for (;;)
    {

        diagnostics_delay_ms(&current_flags, 0); // To update `current_flags`.

        while (!current_flags) // Wait until we get a diagnostic to handle.
        {
            diagnostics_delay_ms(&current_flags, 100);
        }

        enum DiagnosticMask current_diagnostic = current_flags & -current_flags;

        switch (current_diagnostic)
        {

            case DiagnosticMask_stepper_driver_issue:
            {

                BUZZER_play(BuzzerTune_ambulance);

                while (BUZZER_current_tune())
                {

                    GPIO_TOGGLE  (led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );

                    if (diagnostics_delay_ms(&current_flags, 25))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }

                }

            } break;

            case DiagnosticMask_vn100_mishap:
            {

                BUZZER_play(BuzzerTune_hazard);

                while (BUZZER_current_tune())
                {

                    GPIO_INACTIVE(led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_TOGGLE  (led_channel_blue );

                    if (diagnostics_delay_ms(&current_flags, 25))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }

                }

            } break;

            case DiagnosticMask_angular_velocity_saturated:
            {

                BUZZER_play(BuzzerTune_heartbeat);

                while (BUZZER_current_tune())
                {

                    GPIO_TOGGLE  (led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );

                    if (diagnostics_delay_ms(&current_flags, 500))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }

                }

            } break;

            case DiagnosticMask_logging_mishap:
            {

                BUZZER_play(BuzzerTune_heavy_beep);

                while (BUZZER_current_tune())
                {

                    GPIO_TOGGLE  (led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_TOGGLE  (led_channel_blue );

                    if (diagnostics_delay_ms(&current_flags, 100))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }

                }

            } break;

            case DiagnosticMask_logging_heartbeat:
            {

                BUZZER_play(BuzzerTune_chirp);

                GPIO_INACTIVE(led_channel_red  );
                GPIO_ACTIVE  (led_channel_green);
                GPIO_INACTIVE(led_channel_blue );

                while (BUZZER_current_tune())
                {
                    if (diagnostics_delay_ms(&current_flags, 100))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }
                }

            } break;

            case DiagnosticMask_vn100_heartbeat:
            {

                BUZZER_play(BuzzerTune_burp);

                GPIO_INACTIVE(led_channel_red  );
                GPIO_INACTIVE(led_channel_green);
                GPIO_ACTIVE  (led_channel_blue );

                while (BUZZER_current_tune())
                {
                    if (diagnostics_delay_ms(&current_flags, 100))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }
                }

            } break;

            case DiagnosticMask_wiping_filesystem:
            {

                BUZZER_play(BuzzerTune_tetris);

                GPIO_ACTIVE(led_channel_red  );
                GPIO_ACTIVE(led_channel_green);
                GPIO_ACTIVE(led_channel_blue );

                while (BUZZER_current_tune())
                {

                    GPIO_TOGGLE(led_channel_red  );
                    GPIO_TOGGLE(led_channel_green);
                    GPIO_TOGGLE(led_channel_blue );

                    if (diagnostics_delay_ms(&current_flags, 100))
                    {
                        goto END_OF_DIAGNOSTIC;
                    }

                }

            } break;

            default: sus;

        }

        END_OF_DIAGNOSTIC:;

        current_flags &= ~current_diagnostic; // Acknowledge the completion of the diagnostic.

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(watchdog, 512, 2)
{
    for (;;)
    {

        FREERTOS_delay_ms(1000);



        // TODO Check if we've been able to control the stepper driver.
        // TODO Check if we've been receiving OpenMV data.
        // TODO Check if we've been receiving VN-100 data.
        // TODO Check if ESP32 still working.



        // To prevent the vehicle from being perpetually on forever
        // and drain the batteries empty, we turn ourselves off automatically.

        #if AUTOMATIC_SHUTDOWN_TIME_US > 0
        {
            if (TIMEKEEPING_microseconds() >= AUTOMATIC_SHUTDOWN_TIME_US)
            {

                // Indicate that this is why the vehicle is suddenly shut off.

                BUZZER_play(BuzzerTune_sleeping);
                while (BUZZER_current_tune());



                // Try cut off battery power.

                GPIO_INACTIVE(battery_allowed);
                FREERTOS_delay_ms(1'000'000);


                // If we're still alive by this point, then
                // there's probably external power or something,
                // to which we'll just do a software reset.

                WARM_RESET();

            }
        }
        #endif

    }
}
