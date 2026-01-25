#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"



////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_stepper_uart);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // More peripheral initializations that depend on the above initializations.

    BUZZER_partial_init();
    STEPPER_partial_reinit();



    // Whent the vehicle becomes powered on,
    // it's typically because of the external
    // power suplly through the vehicle interface.
    //
    // TODO Add a delay before we enable the battery?
    // TODO Check if there's actually external power?

    GPIO_ACTIVE(battery_allowed);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



static volatile f32 current_angular_acceleration = 0.0f;

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    static f32 current_angular_velocity = 1.0f;

    for (;;)
    {

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

        current_angular_velocity += current_angular_acceleration * (STEPPER_VELOCITY_UPDATE_US / 100'000.0f);

        f32 limit = 2.0f * PI * 8.0f;

        if (current_angular_velocity > limit)
        {
            current_angular_velocity = limit;
            GPIO_ACTIVE(led_channel_red);
        }
        else if (current_angular_velocity < -limit)
        {
            current_angular_velocity = -limit;
            GPIO_ACTIVE(led_channel_red);
        }
        else
        {
            GPIO_INACTIVE(led_channel_red);
        }

    }

}



////////////////////////////////////////////////////////////////////////////////



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



FREERTOS_TASK(logger, 1024, 0)
{
    for (;;)
    {
        stlink_tx("Angular acceleration: %.6f\n", current_angular_acceleration);
        spinlock_nop(1'000'000);
    }
}
