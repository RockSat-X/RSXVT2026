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

        {

            u8 TMP[] =
                {
                    (0 << 6),  // Interpret the following bytes as commands. @/pg 20/sec 8.1.5.2/`SSD1306`.
                    0xD3, 0x00, // "Set Display Offset"; resets the draw pointer to beginning of display.
                    0x00,
                    0x10,
                    0xB0,
                };

            struct I2CDoJob job =
                {
                    .handle       = I2CHandle_ssd1306, // TODO Coupled.
                    .address_type = I2CAddressType_seven,
                    .address      = SSD1306_SEVEN_BIT_ADDRESS,
                    .writing      = true,
                    .repeating    = false,
                    .pointer      = (u8*) TMP,
                    .amount       = countof(TMP),
                };

            for (b32 yield = false; !yield;)
            {
                enum I2CDoResult result = I2C_do(&job);

                switch (result)
                {

                    case I2CDoResult_working:
                    {
                        // Transfer busy...
                    } break;

                    case I2CDoResult_success:
                    {
                        yield = true;
                    } break;

                    case I2CDoResult_no_acknowledge        : sorry
                    case I2CDoResult_clock_stretch_timeout : sorry
                    case I2CDoResult_bus_misbehaved        : sorry
                    case I2CDoResult_watchdog_expired      : sorry
                    case I2CDoResult_bug                   : sorry
                    default                                : sorry

                }

            }

        }

        ((u8*) _SSD1306_framebuffer)[j % (SSD1306_ROWS * SSD1306_COLUMNS / bitsof(u8))] ^= 0xFF;

        SSD1306_write_format
        (
            j % 500 / 2 - 100,
            j / 4 % 12 - 2,
            "meow! %d",
            j
        );

        struct I2CDoJob job =
            {
                .handle       = I2CHandle_ssd1306, // TODO Coupled.
                .address_type = I2CAddressType_seven,
                .address      = SSD1306_SEVEN_BIT_ADDRESS,
                .writing      = true,
                .repeating    = false,
                .pointer      = _SSD1306_data_payload,
                .amount       = sizeof(_SSD1306_data_payload),
            };

        for (b32 yield = false; !yield;)
        {
            enum I2CDoResult result = I2C_do(&job);

            switch (result)
            {

                case I2CDoResult_working:
                {
                    // Transfer busy...
                } break;

                case I2CDoResult_success:
                {
                    yield = true;
                } break;

                case I2CDoResult_no_acknowledge        : sorry
                case I2CDoResult_clock_stretch_timeout : sorry
                case I2CDoResult_bus_misbehaved        : sorry
                case I2CDoResult_watchdog_expired      : sorry
                case I2CDoResult_bug                   : sorry
                default                                : sorry

            }

        }

        GPIO_TOGGLE(led_green);
    }

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
