#define STEP_PERIOD_US 25'000



#include "stepper_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'Stepper',
        cmsis_name  = 'TIM',
        common_name = 'TIMx',
        entries     = (
            { 'name'      : '{}'          , 'value': ... },
            { 'interrupt' : 'INTERRUPT_{}'               },
        ),
    )

*/



struct StepperDriver
{
    i8           deltas[32];
    volatile u32 reader;
    volatile u32 writer;
};




static struct StepperDriver _Stepper_drivers[StepperHandle_COUNT] = {0};



static void
_Stepper_driver_interrupt(enum StepperHandle handle)
{

    _EXPAND_HANDLE



    // See if the timer has finished pulsing steps;
    // if so, the next sequence of steps can be queued up now.

    if (CMSIS_GET(TIMx, SR, UIF))
    {

        CMSIS_SET(TIMx, SR, UIF, false); // Acknowledge timer's update flag.



        // Determine the amount of steps we'll
        // need to take now and the direction.

        i32 steps = {0};

        if (driver->reader == driver->writer)
        {
            // Underflow condition.
            // The step ring-buffer was exhausted and
            // no steps will be taken during this period.
            // TODO Indicate this situation somehow.
            steps = 0;
        }
        else
        {
            // Pop from the ring-buffer the next
            // signed amount of steps to take.
            i32 read_index  = driver->reader % countof(driver->deltas);
            steps           = driver->deltas[read_index];
            driver->reader += 1;
        }

        i32 abs_steps = steps < 0 ? -steps : steps;



        // Set the direction arbitrarily.

        GPIO_SET(driver_direction, steps > 0);



        // Set the amount of step pulses to be done.
        // Note that this field has it so a value of
        // 0 encodes a repetition of 1, so in the event
        // of no steps to be done, there still needs to
        // be a repetition.

        CMSIS_SET(TIMx, RCR, REP, abs_steps ? (abs_steps - 1) : 0);



        // So to prevent a pulse in the case of zero steps,
        // the comparison value is changed to the largest
        // value so the pulse won't occur.
        // In any other case, the comparison value is non-zero
        // so that the waveform has its rising edge slighly
        // delayed from when the DIR signal is updated.

        CMSIS_SET(TIMx, CCR1, CCR1, abs_steps ? 1 : -1);



        // The amount of time between each step pulse
        // is set so that after N steps, a fixed amount
        // of time has passed. This allows us to update
        // the direction and rate of stepping at fixed
        // intervals.

        CMSIS_SET(TIMx, ARR, ARR, (STEP_PERIOD_US - 1) / (abs_steps ? abs_steps : 1));



        // Generate an update event so the above configuration
        // will be loaded into the timer's shadow registers.

        CMSIS_SET(TIMx, EGR, UG, true);



        // Enable the timer's counter; the counter will
        // become disabled after the end of step repetitions.

        CMSIS_SET(TIMx, CR1, CEN, true);

    }

}
