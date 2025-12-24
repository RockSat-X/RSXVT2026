#include "system.h"
#include "uxart.c"
#include "i2c.c"
#include "lis2mdl.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    I2C_reinit(I2CHandle_primary);
    LIS2MDL_init();



    for (;;)
    {

        for (i32 register_i = 0; register_i < countof(LIS2MDL_REGISTERS); register_i += 1)
        {

            u8 data = {0};
            {

                enum I2CMasterError error =
                    I2C_blocking_transfer
                    (
                        I2CHandle_primary,
                        LIS2MDL_SEVEN_BIT_ADDRESS,
                        I2CAddressType_seven,
                        I2COperation_write,
                        &(u8) { LIS2MDL_REGISTERS[register_i].address },
                        1
                    );

                if (error)
                    sorry

            }
            {

                enum I2CMasterError error =
                    I2C_blocking_transfer
                    (
                        I2CHandle_primary,
                        LIS2MDL_SEVEN_BIT_ADDRESS,
                        I2CAddressType_seven,
                        I2COperation_read,
                        &data,
                        sizeof(data)
                    );

                if (error)
                    sorry

            }

            stlink_tx
            (
                "Register %-14s (0x%02X) : 0x%02X\n",
                LIS2MDL_REGISTERS[register_i].name,
                LIS2MDL_REGISTERS[register_i].address,
                data
            );

        }

        stlink_tx("\n");

        GPIO_TOGGLE(led_green);
        spinlock_nop(400'000'000);

    }

}
