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

    {
        enum SSD1306ReinitResult result = SSD1306_reinit();
        switch (result)
        {
            case SSD1306ReinitResult_success                       : break;
            case SSD1306ReinitResult_failed_to_initialize_with_i2c : sorry
            case SSD1306ReinitResult_bug                           : sorry
            default                                                : sorry
        }
    }



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate the SSD1306 driver.
    //



    i32 held    = 0;
    u8  text[8] = {0};
    memset(text, 0xFF, sizeof(text));

    for (i32 j = 0;; j += 1)
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
            "C?    %s"   "\n",
            TIMEKEEPING_microseconds() / 1000.0 / 1000.0,
            held,
            text,
            text[0] == 'A' ? "OK" : "nil",
            held           ? "OK" : "nil",
            held % 4       ? "OK" : "nil"
        );



        // Swap framebuffer.

        enum SSD1306RefreshResult result = SSD1306_refresh();

        switch (result)
        {
            case SSD1306RefreshResult_success            : break;
            case SSD1306RefreshResult_i2c_transfer_error : sorry
            case SSD1306RefreshResult_bug                : sorry
            default                                      : sorry
        }

        GPIO_TOGGLE(led_green);

    }

}
