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
    //
    // Test the SPI driver.
    //

    for (i32 iteration = 0;; iteration += 1)
    {

        // Do a transfer.

        u8 transmission[] =
            {
                ((u8*) &iteration)[3],
                ((u8*) &iteration)[2],
                ((u8*) &iteration)[1],
                ((u8*) &iteration)[0],
            };

        u8 reception[sizeof(transmission)] = {0};

        enum SPIDriverError error =
            SPI_blocking_transfer
            (
                SPIHandle_primary,
                transmission,
                reception,
                sizeof(iteration)
            );



        // Check for any errors.

        switch (error)
        {

            case SPIDriverError_none:
            {
                // No issues.
            } break;

            default: panic;

        }



        // Output what we received.

        stlink_tx("%d :", iteration);

        for (i32 i = 0; i < sizeof(iteration); i += 1)
        {
            stlink_tx(" 0x%02X", reception[i]);
        }

        stlink_tx("\n");



        // Bit of breather...

        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000); // TODO Let's use a real delay here...

    }

}
