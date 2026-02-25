#include "timekeeping.meta"
/* #meta

    from deps.stpy.mcus import MCUS

    for target in PER_TARGET():



        # If the target needs timekeeping, then it needs to configure
        # a hardware timer to have a counter frequency of 1MHz in order
        # to achieve microsecond granularity.

        driver = [driver for driver in target.drivers if driver['type'] == 'TIMEKEEPING']

        if not driver:
            continue

        driver, = driver
        timer   = driver['peripheral']

        if target.schema.get(property := f'{timer}_COUNTER_RATE', None) != 1_000_000:
            raise ValueError(
                f'In order to have timekeeping '
                f'for target {repr(target.name)}, '
                f'there must be {repr(property)} '
                f'in the schema with value of {1_000_000}.'
            )



        # We require 32-bit timers because 2^32 microseconds is about 1.19 hours,
        # which is good enough for most applications.

        counter_width = MCUS[target.mcu][f'{timer}_COUNTER_WIDTH'].value

        if counter_width != 32:
            raise ValueError(
                f'In order to have timekeeping '
                f'for target {repr(target.name)}, '
                f'the timer must be 32-bit; '
                f'timer {repr(timer)} is {repr(counter_width)}-bit.'
            )



        # Some driver support definitions.

        peripheral, register, field = MCUS[target.mcu][f'{timer}_COUNTER'].location

        Meta.line(f'''
            static constexpr auto TIMEKEEPING_TIMER_ENABLE = {CMSIS_TUPLE(target.mcu, f'{timer}_ENABLE')};
            static constexpr auto TIMEKEEPING_DIVIDER      = STPY_{timer}_DIVIDER;
            #define TIMEKEEPING_microseconds() CMSIS_GET({peripheral}, {register}, {field})
            #define TIMEKEEPING_TIMER_         {timer}_
            #define TIMEKEEPING_TIMER          {timer}
        ''')

*/



static void
TIMEKEEPING_partial_init(void)
{

    // Enable the peripheral.

    CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



    // Trigger an update event so that the shadow registers
    // are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);

}



static void
spinlock_us(u32 duration_us)
{

    u32 start_us   = TIMEKEEPING_microseconds();
    u32 elapsed_us = 0;

    do
    {

        u32 present_us = TIMEKEEPING_microseconds();

        elapsed_us += present_us - start_us;
        start_us    = present_us;

    }
    while (elapsed_us < duration_us);

}
