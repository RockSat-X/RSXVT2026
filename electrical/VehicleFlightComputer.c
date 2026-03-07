#define AUTOMATIC_SHUTDOWN_TIME_US 0 // TODO Once finalized, we should use (10 * 60'000'000).
#define MAX_ANGULAR_ACCELERATION   (200.0f)
#define MAX_ANGULAR_VELOCITY       (2000.0f * 2.0f * PI / 60.0f)


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



static volatile f32 current_angular_acceleration = 0.0f; // TODO Atomic?
static volatile f32 current_angular_velocity     = 1.0f; // TODO Atomic?

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    STEPPER_reinit();

    for (;;)
    {



        // Interpret user's input, if any.

        u8 input = {0};

        if (stlink_rx(&input))
        {
            switch (input)
            {
                case 'j': current_angular_acceleration +=   -0.1f;                                    break;
                case 'J': current_angular_acceleration +=   -1.0f;                                    break;
                case 'k': current_angular_acceleration +=    0.1f;                                    break;
                case 'K': current_angular_acceleration +=    1.0f;                                    break;
                case '0': current_angular_acceleration  =    0.0f;                                    break;
                case '<': current_angular_acceleration += -200.0f;                                    break;
                case '>': current_angular_acceleration +=  200.0f;                                    break;
                case ' ': current_angular_acceleration  =    0.0f; current_angular_velocity  =  0.0f; break;
                case '-': current_angular_acceleration  =    0.0f; current_angular_velocity *= -1.0f; break;
                default: break;
            }
        }



        // Limit the acceleration.

        if (current_angular_acceleration >= MAX_ANGULAR_ACCELERATION)
        {
            current_angular_acceleration = MAX_ANGULAR_ACCELERATION;
        }
        else if (current_angular_acceleration <= -MAX_ANGULAR_ACCELERATION)
        {
            current_angular_acceleration = -MAX_ANGULAR_ACCELERATION;
        }



        // Find new angular velocity.

        b32 max_angular_velocity_has_already_been_reached =
            current_angular_velocity >=  MAX_ANGULAR_VELOCITY ||
            current_angular_velocity <= -MAX_ANGULAR_VELOCITY;

        current_angular_velocity += current_angular_acceleration * (f32) STEPPER_VELOCITY_UPDATE_US / 1'000'000.0f;



        // Limit the angular velocity.

        b32 max_angular_velocity_reached = false;

        if (current_angular_velocity >= MAX_ANGULAR_VELOCITY)
        {
            current_angular_velocity     = MAX_ANGULAR_VELOCITY;
            max_angular_velocity_reached = true;
        }
        else if (current_angular_velocity <= -MAX_ANGULAR_VELOCITY)
        {
            current_angular_velocity     = -MAX_ANGULAR_VELOCITY;
            max_angular_velocity_reached = true;
        }



        // Indicate that the max angular velocity has been reached.

        GPIO_SET(led_channel_red, max_angular_velocity_reached);

        if (!max_angular_velocity_has_already_been_reached && max_angular_velocity_reached)
        {
            BUZZER_play(BuzzerTune_heartbeat);
        }



        // Queue up the new angular velocity.

        for (b32 yield = false; !yield;)
        {

            enum StepperPushAngularVelocitiesResult result =
                STEPPER_push_angular_velocities
                (
                    &(struct StepperTuple)
                    {
                        .values =
                            {
                                [StepperUnit_axis_x] = current_angular_velocity,
                                [StepperUnit_axis_y] = current_angular_velocity,
                                [StepperUnit_axis_z] = current_angular_velocity,
                            }
                    }
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

        stlink_tx
        (
            "Angular acceleration : %.6f" "\n"
            "Angular velocity     : %.6f" "\n"
            "RPM                  : %.6f" "\n"
            "\n\n",
            current_angular_acceleration,
            current_angular_velocity,
            current_angular_velocity / (2.0f * PI) * 60.0f
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


        #if AUTOMATIC_SHUTDOWN_TIME_US > 0
        {

            // To prevent the vehicle from being perpetually on forever
            // and drain the batteries empty, we turn ourselves off automatically.

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
