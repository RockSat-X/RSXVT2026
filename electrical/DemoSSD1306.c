#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"



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
        enum I2CReinitResult result = I2C_reinit(I2CHandle_ssd1306);
        switch (result)
        {
            case I2CReinitResult_success : break;
            case I2CReinitResult_bug     : sorry
            default                      : sorry
        }
    }

    for (;;)
    {

        struct I2CDoJob job =
            {
                .handle       = I2CHandle_ssd1306,
                .address_type = I2CAddressType_seven,
                .address      = 0x3C,
                .writing      = false,
                .repeating    = false,
                .pointer      = &(u8) {0},
                .amount       = 1,
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
                    sorry
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

}
