#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////



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



FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{
    for (;;)
    {

        #include "DemoStepper_ANGULAR_VELOCITIES.meta"

        static i32 index = 0;

        while (true)
        {

            // If there are multiple motors involved,
            // a new angular velocity must be pushed for
            // all of them at the same time.
            // This demo however only has one motor involved,
            // that is the primary motor.

            b32 was_pushed =
                STEPPER_push_angular_velocities
                (
                    &(f32[])
                    {
                        [StepperInstanceHandle_axis_x] = ANGULAR_VELOCITIES[index],
                        [StepperInstanceHandle_axis_y] = 0,
                        [StepperInstanceHandle_axis_z] = 0,
                    }
                );

            if (was_pushed)
            {
                break; // The angular velocity was sucessfully queued up.
            }
            else
            {
                // The stepper driver's ring-buffer is full;
                // wait until there's space to queue the next
                // angular velocity.
            }

        }

        index += 1;
        index %= countof(ANGULAR_VELOCITIES);

    }
}
