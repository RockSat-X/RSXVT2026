#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

    for (i32 iteration = 0;; iteration += 1)
    {

        for (i32 index = 0; index < 8; index += 1)
        {

            GPIO_SET(led_channel_red  , (index >> 0) & 1);
            GPIO_SET(led_channel_green, (index >> 1) & 1);
            GPIO_SET(led_channel_blue , (index >> 2) & 1);

            stlink_tx("Color index: %d\n", index);

            char input = {0};
            if (stlink_rx(&input))
            {
                stlink_tx("Got '%c'!\n", input);
            }

            spinlock_nop(100'000'000);

        }

    }

}
