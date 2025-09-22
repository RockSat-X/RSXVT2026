#include "system.h"
#include "uxart.c"
#include "spi.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Miscellaneous initialization.
    //

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize the SPI driver for each peripheral used by the target.
    //

    SPI_reinit(SPIHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////

    for (;;)
    {

        enum SPIDriverError error =
            SPI_blocking_transfer
            (
                SPIHandle_primary,
                (u8[]) { 0xDE, 0xAD, 0xBE, 0xEF },
                (u8[4]) {0},
                4
            );

        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);

    }

}
