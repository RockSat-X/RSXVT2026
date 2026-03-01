#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "ssd1306.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize stuff.
    //



    STPY_init();
    UXART_reinit(UXARTHandle_stlink);

    {

        // Set the prescaler that'll affect all timers' kernel frequency.

        CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



        // Configure the other registers to get timekeeping up and going.

        TIMEKEEPING_partial_init();

    }



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate the SSD1306 driver.
    //



    while (true)
    {



        // Initialize the SSD1306 display.

        for (b32 yield = false; !yield;)
        {

            stlink_tx("Initializing...\n");

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



        // Do some rendering.

        b32 flashing = false;
        i32 held     = 0;
        u8  text[8]  = {0};
        memset(text, 0xFF, sizeof(text));

        while (true)
        {



            // Read inputs.

            if (GPIO_READ(button))
            {
                held += 1;
            }

            u8 input = {0};
            if (stlink_rx(&input))
            {

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
                case SSD1306RefreshResult_i2c_transfer_error : goto ERROR;
                case SSD1306RefreshResult_bug                : sorry
                default                                      : sorry
            }

            GPIO_TOGGLE(led_green);

        }
        ERROR:;

    }

}
