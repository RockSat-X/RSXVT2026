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
    // Initialize the SPI drivers used by the target.
    //

    SPI_reinit(SPIHandle_primary);
    SPI_reinit(SPIHandle_secondary);



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

        enum SPIDriverMasterError error =
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

            case SPIDriverMasterError_none:
            {
                // No issues.
            } break;

            default: panic;

        }



        // Output what the master received.

        stlink_tx("%d :", iteration);

        for (i32 i = 0; i < sizeof(iteration); i += 1)
        {
            stlink_tx(" 0x%02X", reception[i]);
        }

        stlink_tx("\n");



        // Output what the slave-receiver got.

        {
            u8 data = {0};
            while (SPI_receive_byte(SPIHandle_secondary, &data))
            {
                stlink_tx("Got : 0x%02X\n", data);
            }
            stlink_tx("\n");
        }



        // Bit of breather...

        GPIO_TOGGLE(led_green);
        spinlock_nop(40'000'000);

    }

}
