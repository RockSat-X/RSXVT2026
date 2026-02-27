#include "system.h"
#include "uxart.c"
#include "buzzer.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize stuff.
    //



    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get the buzzer up and going.

    BUZZER_partial_init();



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate buzzer.
    //



    for (;;)
    {

        BUZZER_play(BuzzerTune_birthday);
        BUZZER_spinlock_to_completion();

    }

}
