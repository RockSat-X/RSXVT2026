#include "system.h"
#include "uxart.c"
#include "i2c.c"
#include "lis2mdl.c"



static volatile b32 lis2mdl_data_ready = false;

INTERRUPT_EXTI1
{
    CMSIS_SET(EXTI, RPR1, RPIF1, true);
    lis2mdl_data_ready = true;
}



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

    I2C_reinit(I2CHandle_primary);

    CMSIS_SET(EXTI, RTSR1 , RT1 , true);
    CMSIS_SET(EXTI, SWIER1, SWI1, true);
    CMSIS_SET(EXTI, IMR1  , IM1 , true);
    CMSIS_WRITE(EXTI_EXTICR1, EXTI->EXTICR[0], EXTI1, 0x01);

    NVIC_ENABLE(EXTI1);

    LIS2MDL_init();



    for (;;)
    {

        if (lis2mdl_data_ready)
        {
            lis2mdl_data_ready = false;

            GPIO_HIGH(debug);
            struct LIS2MDLPayload payload = LIS2MDL_get_payload();
            GPIO_LOW(debug);

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
        }

        GPIO_TOGGLE(led_green);

    }

}
