#include "defs.h"
#include "jig.c"
#include "i2c.c"

extern noret void
main(void)
{

    SYSTEM_init();
    JIG_init();



    // A delay here so if we inspect the I2C pins,
    // it's clearly differentiated from when the MCU
    // is under reset.

    spinlock_nop(100'000'000);



    // Set up the I2C.

    I2C_reinit(I2CHandle_1); // TMP.



    // TODO Test the I2C.

    {
        u8 buffer[] = { 0x30, 0x08 };

        enum I2CBlockingTransfer result = I2C_blocking_transfer(I2CHandle_1, 0x78, false, buffer, countof(buffer));

        switch (result)
        {
            case I2CBlockingTransfer_done:
            {
                // Transfer was successful!
            } break;

            case I2CBlockingTransfer_no_acknowledge:
            {
                // Uh oh!
            } break;

            default: panic;
        }
    }
    {
        u8 buffer[] = { 0x00, 0x00 };

        enum I2CBlockingTransfer result = I2C_blocking_transfer(I2CHandle_1, 0x78, true, buffer, countof(buffer));

        switch (result)
        {
            case I2CBlockingTransfer_done:
            {
                // Transfer was successful!
            } break;

            case I2CBlockingTransfer_no_acknowledge:
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


    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000);
    }

}
