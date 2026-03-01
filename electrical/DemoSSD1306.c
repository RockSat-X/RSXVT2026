#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "ssd1306.c"



extern noret void
main(void)
{

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

    for (i32 j = 0;; j += 1)
    {

        ((u8*) _SSD1306_framebuffer)[j % (SSD1306_ROWS * SSD1306_COLUMNS / bitsof(u8))] ^= 0xFF;

        SSD1306_write_format
        (
            j % 500 / 2 - 100,
            j / 4 % 12 - 2,
            "meow! %d",
            j
        );

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

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
