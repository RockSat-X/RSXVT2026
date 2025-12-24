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

        struct LIS2MDLPayload payload = LIS2MDL_get_payload();

        stlink_tx
        (
            "status      : " "0x%02X" "\n"
            "x           : " "%f"     "\n"
            "y           : " "%f"     "\n"
            "z           : " "%f"     "\n"
            "temperature : " "%d"     "\n",
            payload.status,
            payload.x / 32768.0f,
            payload.y / 32768.0f,
            payload.z / 32768.0f,
            payload.temperature
        );

        stlink_tx("\n");

        GPIO_TOGGLE(led_green);
        spinlock_nop(10'000'000);

    }

}
