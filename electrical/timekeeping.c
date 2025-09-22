#include "timekeeping.meta"
/* #meta

    from deps.stpy.mcus import MCUS

    for target in PER_TARGET():



        # If the target needs timekeeping, then it needs to configure
        # a hardware timer to have a counter frequency of 1MHz in order
        # to achieve microsecond granularity.

        timer = target.drivers.get('TIMEKEEPING', None)

        if timer is None:
            continue

        if target.schema.get(property := f'{timer}_COUNTER_RATE', None) != 1_000_000:
            raise ValueError(
                f'In order to have timekeeping '
                f'for target {repr(target.name)}, '
                f'there must be {repr(property)} '
                f'in the schema with value of {1_000_000}.'
            )



        # Some driver support definitions.

        peripheral, register, field = MCUS[target.mcu][f'{timer}_COUNTER'      ].location
        counter_width               = MCUS[target.mcu][f'{timer}_COUNTER_WIDTH'].value;

        Meta.line(f'''
            static constexpr auto TIMEKEEPING_TIMER_ENABLE = {CMSIS_TUPLE(target.mcu, f'{timer}_ENABLE')};
            static constexpr auto TIMEKEEPING_DIVIDER      = STPY_{timer}_DIVIDER;
            typedef u{counter_width} TIMEKEEPING_COUNTER_TYPE;
            #define TIMEKEEPING_COUNTER() CMSIS_GET({peripheral}, {register}, {field})
            #define TIMEKEEPING_TIMER_    {timer}_
            #define TIMEKEEPING_TIMER     {timer}
        ''')

*/



static void
spinlock_us(i32 microseconds)
{

    u32 past_us    = TIMEKEEPING_COUNTER();
    i32 elapsed_us = {0};

    do
    {

        u32 present_us = TIMEKEEPING_COUNTER();

        // Timer counters are 16-bit or 32-bit. When it's 16-bit
        // and we perform a subtraction, this typically results
        // in a modular difference between the two numbers. However,
        // when an overflow happens and `present_us` is now smaller than
        // `past_us`, this will result in a negative number. In 16-bit
        // modular arithmetic, this doesn't actually change anything,
        // but things are implicitly upcasted to 32-bit, and this does
        // break things. So to fix that edge case, we perform the
        // appropriate cast.
        elapsed_us += (TIMEKEEPING_COUNTER_TYPE) (present_us - past_us);

        past_us = present_us;

    }
    while (elapsed_us < microseconds);

}
