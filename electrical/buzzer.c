struct BuzzerNote
{
    u16 modulation;
    u16 repetitions;
};

#include "BUZZER_TUNES.meta"
/* #meta

    import math



    # As of right now, the buzzer only works with TIM8 on the STM32H533xx
    # because it has the repetition feature that we need. Other timers
    # that also have the repetition counter could be used, but it's not
    # something I want to support right now.

    TIMER_COUNTER_RATE = 1_000_000
    TIMER_MAX_VALUE    = (1 << 16) - 1

    for target in PER_TARGET():

        if (
            target.schema is not None
            and 'TIM8_COUNTER_RATE' in target.schema
            and target.schema['TIM8_COUNTER_RATE'] == TIMER_COUNTER_RATE
        ):
            continue

        Meta.line(
            f'#error Target {repr(target.name)} needs to set {repr('TIM8_COUNTER_RATE')} '
            f'with value of {TIMER_COUNTER_RATE} in its schema.'
        )



    # Tunes are defined by a name and pairs of frequency (Hz) and durations (seconds).

    BUZZER_TUNES = (
        ('null', (
        )),
        ('ambulance', (
            (800, 0.5), (600, 0.5), (800, 0.5), (600, 0.5), (800, 0.5), (600, 0.5),
        )),
        ('up_and_down', (
            (400, 0.1), (450, 0.1), (500, 0.1), (550, 0.1), (600, 0.1), (650, 0.1), (700, 0.1), (750, 0.1), (800, 0.1),
            (750, 0.1), (700, 0.1), (650, 0.1), (600, 0.1), (550, 0.1), (500, 0.1), (450, 0.1), (400, 0.1),
        )),
        ('deep_beep', (
            (1000, 0.15), (0, 0.15), (1000, 0.15), (0, 0.15), (1000, 0.15), (0, 0.15), (1000, 0.15), (0, 0.15),
        )),
        ('heartbeat', (
            (1200, 0.1), (0, 0.1), (1200, 0.1), (0, 0.5), (1200, 0.1), (0, 0.1), (1200, 0.1), (0, 0.5),
        )),
        ('three_tone', (
            (700, 0.3), (900, 0.3), (1100, 0.3), (0, 0.5),
        )),
        ('tetris', (
            (659, 0.4), (494, 0.2), (523, 0.2), (587, 0.4), (523, 0.2), (494, 0.2),
            (440, 0.4), (440, 0.2), (523, 0.2), (659, 0.4), (587, 0.2), (523, 0.2),
            (494, 0.6), (523, 0.2), (587, 0.4), (659, 0.4), (523, 0.4), (440, 0.4),
            (440, 0.4), (  0, 0.2),
            (587, 0.4), (698, 0.2), (880, 0.4), (784, 0.2), (698, 0.2),
            (659, 0.6), (523, 0.2), (659, 0.4), (587, 0.2), (523, 0.2),
            (494, 0.4), (494, 0.2), (523, 0.2), (587, 0.4), (659, 0.4),
            (523, 0.4), (440, 0.4), (440, 0.4), (  0, 0.4),
        )),
        ('mario', (
            (659, 0.15), (659, 0.15), (  0, 0.15), (659, 0.15), (0, 0.15), (523, 0.15), (659, 0.15), (0, 0.15),
            (784, 0.15), (  0, 0.45), (392, 0.15), (  0, 0.45),
        )),
        ('birthday', (
            (262, 0.2), (262, 0.2), (294, 0.4), (262, 0.4), (349, 0.4), (330, 0.8),
            (262, 0.2), (262, 0.2), (294, 0.4), (262, 0.4), (392, 0.4), (349, 0.8),
            (262, 0.2), (262, 0.2), (523, 0.4), (440, 0.4), (349, 0.4), (330, 0.4), (294, 0.4),
            (466, 0.2), (466, 0.2), (440, 0.4), (349, 0.4), (392, 0.4), (349, 0.8),
        )),
        ('starwars', (
            (392, 0.5), (392, 0.5 ), (392, 0.5 ), (311, 0.35), (466, 0.15),
            (392, 0.5), (311, 0.35), (466, 0.15), (392, 1   ),
            (587, 0.5), (587, 0.5 ), (587, 0.5 ), (622, 0.35), (466, 0.15),
            (370, 0.5), (311, 0.35), (466, 0.15), (392, 1   ),
        )),
        ('nokia', (
            (659, 0.125), (587, 0.125), (370, 0.25), (415, 0.25),
            (554, 0.125), (494, 0.125), (294, 0.25), (330, 0.25),
            (494, 0.125), (440, 0.125), (277, 0.25), (330, 0.25), (440, 0.5),
        )),
    )



    # Enumeration so the user can specify the tune they'd like to play.

    Meta.enums('BuzzerTune', 'u32', (
        name
        for name, notes in BUZZER_TUNES
    ))



    # Define each of the variable-lengthed tunes.

    for name, notes in BUZZER_TUNES:

        with Meta.enter(f'static const struct BuzzerNote BUZZER_TUNE_{name}[] ='):

            for frequency, duration in notes:

                modulation  = max(min(round(TIMER_COUNTER_RATE / (frequency + 1)), TIMER_MAX_VALUE), 1)
                repetitions = math.ceil(duration / (modulation / TIMER_COUNTER_RATE)) - 1

                for repetition_part in (
                    [TIMER_MAX_VALUE] * (repetitions // TIMER_MAX_VALUE) +
                    [repetitions % TIMER_MAX_VALUE]
                ):

                    Meta.line(f'''
                        {{ {modulation}, {repetition_part} }},
                    ''')



    # Make table to point to all of the buzzer tunes.

    Meta.lut('BUZZER_TUNES', (
        (
            ('name'      , f'(const char*) "{name}"'                 ),
            ('note_array', f'(struct BuzzerNote*) BUZZER_TUNE_{name}'),
            ('note_count', len(notes)                                ),
        )
        for name, notes in BUZZER_TUNES
    ))

*/

