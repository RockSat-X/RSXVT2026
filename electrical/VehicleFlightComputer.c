#define STEPPER_ENABLE_DELAY_US          500'000
#define STEPPER_VELOCITY_UPDATE_US       25'000 // @/`Sequence Angular Accelerations Delta Time`.
#define STEPPER_UART_TIME_MARGIN_US      2'000
#define STEPPER_RING_BUFFER_LENGTH       8      // TODO Determine latency.
#define SPI_BLOCK_SIZE                   64     // @/`OpenMV SPI Block Size`.
#define SPI_RECEPTION_RING_BUFFER_LENGTH 32
#define WATCHDOG_DURATION_US             (10 * 60'000'000)
#define MAX_ANGULAR_ACCELERATION         (200.0f)
#define MAX_ANGULAR_VELOCITY             (2000.0f * 2.0f * PI / 60.0f)
#define GOD_MODE                         true
#define CONTROLLER_ENABLE                true
#define VN100_ENABLE                     true
#define OPENMV_ENABLE                    true
#define WATCHDOG_ENABLE                  false
#define TRANSMIT_TV                      true

#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "spi.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"
#include "gnc.c"

// TODO Check if we've been receiving OpenMV data.
// TODO Check if ESP32 still working.



////////////////////////////////////////////////////////////////////////////////



enum OpenMVImageState : u32
{
    OpenMVImageState_empty,
    OpenMVImageState_filling,
    OpenMVImageState_filled,
    OpenMVImageState_using,
};

struct OpenMVImage
{
    volatile _Atomic enum OpenMVImageState state;
    u8                                     bytes[16 * 1024]; // Average ~3 KiB when using YUV422, QSIF, 50% quality.
    i32                                    size;
};



static struct
{

    RingBuffer(struct VN100Packet    , 8) vn100_packets;
    RingBuffer(struct OpenMVPacketGNC, 8) openmv_gnc_packets;
    volatile struct StepperTuple          current_angular_accelerations;
    volatile struct StepperTuple          current_angular_velocities;

    #if GOD_MODE
        volatile u32 replay_sequence_number;
    #endif

} CONTROLLER = {0};

static struct
{
    RingBuffer(struct VN100Packet    , 8) vn100_packets;
    RingBuffer(struct OpenMVPacketGNC, 8) openmv_gnc_packets;
} LOGGER = {0};

static struct
{
    volatile _Atomic b32 magnetic_disturbance_exists;
    volatile _Atomic b32 acceleration_disturbance_exists;
} VN100 = {0};

static struct
{
    struct OpenMVImage openmv_image;
} ESP32 = {0};



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



enum DiagnosticLEDBehavior : u32
{
    DiagnosticLEDBehavior_inactive,
    DiagnosticLEDBehavior_active,
    DiagnosticLEDBehavior_toggle,
};

#include "VehicleFlightComputer_diagnostics.meta"
/* #meta

    DIAGNOSTICS = ( # Ordered from highest priority to lowest priority.
        ('stepper_driver_issue'      , 'ambulance' , ('toggle'  , 'inactive', 'inactive'),  25),
        ('angular_velocity_saturated', 'heartbeat' , ('toggle'  , 'inactive', 'inactive'), 500),
        ('wiping_filesystem'         , 'tetris'    , ('toggle'  , 'inactive', 'toggle'  ), 100),
        ('vn100_mishap'              , 'hazard'    , ('inactive', 'inactive', 'toggle'  ),  25),
        ('logging_mishap'            , 'heavy_beep', ('toggle'  , 'inactive', 'toggle'  ), 100),
        ('vn100_heartbeat'           , 'burp'      , ('inactive', 'inactive', 'active'  ), 100),
        ('logging_heartbeat'         , 'chirp'     , ('inactive', 'active'  , 'inactive'), 100),
    )

    Meta.enums('DiagnosticMask', 'u32', (
        (name, f'1 << {diagnostic_i}')
        for diagnostic_i, (name, tune, (behavior_red, behavior_green, behavior_blue), delay_ms) in enumerate(DIAGNOSTICS)
    ))

    Meta.lut('DIAGNOSTIC_TABLE', ((
        (
            ('tune'          , f'BuzzerTune_{tune}'                     ),
            ('behavior_red'  , f'DiagnosticLEDBehavior_{behavior_red  }'),
            ('behavior_green', f'DiagnosticLEDBehavior_{behavior_green}'),
            ('behavior_blue' , f'DiagnosticLEDBehavior_{behavior_blue }'),
            ('delay_ms'      , f'{delay_ms}U'                           ),
        )
        for diagnostic_i, (name, tune, (behavior_red, behavior_green, behavior_blue), delay_ms) in enumerate(DIAGNOSTICS)
    )))

*/

