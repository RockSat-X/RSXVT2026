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

    enum I2CBlockingTransfer result = I2C_blocking_transfer(I2CHandle_1, 0x78, true, 5);
    switch (result)
    {
        case I2CBlockingTransfer_done:
        {
            // Transfer was successful!
        } break;

        default: panic;
    }

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000);
    }

}
