#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



static enum ESP32FindStartTokenResult : u32
{
    ESP32FindStartTokenResult_esp32,
    ESP32FindStartTokenResult_lora,
}
esp32_find_start_token(void)
{

    i32 esp32_match_length = 0;
    i32 lora_match_length  = 0;

    while (true)
    {

        u8 data = {0};
        while (!UXART_rx(UXARTHandle_esp32, &data));

        if (ESP32_TOKEN_START[esp32_match_length] != data) // Reset the search for the ESP32 token?
        {
            esp32_match_length = 0;
        }

        if (ESP32_TOKEN_START[esp32_match_length] == data)
        {

            esp32_match_length += 1;

            if (ESP32_TOKEN_START[esp32_match_length] == '\0')
            {
                return ESP32FindStartTokenResult_esp32;
            }

        }

        if (LORA_TOKEN_START[lora_match_length] != data) // Reset the search for the LoRa token?
        {
            lora_match_length = 0;
        }

        if (LORA_TOKEN_START[lora_match_length] == data)
        {

            lora_match_length += 1;

            if (LORA_TOKEN_START[lora_match_length] == '\0')
            {
                return ESP32FindStartTokenResult_lora;
            }

        }

    }
}

extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);
    UXART_reinit(UXARTHandle_vn100); // TODO Just to test VN-100 reception for VehicleFlightComputer.
    UXART_reinit(UXARTHandle_esp32);

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



FREERTOS_TASK(vehicle_interface, 0)
{
    for (;;)
    {

#if 1

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
                    "[%u ms] (0x%02X)"    "\n"
                    "stepper_issues : %d" "\n"
                    "vn100_issues   : %d" "\n"
                    "openmv_issues  : %d" "\n"
                    "esp32_issues   : %d" "\n"
                    "\n",
                    elapsed_timestamp_us / 1000,
                    digest,
                    payload.stepper_issues,
                    payload.vn100_issues,
                    payload.openmv_issues,
                    payload.esp32_issues
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

#endif

        FREERTOS_delay_ms(10);

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Just to test VN-100 reception for VehicleFlightComputer.
//



FREERTOS_TASK(vn100_uart, 0)
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


            if (i % 4 == 0)
            {
                u8 byte = {0};
                if (UXART_rx(UXARTHandle_vn100, &byte))
                {
                    FREERTOS_delay_ms(1);
                    do
                    {
                        UXART_tx_bytes(UXARTHandle_vn100, &byte, 1);
                    }
                    while (UXART_rx(UXARTHandle_vn100, &byte));
                }
            }

        }

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Just an LED heartbeat.
//



FREERTOS_TASK(heartbeat, 0)
{
    for (;;)
    {
        GPIO_TOGGLE(led_green);
        FREERTOS_delay_ms(500);
    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(esp32, 0)
{
    for (;;)
    {

        enum ESP32FindStartTokenResult result = esp32_find_start_token();

        switch (result)
        {

            case ESP32FindStartTokenResult_esp32:
            {

                struct ESP32Packet packet = {0};

                for (i32 i = 0; i < sizeof(packet); i += 1)
                {
                    u8 data = {0};
                    while (!UXART_rx(UXARTHandle_esp32, &data))
                    {
                        FREERTOS_delay_ms(1);
                    }
                    ((u8*) &packet)[i] = data;
                }

                u8 digest = ESP32_calculate_crc((u8*) &packet, sizeof(packet));

                if (digest)
                {
                    stlink_tx(">>>> CRC mismatch! Maybe due to UART RX-FIFO overflow... <<<<\n\n");
                }
                else
                {

                    #if 0
                    {
                        stlink_tx
                        (
                            "QuatX                      : %f"    "\n"
                            "QuatY                      : %f"    "\n"
                            "QuatZ                      : %f"    "\n"
                            "QuatS                      : %f"    "\n"
                            "MagX                       : %f"    "\n"
                            "MagY                       : %f"    "\n"
                            "MagZ                       : %f"    "\n"
                            "AccelX                     : %f"    "\n"
                            "AccelY                     : %f"    "\n"
                            "AccelZ                     : %f"    "\n"
                            "GyroX                      : %f"    "\n"
                            "GyroY                      : %f"    "\n"
                            "GyroZ                      : %f"    "\n"
                            "timestamp_ms               : %u ms" "\n"
                            "rolling_sequence_number    : %u"    "\n"
                            "computer_vision_confidence : %u"    "\n"
                            "image_sequence_number      : %u"    "\n"
                            "\n",
                            packet.nonredundant.QuatX,
                            packet.nonredundant.QuatY,
                            packet.nonredundant.QuatZ,
                            packet.nonredundant.QuatS,
                            packet.MagX,
                            packet.MagY,
                            packet.MagZ,
                            packet.nonredundant.AccelX,
                            packet.nonredundant.AccelY,
                            packet.nonredundant.AccelZ,
                            packet.nonredundant.GyroX,
                            packet.nonredundant.GyroY,
                            packet.nonredundant.GyroZ,
                            packet.nonredundant.timestamp_ms,
                            packet.nonredundant.rolling_sequence_number,
                            packet.nonredundant.computer_vision_confidence,
                            packet.image_sequence_number
                        );
                    }
                    #elif 0
                    {

                        if (packet.image_sequence_number == 1) // @/`ESP32 Sequence Numbers`.
                        {
                            stlink_tx(TV_TOKEN_END);
                            stlink_tx(TV_TOKEN_START);
                        }

                        if (packet.image_sequence_number) // @/`ESP32 Sequence Numbers`.
                        {
                            UXART_tx_bytes(UXARTHandle_stlink, packet.image_bytes, countof(packet.image_bytes));
                        }

                    }
                    #endif
                }

            } break;

            case ESP32FindStartTokenResult_lora:
            {
                sorry
            } break;

            default: sorry

        }

    }
}