FREERTOS_TASK(diagnostics, 512, 1)
{

    u32 current_flags = 0;

    for (;;)
    {



        // Check and wait for there to be a new diagnostic to handle.

        b32 waiting = false;

        do
        {

            u32 new_flags = {0};

            BaseType_t got_notification =
                xTaskNotifyWait
                (
                    0,
                    (u32) -1, // Immediately acknowledge the new diagnostics.
                    (uint32_t*) &new_flags,
                    waiting ? 100 : 0
                );

            if (got_notification)
            {
                current_flags |= new_flags;
            }

            waiting = true;

        }
        while (!current_flags);



        // Handle the highest priority diagnostic with buzzer tunes and blinking LEDs.

        enum DiagnosticMask current_diagnostic = current_flags & -current_flags;

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

        if (current_diagnostic && __builtin_ctz(current_diagnostic) < countof(DIAGNOSTIC_TABLE))
        {

            auto info = &DIAGNOSTIC_TABLE[__builtin_ctz(current_diagnostic)];

            BUZZER_play(info->tune);

            while (BUZZER_current_tune())
            {



                // Control the LEDs.

                switch (info->behavior_red)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_red); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_red); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_red); break;
                    default                             : sus;
                }

                switch (info->behavior_green)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_green); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_green); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_green); break;
                    default                             : sus;
                }

                switch (info->behavior_blue)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_blue); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_blue); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_blue); break;
                    default                             : sus;
                }



                // See if there's a new higher priority diagnostic we should handle.

                u32 new_flags = 0;

                BaseType_t got_notification =
                    xTaskNotifyWait
                    (
                        0,
                        (u32) -1, // Immediately acknowledge the new diagnostics.
                        (uint32_t*) &new_flags,
                        0
                    );

                if (got_notification)
                {
                    current_flags |= new_flags; // Add the new diagnostics to the list of existing ones to process.
                }

                enum DiagnosticMask new_diagnostic = current_flags & -current_flags;

                if (new_diagnostic == current_diagnostic)
                {

                    // Although `xTaskNotifyWait` has a time-out argument that could be used
                    // as a delay that can also be interrupted early when there's a notification,
                    // it's possible that a task set the same diagnostic repeatedly over and
                    // over again to which a delay never really happens. By using a regular
                    // task delay here, we're making things slightly less responsive because
                    // the delay won't allow for the `diagnostics` task to handle a new
                    // diagnostic of higher priority, but since this is literally just for
                    // diagnostics, it's okay.

                    FREERTOS_delay_ms(info->delay_ms);

                }
                else
                {
                    break; // There's a new diagnostic of higher priority that we should handle.
                }

            }

        }
        else
        {
            sus; // Invalid diagnostic..?
        }

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

        current_flags &= ~current_diagnostic; // Acknowledge the completion of the diagnostic.

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(controller, 1024, 0)
{

#if CONTROLLER_ENABLE

    STEPPER_reinit();



    // For diagnostic purposes, we immediately set angular velocities to
    // something non-zero so we can easily tell if something is wrong.

    #if GOD_MODE
    {
        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
        {
            CONTROLLER.current_angular_velocities.values[unit] = 1.0f;
        }
    }
    #endif



    for (;;)
    {



        // If requested, we play back a sequence of angular accelerations for simulation purposes.

        #if GOD_MODE
        {
            if (CONTROLLER.replay_sequence_number)
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

                CONTROLLER.current_angular_accelerations  = SEQUENCE_ANGULAR_ACCELERATIONS[CONTROLLER.replay_sequence_number - 1];
                CONTROLLER.replay_sequence_number        += 1;
                CONTROLLER.replay_sequence_number        %= countof(SEQUENCE_ANGULAR_ACCELERATIONS) + 1;

                if (!CONTROLLER.replay_sequence_number)
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



#include "VN100Command.meta"
/* #meta

    import functools

    COMMANDS = (
        ('disable_known_magnetic_disturbance'    , 'VNKMD,0'),
        ('enable_known_magnetic_disturbance'     , 'VNKMD,1'),
        ('disable_known_acceleration_disturbance', 'VNKAD,0'),
        ('enable_known_acceleration_disturbance' , 'VNKAD,1'),
    )

    messages = [
        f'${text}*{functools.reduce(lambda x, y: x^ord(y), text, 0):02X}'
        for name, text in COMMANDS
    ]

    Meta.enums('VN100Command', 'u32', (name for name, text in COMMANDS))

    Meta.lut('VN100Command_TABLE', (
        (
            f'VN100Command_{name}',
            ('message', f'(const u8*) "{message}"'),
            ('length' , len(message)),
        )
        for (name, text), message in zip(COMMANDS, messages)
    ))

*/

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

    u8 calculated_checksum = 0;

    for
    (
        i32 i = 1;                    // Starting character ($) not included.
        i < checksum_indicator_index; // Checksum field (*__) not included.
        i += 1
    )
    {
        calculated_checksum ^= dst_response_buffer[i];
    }

    u8 received_checksum = 0;

    received_checksum |=
        '0' <= dst_response_buffer[checksum_indicator_index + 1] && dst_response_buffer[checksum_indicator_index + 1] <= '9'
            ? (u8) (dst_response_buffer[checksum_indicator_index + 1] - '0'     ) << 4
            : (u8) (dst_response_buffer[checksum_indicator_index + 1] - 'A' + 10) << 4;

    received_checksum |=
        '0' <= dst_response_buffer[checksum_indicator_index + 2] && dst_response_buffer[checksum_indicator_index + 2] <= '9'
            ? (u8) (dst_response_buffer[checksum_indicator_index + 2] - '0'     ) << 0
            : (u8) (dst_response_buffer[checksum_indicator_index + 2] - 'A' + 10) << 0;

    if (calculated_checksum == received_checksum)
    {
        return VN100AwaitResponseResult_successful;
    }
    else
    {
        return VN100AwaitResponseResult_checksum_mismatch;
    }

}

static enum VN100AwaitCommandResult : u32
{
    VN100AwaitCommandResult_successful,
    VN100AwaitCommandResult_timeout,
    VN100AwaitCommandResult_checksum_mismatch,
    VN100AwaitCommandResult_missing_echo,
}
vn100_await_command(enum VN100Command command)
{

    UXART_tx_bytes
    (
        UXARTHandle_vn100,
        VN100Command_TABLE[command].message,
        VN100Command_TABLE[command].length
    );

    for (i32 response_index = 0;; response_index += 1)
    {

        u8  response_buffer[256] = {0};
        i32 response_length      = {0};

        enum VN100AwaitResponseResult response_result =
            vn100_await_response
            (
                response_buffer,
                countof(response_buffer),
                &response_length
            );

        switch (response_result)
        {

            case VN100AwaitResponseResult_successful:
            {

                b32 matching_response =
                    response_length == VN100Command_TABLE[command].length &&
                    !memcmp
                    (
                        response_buffer,
                        VN100Command_TABLE[command].message,
                        (u32) VN100Command_TABLE[command].length
                    );

                if (matching_response)
                {
                    return VN100AwaitCommandResult_successful; // Yippee!
                }
                else if (response_index < 16)
                {
                    // We got some other response from the VN-100;
                    // for now, let's just ignore it and hope the
                    // expected response is going to come in a bit later.
                }
                else
                {
                    // Seems like the VN-100 isn't echoing back
                    // the expected response to our command...
                    return VN100AwaitCommandResult_missing_echo;
                }

            } break;

            case VN100AwaitResponseResult_timeout           : return VN100AwaitCommandResult_timeout;
            case VN100AwaitResponseResult_checksum_mismatch : return VN100AwaitCommandResult_checksum_mismatch;
            default                                         : sus;

        }

    }

}

FREERTOS_TASK(vn100, 8192, 0)
{

#if VN100_ENABLE

    for (;;)
    {

        UXART_reinit(UXARTHandle_vn100);

        b32 active_magnetic_disturbance     = {0};
        b32 active_acceleration_disturbance = {0};
        i32 valid_packet_count              = 0;
        i32 consecutive_issue_count         = 0;

        for (i32 iteration_index = 0;; iteration_index += 1)
        {



            // See if we need to reconfigure the VN-100.

            b32 current_magnetic_disturbance =
                atomic_load_explicit
                (
                    &VN100.magnetic_disturbance_exists,
                    memory_order_relaxed // No synchronization needed.
                );

            b32 current_acceleration_disturbance =
                atomic_load_explicit
                (
                    &VN100.acceleration_disturbance_exists,
                    memory_order_relaxed // No synchronization needed.
                );

            if
            (
                iteration_index == 0                                                ||
                active_magnetic_disturbance     != current_magnetic_disturbance     ||
                active_acceleration_disturbance != current_acceleration_disturbance
            )
            {

                active_magnetic_disturbance     = current_magnetic_disturbance;
                active_acceleration_disturbance = current_acceleration_disturbance;

                { // Initialize the VNKMD state.

                    enum VN100AwaitCommandResult result =
                        vn100_await_command
                        (
                            active_magnetic_disturbance
                                ?  VN100Command_enable_known_magnetic_disturbance
                                :  VN100Command_disable_known_magnetic_disturbance
                        );

                    switch (result)
                    {
                        case VN100AwaitCommandResult_successful        : break;
                        case VN100AwaitCommandResult_timeout           : goto REINITIALIZE;
                        case VN100AwaitCommandResult_checksum_mismatch : goto REINITIALIZE;
                        case VN100AwaitCommandResult_missing_echo      : goto REINITIALIZE;
                        default                                        : sus;
                    }

                }

                { // Initialize the VNKAD state.

                    enum VN100AwaitCommandResult result =
                        vn100_await_command
                        (
                            active_acceleration_disturbance
                                ?  VN100Command_enable_known_acceleration_disturbance
                                :  VN100Command_disable_known_acceleration_disturbance
                        );

                    switch (result)
                    {
                        case VN100AwaitCommandResult_successful        : break;
                        case VN100AwaitCommandResult_timeout           : goto REINITIALIZE;
                        case VN100AwaitCommandResult_checksum_mismatch : goto REINITIALIZE;
                        case VN100AwaitCommandResult_missing_echo      : goto REINITIALIZE;
                        default                                        : sus;
                    }

                }

            }



            // The VN-100 is preprogrammed to automatically transmit the desired
            // register data; all we have to do to pick it up and parse it.

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

                        valid_packet_count      += 1;
                        consecutive_issue_count  = 0;

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
                    else // Perhaps a packet we don't recognize?
                    {

                        xTaskNotify
                        (
                            diagnostics_handle,
                            DiagnosticMask_vn100_mishap,
                            eSetBits
                        );

                        consecutive_issue_count += 1;

                    }

                } break;



                // Couldn't get a response...

                case VN100AwaitResponseResult_timeout:
                {
                    goto REINITIALIZE;
                } break;



                // Perhaps a mild signal integrity issue?

                case VN100AwaitResponseResult_checksum_mismatch:
                {

                    xTaskNotify
                    (
                        diagnostics_handle,
                        DiagnosticMask_vn100_mishap,
                        eSetBits
                    );

                    consecutive_issue_count += 1;

                } break;



                default: sus;

            }



            // If we're encountering too many soft
            // errors, then something is quite wrong.

            if (consecutive_issue_count >= 8)
            {
                goto REINITIALIZE;
            }

        }

        REINITIALIZE:;

        xTaskNotify
        (
            diagnostics_handle,
            DiagnosticMask_vn100_mishap,
            eSetBits
        );

    }

#else

    for (;;)
    {
        FREERTOS_delay_ms(1'000);
    }

#endif

}



////////////////////////////////////////////////////////////////////////////////



static useret b32
openmv_use_image(struct OpenMVImage* image)
{

    if (!image)
        sus;

    b32 observed_image_state =
        atomic_load_explicit
        (
            &image->state,
            memory_order_acquire
        );

    switch (observed_image_state)
    {

        case OpenMVImageState_empty:
        case OpenMVImageState_filling:
        {
            return false; // No image data available.
        } break;

        case OpenMVImageState_filled:
        {

            atomic_store_explicit
            (
                &image->state,
                OpenMVImageState_using,
                memory_order_release
            );

            return true; // User can read the image data now.

        } break;

        case OpenMVImageState_using:
        {

            atomic_store_explicit
            (
                &image->state,
                OpenMVImageState_empty,
                memory_order_release
            );

            return false; // The image data has been freed up.

        } break;

        default:
        {
            sus;
            memzero(image);
            return false; // Weird...
        } break;

    }

}

static void
openmv_process_packet_for_image(struct OpenMVImage* image, struct OpenMVPacket* packet)
{

    if (!image)
        sus;

    if (!packet)
        sus;

    b32 observed_image_state =
        atomic_load_explicit
        (
            &image->state,
            memory_order_acquire
        );

    switch (observed_image_state)
    {

        case OpenMVImageState_empty:
        {
            if (packet->sequence_number == 0) // @/`OpenMV Sequence Number`.
            {

                image->size = 0;

                atomic_store_explicit
                (
                    &image->state,
                    OpenMVImageState_filling, // We've resynchronized with the OpenMV and can get the next image.
                    memory_order_release
                );

            }
        } break;

        case OpenMVImageState_filling:
        {
            if (packet->sequence_number == 0) // @/`OpenMV Sequence Number`.
            {

                // We're now done collecting the image data,
                // although whether or not it's the entire image intact is a different question.
                // Note that because we're currently not double-buffering the images,
                // we can only process every other imagee at most. This should be
                // fine given that the bottle-neck will likely be in the RF transmission.

                atomic_store_explicit
                (
                    &image->state,
                    OpenMVImageState_filled,
                    memory_order_release
                );

            }
            else if
            (
                packet->sequence_number == image->size / sizeof(packet->image.bytes) + 1 &&
                image->size + sizeof(packet->image.bytes) <= sizeof(image->bytes)
            )
            {

                memmove // Append the new data to the image data we got so far.
                (
                    image->bytes + image->size,
                    packet->image.bytes,
                    sizeof(packet->image.bytes)
                );

                image->size += sizeof(packet->image.bytes);

            }
            else // We missed a packet or ran out of buffer space.
            {
                atomic_store_explicit
                (
                    &image->state,
                    OpenMVImageState_empty, // Just invalidate the image data we got so far.
                    memory_order_release
                );
            }
        } break;

        case OpenMVImageState_filled:
        case OpenMVImageState_using:
        {
            // Nothing we can do until the consumer is done with the image data.
        } break;

        default:
        {
            sus;
            memzero(image);
        } break;

    }

}

FREERTOS_TASK(openmv, 8192, 0)
{

#if OPENMV_ENABLE

    for (;;)
    {

        // Reset the OpenMV for a bit...

        GPIO_ACTIVE(openmv_reset);
        FREERTOS_delay_ms(10);



        // Reboot our SPI communication...

        SPI_reinit(SPIHandle_openmv);



        // Reawaken the OpenMV!

        FREERTOS_delay_ms(10);
        GPIO_INACTIVE(openmv_reset);



        // Start processing packet data from the OpenMV.

        while (true)
        {

            static_assert(sizeof(struct OpenMVPacket) == sizeof(struct SPIBlock));

            struct OpenMVPacket* packet = (struct OpenMVPacket*) RingBuffer_reading_pointer(SPI_reception(SPIHandle_openmv));

            if (packet)
            {



                // Determine if the packet correspond to GNC data.

                if (packet->sequence_number == 0) // @/`OpenMV Sequence Number`.
                {

                    if (!RingBuffer_push(&CONTROLLER.openmv_gnc_packets, &packet->gnc))
                    {
                        // OpenMV data overrun!
                    }

                    if (!RingBuffer_push(&LOGGER.openmv_gnc_packets, &packet->gnc))
                    {
                        // OpenMV data overrun, but it's for the logger; who cares.
                    }

                }



                // If applicable and possible, have the packet's
                // image data be queued up for RF transmission.

                openmv_process_packet_for_image(&ESP32.openmv_image, packet);



                // For diagnostic purposes, we can also redirect the image
                // data to the ST-Link to be viewed in real-time-ish.

                #if TRANSMIT_TV
                {

                    static struct OpenMVImage tv_image = {0};

                    openmv_process_packet_for_image(&tv_image, packet);

                    if (openmv_use_image(&tv_image))
                    {
                        stlink_tx(TV_TOKEN_START);
                        UXART_tx_bytes(UXARTHandle_stlink, tv_image.bytes, tv_image.size);
                        stlink_tx(TV_TOKEN_END);
                    }

                }
                #endif



                // Acknowledge the packet.

                if (!RingBuffer_pop(SPI_reception(SPIHandle_openmv), nullptr))
                    sus;

            }
            else
            {
                FREERTOS_delay_ms(1);
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

            u32                    current_timestamp_us    = TIMEKEEPING_microseconds();
            struct VN100Packet     vn100_packet_data       = {0};
            b32                    vn100_packet_exist      = RingBuffer_pop_to_latest(&LOGGER.vn100_packets, &vn100_packet_data);
            struct OpenMVPacketGNC openmv_gnc_packet_data  = {0};
            b32                    openmv_gnc_packet_exist = RingBuffer_pop_to_latest(&LOGGER.openmv_gnc_packets, &openmv_gnc_packet_data);

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
                    "VNKMD          : %s"                 "\n"
                    "VNKAD          : %s"                 "\n"
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
                    false                                 ? "Yes" : "No", // TODO.
                    VN100.magnetic_disturbance_exists     ? "Yes" : "No",
                    VN100.acceleration_disturbance_exists ? "Yes" : "No"
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

            #if !TRANSMIT_TV // Can't conflict with sending image data over ST-Link.
            {
                if (current_timestamp_us - most_recent_stlink_log_timestamp_us >= 250'000)
                {

                    most_recent_stlink_log_timestamp_us = current_timestamp_us;

                    stlink_tx
                    (
                        "%.*s",
                        log_entry_length,
                        working_sectors[0].bytes
                    );

                    if (openmv_gnc_packet_exist) // TODO Better output...?
                    {
                        stlink_tx
                        (
                            "attitude_x                         : %f" "\n"
                            "attitude_y                         : %f" "\n"
                            "attitude_z                         : %f" "\n"
                            "attitude_w                         : %f" "\n"
                            "computer_vision_processing_time_ms : %u" "\n"
                            "computer_vision_confidence         : %u" "\n"
                            "\n",
                            openmv_gnc_packet_data.attitude_x,
                            openmv_gnc_packet_data.attitude_y,
                            openmv_gnc_packet_data.attitude_z,
                            openmv_gnc_packet_data.attitude_w,
                            openmv_gnc_packet_data.computer_vision_processing_time_ms,
                            openmv_gnc_packet_data.computer_vision_confidence
                        );
                    }

                }
            }
            #endif



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



FREERTOS_TASK(god, 1024, 2)
{

#if GOD_MODE

    for (;;)
    {

        u8 input = {0};

        while (stlink_rx(&input))
        {
            switch (input)
            {



                // Stepper motor control.

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

                case 'x':
                {

                    for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                    {
                        CONTROLLER.current_angular_accelerations.values[unit] = 0.0f;
                        CONTROLLER.current_angular_velocities   .values[unit] = 0.0f;
                    }

                    CONTROLLER.replay_sequence_number = 1;

                } break;



                // VN-100.

                case 'm':
                {
                    atomic_fetch_xor_explicit
                    (
                        &VN100.magnetic_disturbance_exists,
                        -1,
                        memory_order_relaxed // No synchronization needed.
                    );
                } break;

                case 'a':
                {
                    atomic_fetch_xor_explicit
                    (
                        &VN100.acceleration_disturbance_exists,
                        -1,
                        memory_order_relaxed // No synchronization needed.
                    );
                } break;



                // Misc.

                case 'b':
                {
                    GPIO_TOGGLE(battery_allowed);
                } break;



                default:
                {
                    // Don't care.
                } break;

            }
        }

        FREERTOS_delay_ms(10);

    }

#else

    for (;;)
    {
        FREERTOS_delay_ms(1'000);
    }

#endif

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(watchdog, 512, 2)
{

#if WATCHDOG_ENABLE

    for (;;)
    {

        // To prevent the vehicle from being perpetually on forever
        // and drain the batteries empty, we turn ourselves off automatically.

        if (TIMEKEEPING_microseconds() >= WATCHDOG_DURATION_US)
        {

            // Indicate that this is why the vehicle is suddenly shut off.

            BUZZER_play(BuzzerTune_mario);
            while (BUZZER_current_tune());



            // Try cut off battery power.

            GPIO_INACTIVE(battery_allowed);
            FREERTOS_delay_ms(1'000'000);


            // If we're still alive by this point, then
            // there's probably external power or something,
            // to which we'll just do a software reset.

            WARM_RESET();

        }

        FREERTOS_delay_ms(1'000);

    }

#else

    for (;;)
    {
        FREERTOS_delay_ms(1'000);
    }

#endif

}
