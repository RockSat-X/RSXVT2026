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



    enum BuzzerTune tune_to_play = BuzzerTune_birthday; // First song to immediately play.

    for (;;)
    {

        BUZZER_play(tune_to_play);

        stlink_tx("Press a key to play the next tune:\n");

        for (enum BuzzerTune tune = {0}; tune < BuzzerTune_COUNT; tune += 1)
        {
            stlink_tx("    '%c' -> %s", 'A' + tune, BUZZER_TUNES[tune].name);

            if (tune == tune_to_play)
            {
                stlink_tx(" (currently playing)");
            }

            stlink_tx("\n");
        }

        stlink_tx("\n");

        while (true)
        {

            u8 input_character = {0};
            while (!stlink_rx(&input_character));

            enum BuzzerTune desired_tune =
                'A' <= input_character && input_character <= 'Z' ? (u8) (input_character - 'A') :
                'a' <= input_character && input_character <= 'z' ? (u8) (input_character - 'a') : BuzzerTune_COUNT;

            if (desired_tune < BuzzerTune_COUNT)
            {
                tune_to_play = desired_tune;
                break;
            }
            else
            {
                stlink_tx("Invalid selection '%c'\n", input_character);
            }

        }

        stlink_tx("\n");

    }

}
