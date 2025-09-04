#include "defs.h"
#include "jig.c"
#include "i2c.c"

extern noret void
main(void)
{

    SYSTEM_init();
    JIG_init();



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



    I2C_reinit(I2CHandle_1);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Test the I2C.
    //

    {
        u8 buffer[] = { 0x30, 0x08 };

        enum I2CDriverError result = I2C_blocking_transfer(I2CHandle_1, 0x78, false, buffer, countof(buffer));

        switch (result)
        {
            case I2CDriverError_none:
            {
                // Transfer was successful!
            } break;

            case I2CDriverError_no_acknowledge:
            {
                // Uh oh!
            } break;

            default: panic;
        }
    }
    {
        u8 buffer[] = { 0x00, 0x00 };

        enum I2CDriverError result = I2C_blocking_transfer(I2CHandle_1, 0x78, true, buffer, countof(buffer));

        switch (result)
        {
            case I2CDriverError_none:
            {
                // Transfer was successful!
            } break;

            case I2CDriverError_no_acknowledge:
            {
                // Uh oh!
            } break;

            default: panic;
        }

        for (i32 i = 0; i < countof(buffer); i += 1)
        {
            JIG_tx("%d : 0x%02X\n", i, buffer[i]);
        }
    }



    ////////////////////////////////////////////////////////////////////////////////



    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(40'000'000);
    }

}
