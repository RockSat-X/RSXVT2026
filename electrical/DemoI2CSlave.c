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

    I2C_reinit(I2CHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Test the I2C driver.
    //

    for
    (
        enum I2CAddressType address_type = I2CAddressType_seven;
        ;
        address_type = !address_type // Flip-flop between 7-bit and 10-bit addressing.
    )
    {

        // Get the address range.

        u32 start_address = {0};
        u32 end_address   = {0};

        switch (address_type)
        {
            case I2CAddressType_seven:
            {
                start_address = 0b0000'1000; // @/`I2C Slave Address`.
                end_address   = 0b0111'0111; // "
            } break;

            case I2CAddressType_ten:
            {
                start_address = 0;
                end_address   = (1 << 10) - 1;
            } break;

            default: panic;
        }



        // Test every valid 7-bit and 10-bit addresses to
        // see what slaves are connected to the I2C bus.

        for
        (
            u32 slave_address = start_address;
            slave_address <= end_address;
            slave_address += 1
        )
        {

            // We try to read a single byte from the slave at the
            // current slave address to see if we get an acknowledge.

            enum I2CDriverError error =
                I2C_blocking_transfer
                (
                    I2CHandle_primary,
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

            spinlock_nop(1'000'000);

        }



        // Longer breather before starting all over again.

        stlink_tx("--------------------------------\n");

        spinlock_nop(400'000'000);

    }

}
