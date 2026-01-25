#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"
#include "timekeeping.c"



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_stepper_uart);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Initialize timekeeping.

    {

        // Enable the timekeeping's timer peripheral.

        CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



        // Configure the divider to set the rate at
        // which the timer's counter will increment.

        CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



        // Trigger an update event so that the shadow registers
        // are what we initialize them to be.
        // The hardware uses shadow registers in order for updates
        // to these registers not result in a corrupt timer output.

        CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



        // Enable the timekeeping timer's counter.

        CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);

    }



    // More peripheral initializations that depend on the above initializations.

    BUZZER_partial_init();
    STEPPER_partial_reinit();



    // When the vehicle becomes powered on,
    // it's typically because of the external
    // power suplly through the vehicle interface.
    //
    // TODO Add a delay before we enable the battery?
    // TODO Check if there's actually external power?

    BUZZER_play(BuzzerTune_three_tone);
    BUZZER_spinlock_to_completion();

    GPIO_ACTIVE(battery_allowed);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Update motor angular velocities.
//



static volatile f32 current_angular_acceleration = 0.0f;

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    static f32 current_angular_velocity = 1.0f;

    for (;;)
    {

        #define MAX_ANGULAR_VELOCITY (2.0f * PI * 8.0f)

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

        while
        (
            !STEPPER_push_angular_velocities
            (
                &(f32[])
                {
                    [StepperInstanceHandle_axis_x] = current_angular_velocity,
                    [StepperInstanceHandle_axis_y] = 0,
                    [StepperInstanceHandle_axis_z] = 0,
                }
            )
        );

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

        char input = {0};
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



FREERTOS_TASK(logger, 1024, 0)
{
    for (;;)
    {
        stlink_tx("Angular acceleration: %.6f\n", current_angular_acceleration);
        spinlock_us(10'000);
    }
}
