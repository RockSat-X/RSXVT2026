#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"
#include "gnc.c"



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



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



    // When the vehicle becomes powered on,
    // it's typically because of the external
    // power suplly through the vehicle interface.
    //
    // TODO Add a delay before we enable the battery?
    // TODO Check if there's actually external power?

    BUZZER_play(BuzzerTune_three_tone);
    while (BUZZER_current_tune());

    GPIO_ACTIVE(battery_allowed);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Update motor angular velocities.
//



static volatile f32 current_angular_acceleration = 0.0f;
static volatile f32 current_angular_velocity     = 1.0f;

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    STEPPER_reinit();

    for (;;)
    {

        #define MAX_ANGULAR_VELOCITY (4300.0f * 2.0f * PI / 60.0f)

        b32 max_angular_velocity_has_already_been_reached =
            current_angular_velocity >=  MAX_ANGULAR_VELOCITY ||
            current_angular_velocity <= -MAX_ANGULAR_VELOCITY;



        // Find new angular velocity.

        current_angular_velocity += current_angular_acceleration * (STEPPER_VELOCITY_UPDATE_US / 100'000.0f);



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



        // Queue up the angular velocity.

        for (b32 yield = false; !yield;)
        {

            enum StepperPushAngularVelocitiesResult result =
                STEPPER_push_angular_velocities
                (
                    &(StepperAngularVelocities)
                    {
                        [StepperUnit_axis_x] = current_angular_velocity,
                        [StepperUnit_axis_y] = current_angular_velocity,
                        [StepperUnit_axis_z] = current_angular_velocity,
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
                    // Keep spin-locking.
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
//
// Take user input and do stuff.
//



FREERTOS_TASK(user_inputter, 1024, 0)
{
    for (;;)
    {

        u8 input = {0};
        while (!stlink_rx(&input));

        switch (input)
        {
            case 'j': current_angular_acceleration += -0.1f; break;
            case 'J': current_angular_acceleration += -1.0f; break;
            case 'k': current_angular_acceleration +=  0.1f; break;
            case 'K': current_angular_acceleration +=  1.0f; break;
            case '0': current_angular_acceleration  =  0.0f; break;
            default: break;
        }

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// Periodically log out stuff.
//



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
