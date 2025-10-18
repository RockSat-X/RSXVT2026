#include "system.h"
#include "uxart.c"
#include "i2c.c"



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
    // The I2C protocol defines the data and clock lines to be idle-high, but when
    // the MCU is undergoing a reset, the pins will be sunk low (unless pulled up
    // by an external pull-up resistor). So to make oscilloscope measurements
    // easier, we add a delay before we initialize the I2C driver to clearly
    // differentiate between when the MCU is resetted and when the I2C driver
    // starts working.
    //

    spinlock_nop(100'000'000);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize the I2C driver for each peripheral used by the target.
    //

    I2C_reinit(I2CHandle_queen);
    I2C_reinit(I2CHandle_bee  );



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Test the I2C driver.
    //

    for (;;)
    {

        enum I2CAddressType address_type  = I2CAddressType_seven;
        u32                 slave_address = TMP_SLAVE_ADDRESS;

        enum I2CDriverError error =
            I2C_blocking_transfer
            (
                I2CHandle_queen,
                slave_address,
                address_type,
                I2COperation_read,
                &(u8) {0},
                1
            );



        // Check the results of the transfer.

        switch (error)
        {
            case I2CDriverError_none:
            {
                stlink_tx("Slave 0x%03X acknowledged!\n", slave_address);
            } break;

            case I2CDriverError_no_acknowledge:
            {
                stlink_tx("Slave 0x%03X didn't acknowledge!\n", slave_address);
            } break;

            default: panic;
        }



        // Bit of breather...

        GPIO_TOGGLE(led_green);

        spinlock_nop(40'000'000);

    }

}
