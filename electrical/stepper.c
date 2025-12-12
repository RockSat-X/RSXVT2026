#define STEPPER_PERIOD_US 25'000



#include "stepper_driver_support.meta"
/* #meta

    # As of writing, the timer's counter
    # frequency must be 1 MHz; this is mostly
    # arbitrary can be subject to customization.

    for target in TARGETS:

        for driver in target.drivers:
            if driver['type'] != 'Stepper':
                continue

            timer = driver['peripheral']

            if target.schema.get(property := f'{timer}_COUNTER_RATE', None) != 1_000_000:
                raise ValueError(
                    f'The stepper driver '
                    f'for target {repr(target.name)} '
                    f'requires {repr(property)} '
                    f'in the schema with value of {1_000_000}.'
                )



    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'Stepper',
        cmsis_name  = 'TIM',
        common_name = 'TIMx',
        entries     = (
            { 'name' : '{}'                           , 'value'       : ...                                                                                 },
            { 'name' : 'NVICInterrupt_{}_update_event', 'value'       : lambda driver: f'NVICInterrupt_{driver['interrupt']}'                               },
            { 'name' : 'STPY_{}_DIVIDER'              , 'value'       : ...                                                                                 },
            { 'name' : '{}_ENABLE'                    , 'cmsis_tuple' : ...                                                                                 },
            { 'name' : '{}_CAPTURE_COMPARE_ENABLE_y'  , 'cmsis_tuple' : lambda driver: f'{driver['peripheral']}_CAPTURE_COMPARE_ENABLE_{driver['channel']}' },
            { 'name' : '{}_CAPTURE_COMPARE_VALUE_y'   , 'cmsis_tuple' : lambda driver: f'{driver['peripheral']}_CAPTURE_COMPARE_VALUE_{driver['channel']}'  },
            { 'name' : '{}_CAPTURE_COMPARE_MODE_y'    , 'cmsis_tuple' : lambda driver: f'{driver['peripheral']}_CAPTURE_COMPARE_MODE_{driver['channel']}'   },
            { 'name' : 'INTERRUPT_{}_update_event'    , 'interrupt'   : lambda driver: f'INTERRUPT_{driver['interrupt']}'                                   },
        ),
    )

*/



struct StepperDriver
{
    i8           deltas[32];
    volatile u32 reader;
    volatile u32 writer;
};




static struct StepperDriver _STEPPER_drivers[StepperHandle_COUNT] = {0};



static void
_STEPPER_partial_init(enum StepperHandle handle)
{

    _EXPAND_HANDLE



    // Enable the peripheral.

    CMSIS_PUT(TIMx_ENABLE, true);



    // Channel output in PWM mode so we can generate a pulse.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_MODE_y, 0b0111);



    // The comparison channel output is inactive
    // when the counter is below this value.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_VALUE_y, 1);



    // Enable the comparison channel output.

    CMSIS_PUT(TIMx_CAPTURE_COMPARE_ENABLE_y, true);



    // Master enable for the timer's outputs.

    CMSIS_SET(TIMx, BDTR, MOE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMx, PSC, PSC, STPY_TIMx_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMx, EGR, UG, true);



    CMSIS_SET
    (
        TIMx, CR1 ,
        URS , 0b1 , // So that the UG bit doesn't set the update event interrupt.
        OPM , true, // Timer's counter stops incrementing after an update event.
    );



    // Enable interrupt on update events.

    CMSIS_SET(TIMx, DIER, UIE, true);
    NVIC_ENABLE(TIMx_update_event);

}



static useret b32
_STEPPER_push_delta(enum StepperHandle handle, i8 delta)
{

    _EXPAND_HANDLE

    b32 has_space = driver->writer - driver->reader < countof(driver->deltas);

    if (has_space)
    {
        driver->deltas[driver->writer % countof(driver->deltas)]  = delta;
        driver->writer                                           += 1;
    }

    return has_space;

}



static void
_STEPPER_driver_interrupt(enum StepperHandle handle)
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

        CMSIS_SET(TIMx, ARR, ARR, (STEPPER_PERIOD_US - 1) / (abs_steps ? abs_steps : 1));



        // Generate an update event so the above configuration
        // will be loaded into the timer's shadow registers.

        CMSIS_SET(TIMx, EGR, UG, true);



        // Enable the timer's counter; the counter will
        // become disabled after the end of step repetitions.

        CMSIS_SET(TIMx, CR1, CEN, true);

    }

}
