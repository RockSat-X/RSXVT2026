#include "system.h"
#include "uxart.c"
#include "i2c.c"



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_vn100); // TODO Just to test VN-100 reception for VehicleFlightComputer.
    {
        enum I2CReinitResult result = I2C_reinit(I2CHandle_vehicle_interface);
        switch (result)
        {
            case I2CReinitResult_success : break;
            case I2CReinitResult_bug     : panic;
            default                      : panic;
        }
    }



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Handle vehicle interface.
//



FREERTOS_TASK(vehicle_interface, 1024, 0)
{
    for (;;)
    {

        struct VehicleInterfacePayload payload = {0};

        enum I2CTransferResult result =
            I2C_transfer
            (
                I2CHandle_vehicle_interface,
                VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS,
                I2CAddressType_seven,
                I2COperation_single_read,
                (u8*) &payload,
                sizeof(payload)
            );

        switch (result)
        {
            case I2CTransferResult_transfer_done:
            {

                static u16 previous_timestamp_us = 0;
                static u32 elapsed_timestamp_us  = 0;

                u8 digest = VEHICLE_INTERFACE_calculate_crc((u8*) &payload, sizeof(payload));

                if (!digest)
                {
                    elapsed_timestamp_us  += (u16) (payload.timestamp_us - previous_timestamp_us);
                    previous_timestamp_us  = payload.timestamp_us;
                }

                stlink_tx
                (
                    "%6u ms : 0x%02X : (0x%02X)\n",
                    elapsed_timestamp_us / 1000,
                    payload.flags,
                    digest
                );

            } break;

            case I2CTransferResult_no_acknowledge:
            {
                stlink_tx("Slave 0x%03X didn't acknowledge!\n", VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS);
            } break;

            case I2CTransferResult_transfer_ongoing      : panic;
            case I2CTransferResult_clock_stretch_timeout : panic;
            case I2CTransferResult_bug                   : panic;
            default                                      : panic;
        }

        spinlock_nop(10'000'000);

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Just to test VN-100 reception for VehicleFlightComputer.
//



FREERTOS_TASK(vn100_uart, 1024, 0)
{
    for (;;)
    {

        char* payloads[] =
            {
                "$VNRRG,15,-0.094586,+0.226808,+0.325434,+0.913074,-00.0340,-00.2107,+00.8741,+00.371,+00.285,-09.578,+00.000666,-00.003642,-00.000202*64",
            };

        for (i32 i = 0; i < countof(payloads); i += 1)
        {
            UXART_tx_bytes(UXARTHandle_vn100, (u8*) payloads[i], (i32) strlen(payloads[i]));
        }

        spinlock_nop(10'000'000);

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Just an LED heartbeat.
//



FREERTOS_TASK(heartbeat, 1024, 0)
{
    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(50'000'000);
    }
}