struct BuzzerDriver
{
    volatile _Atomic enum BuzzerTune atomic_current_tune;
    volatile _Atomic enum BuzzerTune atomic_desired_tune;
    i32                              note_index;
};

static struct BuzzerDriver _BUZZER_driver = {0};



//////////////////////////////////////////////////////////////////////////////////////////



static void
BUZZER_partial_init(void)
{

    // Enable the peripheral.

    CMSIS_SET(RCC, APB2ENR, TIM8EN, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIM8, PSC, PSC, STPY_TIM8_DIVIDER);



    // Set the mode of the OC1 pin to be in "PWM mode 1".
    // @/pg 1551/sec 36.6.8/`H533RErm`.

    CMSIS_SET(TIM8, CCMR1, OC1M, 0b0110);



    CMSIS_SET
    (
        TIM8 , CCER,
        CC1E , true, // Enable the OC1 pin output.
        CC1NE, true, // Same thing for its complement.
    );



    // Master output enable.

    CMSIS_SET(TIM8, BDTR, MOE, true);



    CMSIS_SET
    (
        TIM8, CR1 ,
        URS , 0b1 , // So that the UG bit doesn't set the update event interrupt.
        OPM , true, // Timer's counter stops incrementing after an update event.
    );



    // Enable the update interrupt.

    CMSIS_SET(TIM8, DIER, UIE, true);
    NVIC_ENABLE(TIM8_UP);

}



static void
BUZZER_play(enum BuzzerTune tune)
{

    // To allow for restarted plays, we first clear the current tune.

    atomic_store_explicit
    (
        &_BUZZER_driver.atomic_desired_tune,
        BuzzerTune_null,
        memory_order_relaxed // No synchronization needed.
    );



    // Pend the interrupt handler so it'll stop the current tune.
    // Note that if the user is calling us from an interrupt handler
    // that's of higher priority, then the current tune won't actually
    // be properly cleared. This is bit of a subtle bug, but it's not
    // high importance since the buzzer should be used for minor diagnostics.

    NVIC_SET_PENDING(TIM8_UP);



    // Now we play the actual desired tune.

    if (tune)
    {

        atomic_store_explicit
        (
            &_BUZZER_driver.atomic_desired_tune,
            tune,
            memory_order_relaxed // No synchronization needed.
        );

        NVIC_SET_PENDING(TIM8_UP);

    }

}



static b32
BUZZER_current_tune(void)
{

    enum BuzzerTune observed_current_tune =
        atomic_load_explicit
        (
            &_BUZZER_driver.atomic_current_tune,
            memory_order_relaxed // No synchronization needed.
        );

    return observed_current_tune;

}



////////////////////////////////////////////////////////////////////////////////



INTERRUPT_TIM8_UP(void)
{

    // Acknowledge the update event, if there was one.

    CMSIS_SET(TIM8, SR, UIF, false);



    // Switch to the new tune, if there is one.

    {

        enum BuzzerTune observed_current_tune =
            atomic_load_explicit
            (
                &_BUZZER_driver.atomic_current_tune,
                memory_order_relaxed // No synchronization needed.
            );

        enum BuzzerTune observed_desired_tune =
            atomic_load_explicit
            (
                &_BUZZER_driver.atomic_desired_tune,
                memory_order_relaxed // No synchronization needed.
            );

        if (observed_current_tune != observed_desired_tune)
        {

            atomic_store_explicit
            (
                &_BUZZER_driver.atomic_current_tune,
                observed_desired_tune,
                memory_order_relaxed // No synchronization needed.
            );

            _BUZZER_driver.note_index = 0;

        }

    }



    // Process current tune.

    enum BuzzerTune observed_current_tune =
        atomic_load_explicit
        (
            &_BUZZER_driver.atomic_current_tune,
            memory_order_relaxed // No synchronization needed.
        );

    if (_BUZZER_driver.note_index == BUZZER_TUNES[observed_current_tune].note_count) // Reached end of tune?
    {

        atomic_store_explicit
        (
            &_BUZZER_driver.atomic_current_tune,
            BuzzerTune_null,     // Silence!
            memory_order_relaxed // No synchronization needed.
        );

        _BUZZER_driver.note_index = 0;

    }
    else if (!CMSIS_GET(TIM8, CR1, CEN)) // Counter turned itself off, thus we can play the next note.
    {

        struct BuzzerNote* note = &BUZZER_TUNES[observed_current_tune].note_array[_BUZZER_driver.note_index];

        CMSIS_SET(TIM8, ARR , ARR , note->modulation    ); // Modulate the timer's counter to get the desired frequency.
        CMSIS_SET(TIM8, CCR1, CCR1, note->modulation / 2); // Half of the modulation to get 50% duty cycle.
        CMSIS_SET(TIM8, RCR , REP , note->repetitions   ); // Repeat the cycle a certain amount of times to get the desired duration.
        CMSIS_SET(TIM8, EGR , UG  , true                ); // Update the shadow registers.
        CMSIS_SET(TIM8, CR1 , CEN , true                ); // Begin counting again.
        _BUZZER_driver.note_index += 1;                    // Move onto the next note.

    }

}
