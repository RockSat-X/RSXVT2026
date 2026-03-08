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

    I2C_partial_reinit(I2CHandle_vehicle_interface);



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

        #include "VN100_DATA_LOGS.meta"
        /* #meta

            import deps.stpy.pxd.pxd as pxd

            with Meta.enter('static const char* const VN100_DATA_LOGS[] ='):
                for line in pxd.make_main_relative_path('./gnc/vn100_scripts/vn100_dataLog.txt').read_text().splitlines():
                    Meta.line(f'''
                        "{line}",
                    ''')

        */

        for (i32 i = 0; i < countof(VN100_DATA_LOGS); i += 1)
        {
            UXART_tx_bytes(UXARTHandle_vn100, (u8*) VN100_DATA_LOGS[i], (i32) strlen(VN100_DATA_LOGS[i]));
            FREERTOS_delay_ms(20);
        }

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
