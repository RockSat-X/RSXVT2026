#include "defs.h"
#include "uxart.c"
#include "i2c.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Miscellaneous initialization.
    //

    SYSTEM_init();
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

    spinlock_nop(100'000'000); // TODO Let's use a real delay here.



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize the I2C driver for each peripheral used by the target.
    //

    I2C_reinit(I2CHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Test the I2C driver.
    //

    for (;;)
    {

        // Test every valid 7-bit address to see
        // what slaves are connected to the I2C bus.

        for
        (
            i32 slave_address = 0b0001'000'0; // @/`I2C Slave Address`.
            slave_address <= 0b1110'111'0;    // "
            slave_address += 2                // "
        )
        {

            // We try to read a single byte from the slave at the
            // current slave address to see if we get an acknowledge.

            enum I2CDriverError error =
                I2C_blocking_transfer
                (
                    I2CHandle_primary,
                    slave_address,
                    I2COperation_read,
                    &(u8) {0},
                    1
                );



            // Check the results of the transfer.

            switch (error)
            {
                case I2CDriverError_none:
                {
                    stlink_tx("Slave 0x%02X acknowledged!\n", slave_address);
                } break;

                case I2CDriverError_no_acknowledge:
                {
                    stlink_tx("Slave 0x%02X didn't acknowledge!\n", slave_address);
                } break;

                default: panic;
            }



            // Bit of breather...

            GPIO_TOGGLE(led_green);

            spinlock_nop(10'000'000); // TODO Let's use a real delay here.

        }



        // Longer breather before starting all over again.

        stlink_tx("--------------------------------\n");

        spinlock_nop(400'000'000); // TODO Let's use a real delay here.

    }

}
