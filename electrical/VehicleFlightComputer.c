#define STEPPER_ENABLE_DELAY_US     500'000
#define STEPPER_VELOCITY_UPDATE_US   25'000 // @/`Sequence Angular Accelerations Delta Time`.
#define STEPPER_UART_TIME_MARGIN_US   2'000
#define STEPPER_RING_BUFFER_LENGTH  8       // TODO Determine latency.
#define AUTOMATIC_SHUTDOWN_TIME_US  0       // TODO Once finalized, we should use (10 * 60'000'000).
#define MAX_ANGULAR_ACCELERATION    (200.0f)
#define MAX_ANGULAR_VELOCITY        (2000.0f * 2.0f * PI / 60.0f)
#define DEMONSTRATE_STEPPER         true

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
    DiagnosticMask_logging_mishap             = 1 << 3,
    DiagnosticMask_logging_heartbeat          = 1 << 4,
};



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



static volatile struct StepperTuple current_angular_accelerations = {0};
static volatile struct StepperTuple current_angular_velocities    = {0};

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    STEPPER_reinit();



    // For diagnostic purposes, we immediately set angular velocities to
    // something non-zero so we can easily tell if something is wrong.

    #if DEMONSTRATE_STEPPER
    {
        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
        {
            current_angular_velocities.values[unit] = 1.0f;
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
                            current_angular_accelerations.values[unit] -= 0.1f;
                        }
                    } break;

                    case 'J':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] -= 1.0f;
                        }
                    } break;

                    case 'k':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] += 0.1f;
                        }
                    } break;

                    case 'K':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] += 1.0f;
                        }
                    } break;

                    case '0':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] = 0.0f;
                            current_angular_velocities   .values[unit] = 0.0f;
                        }
                    } break;

                    case '<':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] -= 200.0f;
                        }
                    } break;

                    case '>':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit] += 200.0f;
                        }
                    } break;

                    case '-':
                    {
                        for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                        {
                            current_angular_accelerations.values[unit]  =  0.0f;
                            current_angular_velocities   .values[unit] *= -1.0f;
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
                            current_angular_accelerations.values[unit] = 0.0f;
                            current_angular_velocities   .values[unit] = 0.0f;
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

                current_angular_accelerations  = SEQUENCE_ANGULAR_ACCELERATIONS[replay_sequence_number - 1];
                replay_sequence_number        += 1;
                replay_sequence_number        %= countof(SEQUENCE_ANGULAR_ACCELERATIONS) + 1;

                if (!replay_sequence_number)
                {
                    for (enum StepperUnit unit = {0}; unit < StepperUnit_COUNT; unit += 1)
                    {
                        current_angular_accelerations.values[unit] = 0.0f;
                        current_angular_velocities   .values[unit] = 0.0f;
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

            if (current_angular_accelerations.values[unit] >= MAX_ANGULAR_ACCELERATION)
            {
                current_angular_accelerations.values[unit] = MAX_ANGULAR_ACCELERATION;
            }
            else if (current_angular_accelerations.values[unit] <= -MAX_ANGULAR_ACCELERATION)
            {
                current_angular_accelerations.values[unit] = -MAX_ANGULAR_ACCELERATION;
            }



            // Find new angular velocity.

            current_angular_velocities.values[unit] += current_angular_accelerations.values[unit] * (f32) STEPPER_VELOCITY_UPDATE_US / 1'000'000.0f;



            // Limit the angular velocity.

            if (current_angular_velocities.values[unit] >= MAX_ANGULAR_VELOCITY)
            {
                current_angular_velocities.values[unit] = MAX_ANGULAR_VELOCITY;
                max_angular_velocity_reached            = true;
            }
            else if (current_angular_velocities.values[unit] <= -MAX_ANGULAR_VELOCITY)
            {
                current_angular_velocities.values[unit] = -MAX_ANGULAR_VELOCITY;
                max_angular_velocity_reached            = true;
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

            enum StepperPushAngularVelocitiesResult result = STEPPER_push_angular_velocities((struct StepperTuple*) &current_angular_velocities);

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

                    memzero((struct StepperTuple*) &current_angular_accelerations);
                    memzero((struct StepperTuple*) &current_angular_velocities   );

                    STEPPER_reinit();

                    yield = true;

                } break;

            }

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(logger, 8196, 0)
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

            u32 current_timestamp_us = TIMEKEEPING_microseconds();

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
                    "Quaternion     : <%f, %f, %f, %f>"   "\n"
                    "Magnetometer   : <%f, %f, %f>"       "\n"
                    "Accelerometer  : <%f, %f, %f>"       "\n"
                    "Gyroscope      : <%f, %f, %f>"       "\n"
                    "Ext. power     : %s"                 "\n"
                    "\n",
                    current_timestamp_us,
                    0, // TODO.
                    current_angular_accelerations.values[StepperUnit_axis_x],
                    current_angular_accelerations.values[StepperUnit_axis_y],
                    current_angular_accelerations.values[StepperUnit_axis_z],
                    current_angular_velocities.values[StepperUnit_axis_x],
                    current_angular_velocities.values[StepperUnit_axis_y],
                    current_angular_velocities.values[StepperUnit_axis_z],
                    current_angular_velocities.values[StepperUnit_axis_x] / (2.0f * PI) * 60.0f,
                    current_angular_velocities.values[StepperUnit_axis_y] / (2.0f * PI) * 60.0f,
                    current_angular_velocities.values[StepperUnit_axis_z] / (2.0f * PI) * 60.0f,
                    0, // TODO.
                    0, // TODO.
                    0, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
                    0.0f, // TODO.
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

            default: sorry

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
