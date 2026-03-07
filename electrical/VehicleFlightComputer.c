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

            if (stlink_rx(&input))
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

        b32 max_angular_velocity_has_already_been_reached = false;
        b32 max_angular_velocity_reached                  = false;

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

            max_angular_velocity_has_already_been_reached |=
                current_angular_velocities.values[unit] >=  MAX_ANGULAR_VELOCITY ||
                current_angular_velocities.values[unit] <= -MAX_ANGULAR_VELOCITY;

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



        // Indicate that the max angular velocity has been reached.

        GPIO_SET(led_channel_red  , max_angular_velocity_reached);
        GPIO_SET(led_channel_green, false);
        GPIO_SET(led_channel_blue , false);

        if (!max_angular_velocity_has_already_been_reached && max_angular_velocity_reached)
        {
            BUZZER_play(BuzzerTune_heartbeat);
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

                case StepperPushAngularVelocitiesResult_no_response_from_unit  : sorry
                case StepperPushAngularVelocitiesResult_bad_response_from_unit : sorry
                case StepperPushAngularVelocitiesResult_bug                    : sorry
                default                                                        : sorry

            }

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(logger, 2048, 0)
{
    for (;;)
    {

        // TODO Optional SD card logging.

        stlink_tx
        (
            "Angular acceleration : <%.3f, %.3f, %.3f>" "\n"
            "Angular velocity     : <%.3f, %.3f, %.3f>" "\n"
            "RPM                  : <%.3f, %.3f, %.3f>" "\n"
            "\n\n",
            current_angular_accelerations.values[StepperUnit_axis_x],
            current_angular_accelerations.values[StepperUnit_axis_y],
            current_angular_accelerations.values[StepperUnit_axis_z],
            current_angular_velocities.values[StepperUnit_axis_x],
            current_angular_velocities.values[StepperUnit_axis_y],
            current_angular_velocities.values[StepperUnit_axis_z],
            current_angular_velocities.values[StepperUnit_axis_x] / (2.0f * PI) * 60.0f,
            current_angular_velocities.values[StepperUnit_axis_y] / (2.0f * PI) * 60.0f,
            current_angular_velocities.values[StepperUnit_axis_z] / (2.0f * PI) * 60.0f
        );

        spinlock_us(100'000);

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(watchdog, 512, 1)
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
                spinlock_us(1'000'000);


                // If we're still alive by this point, then
                // there's probably external power or something,
                // to which we'll just do a software reset.

                WARM_RESET();

            }
        }
        #endif

    }
}
