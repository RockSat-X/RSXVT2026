#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "buzzer.c"
#include "ssd1306.c"



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // Initialize the SSD1306 display.

    for (b32 yield = false; !yield;)
    {

        enum SSD1306ReinitResult result = SSD1306_reinit();

        switch (result)
        {

            case SSD1306ReinitResult_success:
            {
                yield = true;
            } break;

            case SSD1306ReinitResult_failed_to_initialize_with_i2c:
            {
                // Something went wrong; we'll try again.
            } break;

            case SSD1306ReinitResult_bug : sorry
            default                      : sorry

        }

    }



    // Configure the other registers to get the buzzer up and going.

    BUZZER_partial_init();
    BUZZER_play(BuzzerTune_nokia);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Render stuff to the screen.
//



FREERTOS_TASK(display, 1024, 0)
{

    // Do some rendering.

    b32 flashing = false;
    i32 held     = 0;
    u8  text[8]  = {0};
    memset(text, 0xFF, sizeof(text));

    while (true)
    {



        // Read inputs.

        u8 input = {0};
        if (stlink_rx(&input))
        {

            held += 1;

            memmove(text, text + 1, sizeof(text) - 1);
            text[countof(text) - 1] = input;

            if (input == ' ')
            {
                held = 0;
            }

            if (input == 'F')
            {
                flashing = true;
            }
            else if (input == 'f')
            {
                flashing = false;
            }

        }



        // Update framebuffer.

        memset(SSD1306_framebuffer, 0, sizeof(*SSD1306_framebuffer));

        SSD1306_write_format
        (
            0,
            0,
            "Clock %.2f" "\n"
            "Held  %d"   "\n"
            "Text  %s"   "\n"
            "A?    %s"   "\n"
            "B?    %s"   "\n"
            "C?    %s"   "\n"
            "Flash %s"   "\n",
            TIMEKEEPING_microseconds() / 1000.0 / 1000.0,
            held,
            text,
            text[0] == 'A' ? "OK" : "nope",
            held           ? "OK" : "nope",
            held / 16 % 2  ? "OK" : "nope",
            flashing ? "True" : "False"
        );



        // Swap framebuffer.

        enum SSD1306RefreshResult result =
            SSD1306_refresh
            (
                TIMEKEEPING_microseconds() / 1000 / 1000 / 2 % 2,
                !!flashing
            );

        switch (result)
        {
            case SSD1306RefreshResult_success            : break;
            case SSD1306RefreshResult_i2c_transfer_error : sorry
            case SSD1306RefreshResult_bug                : sorry
            default                                      : sorry
        }

        GPIO_INACTIVE(led_channel_red_A  );
        GPIO_TOGGLE  (led_channel_green_A);
        GPIO_INACTIVE(led_channel_blue_A );

        GPIO_INACTIVE(led_channel_red_B  );
        GPIO_TOGGLE  (led_channel_green_B);
        GPIO_INACTIVE(led_channel_blue_B );

        GPIO_INACTIVE(led_channel_red_C  );
        GPIO_TOGGLE  (led_channel_green_C);
        GPIO_INACTIVE(led_channel_blue_C );

        GPIO_INACTIVE(led_channel_red_D  );
        GPIO_TOGGLE  (led_channel_green_D);
        GPIO_INACTIVE(led_channel_blue_D );

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Check if there's enough errors to the point where we
//      should induce a reset to see if it'll fix anything.
//



FREERTOS_TASK(kicker, 256, 0)
{
    for (;;)
    {
        WATCHDOG_KICK();
    }
}
