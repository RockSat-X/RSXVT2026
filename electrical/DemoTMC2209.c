#include "system.h"
#include "uxart.c"
#include "timekeeping.c"



static i8  step_buffer_data[32] = {0};
static u32 step_buffer_reader   = 0;
static u32 step_buffer_writer   = 0;



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Enable the peripheral.

    CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);



    ////////////////////////////////////////////////////////////////////////////////



    // Enable the peripheral.

    CMSIS_SET(RCC, APB2ENR, TIM15EN, true);



    // Channel output in PWM mode so we can generate a pulse.

    CMSIS_SET(TIM15, CCMR1, OC1M, 0b0111);



    // The comparison channel output is inactive
    // when the counter is below this value.

    CMSIS_SET(TIM15, CCR1, CCR1, 1);



    // Enable the comparison channel output.

    CMSIS_SET(TIM15, CCER, CC1E, true);



    // Master enable for the timer's outputs.

    CMSIS_SET(TIM15, BDTR, MOE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIM15, PSC, PSC, STPY_TIM15_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIM15, EGR, UG, true);



    CMSIS_SET
    (
        TIM15, CR1 ,
        URS  , 0b1 , // So that the UG bit doesn't set the update event interrupt.
        OPM  , true, // Timer's counter stops incrementing after an update event.
    );



    // Enable interrupt on update events.

    CMSIS_SET(TIM15, DIER, UIE, true);
    NVIC_ENABLE(TIM15);



    ////////////////////////////////////////////////////////////////////////////////



    GPIO_LOW(driver_enable);

    #define STEP_PERIOD_US 25'000

    for (;;)
    {

        // See if we can queue up the
        // next sequence of steps to take.

        if (step_buffer_writer - step_buffer_reader < countof(step_buffer_data))
        {

            // Compute the steps that'll need to be taken.

            static i32 index = 0;

            #include "DemoTMC2209_STEPS.meta"
            /* #meta

                import math

                with Meta.enter(f'static const i8 STEPS[] ='):
                    for i in range(4000):
                        Meta.line(f'''
                            {round(math.sin(i / 4000 * 2 * math.pi * 16) * 127)},
                        ''')

            */

            i8 steps = STEPS[index];

            index += 1;
            index %= countof(STEPS);


            // Push into the ring-buffer.

            step_buffer_data[step_buffer_writer % countof(step_buffer_data)]  = steps;
            step_buffer_writer                                               += 1;

        }

    }

}



INTERRUPT_TIM15
{

    // See if the timer has finished pulsing steps;
    // if so, the next sequence of steps can be queued up now.

    if (CMSIS_GET(TIM15, SR, UIF))
    {

        CMSIS_SET(TIM15, SR, UIF, false); // Acknowledge timer's update flag.



        // Determine the amount of steps we'll
        // need to take now and the direction.

        i32 steps = {0};

        if (step_buffer_reader == step_buffer_writer)
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
            steps               = step_buffer_data[step_buffer_reader % countof(step_buffer_data)];
            step_buffer_reader += 1;
        }

        i32 abs_steps = steps < 0 ? -steps : steps;



        // Set the direction arbitrarily.

        GPIO_SET(driver_direction, steps > 0);



        // Set the amount of step pulses to be done.
        // Note that this field has it so a value of
        // 0 encodes a repetition of 1, so in the event
        // of no steps to be done, there still needs to
        // be a repetition.

        CMSIS_SET(TIM15, RCR, REP, abs_steps ? (abs_steps - 1) : 0);



        // So to prevent a pulse in the case of zero steps,
        // the comparison value is changed to the largest
        // value so the pulse won't occur.
        // In any other case, the comparison value is non-zero
        // so that the waveform has its rising edge slighly
        // delayed from when the DIR signal is updated.

        CMSIS_SET(TIM15, CCR1, CCR1, abs_steps ? 1 : -1);



        // The amount of time between each step pulse
        // is set so that after N steps, a fixed amount
        // of time has passed. This allows us to update
        // the direction and rate of stepping at fixed
        // intervals.

        CMSIS_SET(TIM15, ARR, ARR, (STEP_PERIOD_US - 1) / (abs_steps ? abs_steps : 1));



        // Generate an update event so the above configuration
        // will be loaded into the timer's shadow registers.

        CMSIS_SET(TIM15, EGR, UG, true);



        // Enable the timer's counter; the counter will
        // become disabled after the end of step repetitions.

        CMSIS_SET(TIM15, CR1, CEN, true);

    }

}
