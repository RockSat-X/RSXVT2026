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

        struct LIS2MDLMeasurement measurement = {0};

        if (LIS2MDL_pop_measurement(&measurement))
        {
            stlink_tx
            (
                "status      : " "0x%02X" "\n"
                "x           : " "%f"     "\n"
                "y           : " "%f"     "\n"
                "z           : " "%f"     "\n"
                "temperature : " "%d"     "\n"
                "\n",
                measurement.status,
                measurement.x / 32768.0f,
                measurement.y / 32768.0f,
                measurement.z / 32768.0f,
                measurement.temperature
            );
        }

        GPIO_TOGGLE(led_green);

    }

}
