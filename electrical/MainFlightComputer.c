#include "system.h"
#include "timekeeping.c"
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
    UXART_reinit(UXARTHandle_stlink);
    UXART_reinit(UXARTHandle_vn100); // TODO Just to test VN-100 reception for VehicleFlightComputer.

    {

        // Set the prescaler that'll affect all timers' kernel frequency.

        CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



        // Configure the other registers to get timekeeping up and going.

        TIMEKEEPING_partial_init();

    }

    {
        enum I2CReinitResult result = I2C_reinit(I2CHandle_vehicle_interface);
        switch (result)
        {
            case I2CReinitResult_success : break;
            case I2CReinitResult_bug     : sorry
            default                      : sorry
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

        struct I2CDoJob job =
            {
                .handle       = I2CHandle_vehicle_interface,
                .address_type = I2CAddressType_seven,
                .address      = VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS,
                .writing      = false,
                .repeating    = false,
                .pointer      = (u8*) &payload,
                .amount       = sizeof(payload),
            };

        enum I2CDoResult transfer_result = {0};
        do transfer_result = I2C_do(&job); // TODO Clean up.
        while (transfer_result == I2CDoResult_working);

        switch (transfer_result)
        {
            case I2CDoResult_success:
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

            case I2CDoResult_no_acknowledge:
            {
                stlink_tx("Slave 0x%03X didn't acknowledge!\n", VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS);
            } break;

            case I2CDoResult_bus_misbehaved : sorry // TODO.
            case I2CDoResult_watchdog_expired : sorry // TODO.

            case I2CDoResult_working               : sorry
            case I2CDoResult_clock_stretch_timeout : sorry
            case I2CDoResult_bug                   : sorry
            default                                : sorry
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
