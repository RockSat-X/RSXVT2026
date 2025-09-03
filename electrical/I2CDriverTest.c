#include "defs.h"
#include "jig.c"
#include "i2c.c"

extern noret void
main(void)
{

    SYSTEM_init();
    JIG_init();






    GPIO_HIGH(ov_pd); // Pulling high so the camera is powered down.
    GPIO_LOW(ov_rt);  // Pulling low so the camera is resetting.

    spinlock_nop(100'000'000);

    GPIO_LOW(ov_pd);  // Pulling low so the camera is now powered on.
    GPIO_HIGH(ov_rt); // Pulling high so the camera is now out of reset.



    // A delay here so if we inspect the I2C pins,
    // it's clearly differentiated from when the MCU
    // is under reset.

    spinlock_nop(100'000'000);



    // Set up the I2C.

    I2C_reinit(I2CHandle_1); // TMP.



    // TODO Test the I2C.

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


    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(40'000'000);
    }

}
