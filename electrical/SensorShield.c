#include "system.h"
#include "uxart.c"
#include "i2c.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialization.
    //

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    I2C_reinit(I2CHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////


    u8 addresses[] =
        {
            0x19,
            0x1E,
            0x38,
            0x5D,
            0x6A,
            0x6B,
        };
    while(true) {
        for (i32 address_i = 0; address_i < countof(addresses); address_i += 1)
        {

            // We try to read a single byte from the slave at the
            // current slave address to see if we get an acknowledge.

            enum I2CMasterError error =
                I2C_blocking_transfer
                (
                    I2CHandle_primary,
                    addresses[address_i],
                    I2CAddressType_seven,
                    I2COperation_read,
                    &(u8) {0},
                    1
                );



            // Check the results of the transfer.

            switch (error)
            {
                case I2CMasterError_none:
                {
                    stlink_tx("Slave 0x%03X acknowledged!\n", addresses[address_i]);
                } break;

                case I2CMasterError_no_acknowledge:
                {
                    stlink_tx("Slave 0x%03X didn't acknowledge!\n", addresses[address_i]);
                } break;

                default: panic;
            }



            // Bit of breather...

            GPIO_TOGGLE(led_green);

            spinlock_nop(1'000'000); // TODO Let's use a real delay here.

        }
    }

}
