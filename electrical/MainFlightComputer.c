#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"



////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



static useret b32
esp32_get_byte(u8* dst)
{

    u32 start_timestamp_us = TIMEKEEPING_microseconds();

    while (true)
    {

        u32 current_timestamp_us = TIMEKEEPING_microseconds();

        if (UXART_rx(UXARTHandle_esp32, dst))
        {
            return true; // Got what we needed.
        }
        else if (current_timestamp_us - start_timestamp_us < 3'000'000) // TODO Time-out duration when we first power-on?
        {
            FREERTOS_delay_ms(1); // Hmm, just give the ESP32 a moment...
        }
        else
        {
            return false; // The ESP32's been quiet... maybe it's bricked..?
        }

    }

}

static useret enum ESP32GetPacketResult : u32
{
    ESP32GetPacketResult_esp32,
    ESP32GetPacketResult_lora,
    ESP32GetPacketResult_timeout,
    ESP32GetPacketResult_crc_mismatch,
}
esp32_get_packet(struct ESP32Packet* dst_packet)
{

    if (!dst_packet)
        sus;

    memzero(dst_packet);



    // First find the starting token of the payload.

    i32                        esp32_match_length = 0;
    i32                        lora_match_length  = 0;
    enum ESP32GetPacketResult kind               = {0};

    while (true)
    {

        u8 data = {0};

        if (!esp32_get_byte(&data))
        {
            return ESP32GetPacketResult_timeout;
        }

        if (ESP32_TOKEN_START[esp32_match_length] != data) // Reset the search for the ESP32 token?
        {
            esp32_match_length = 0;
        }

        if (ESP32_TOKEN_START[esp32_match_length] == data)
        {

            esp32_match_length += 1;

            if (ESP32_TOKEN_START[esp32_match_length] == '\0')
            {
                kind = ESP32GetPacketResult_esp32;
                break; // Found the ESP32 start token!
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
                kind = ESP32GetPacketResult_lora;
                break; // Found the LoRa start token!
            }

        }

    }



    // Get the payload data based on the token.

    u8* write_pointer = kind == ESP32GetPacketResult_esp32 ? (u8*) dst_packet    : (u8*) &dst_packet->nonredundant;
    i32 write_size    = kind == ESP32GetPacketResult_esp32 ? sizeof(*dst_packet) : sizeof(dst_packet->nonredundant);

    for (i32 i = 0; i < write_size; i += 1)
    {
        if (!esp32_get_byte(&write_pointer[i]))
        {
            return ESP32GetPacketResult_timeout;
        }
    }



    // Verify integrity of the payload.

    u8 digest = ESP32_calculate_crc(write_pointer, write_size);

    if (digest)
    {
        return ESP32GetPacketResult_crc_mismatch;
    }
    else
    {
        return kind;
    }

}

FREERTOS_TASK(esp32, 0)
{
    for (;;)
    {

        // Reset the ESP32 for a bit...

        GPIO_ACTIVE(esp32_reset);
        FREERTOS_delay_ms(10);



        // Reboot our UART communication...

        UXART_reinit(UXARTHandle_esp32);



        // Reawaken the ESP32!

        FREERTOS_delay_ms(10);
        GPIO_INACTIVE(esp32_reset);



        // TODO.

        b32 need_to_reinitialize = false;

        while (!need_to_reinitialize)
        {

            struct ESP32Packet        packet = {0};
            enum ESP32GetPacketResult result = esp32_get_packet(&packet);

            switch (result)
            {



                // TODO.

                case ESP32GetPacketResult_esp32:
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
                } break;



                // TODO.

                case ESP32GetPacketResult_lora:
                {
                    sorry
                } break;



                // TODO.

                case ESP32GetPacketResult_timeout:
                {
                    need_to_reinitialize = true;
                } break;



                // TODO.

                case ESP32GetPacketResult_crc_mismatch:
                {
                    stlink_tx(">>>> CRC mismatch! Maybe due to UART RX-FIFO overflow... <<<<\n\n");
                } break;



                default: sorry

            }

        }



        // TODO Indicate there was an ESP32 error.

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(debug_board, 0)
{
    for (;;)
    {

        I2C_partial_reinit(I2CHandle_debug_board);

        b32 need_to_reinitialize = false;

        while (!need_to_reinitialize)
        {

            struct MainFlightComputerDebugPacket packet =
                {
                    .timestamp_us           = TIMEKEEPING_microseconds(),
                    .solarboard_voltages[0] = 67, // TODO.
                    .solarboard_voltages[1] = 69, // TODO.
                    .flags                  = 0,  // TODO.
                };

            packet.crc = DEBUG_BOARD_calculate_crc((u8*) &packet, sizeof(packet) - sizeof(packet.crc));

            struct I2CDoJob job =
                {
                    .handle       = I2CHandle_debug_board,
                    .address_type = I2CAddressType_seven,
                    .address      = MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS,
                    .writing      = true,
                    .repeating    = false,
                    .pointer      = (u8*) &packet,
                    .amount       = sizeof(packet)
                };

            for (b32 yield = false; !yield;)
            {

                enum I2CDoResult result = I2C_do(&job);

                switch (result)
                {

                    case I2CDoResult_working:
                    {
                        FREERTOS_delay_ms(1); // We'll keep on spin-locking until the transfer is done...
                    } break;

                    case I2CDoResult_success:
                    {
                        yield = true;
                    } break;

                    case I2CDoResult_no_acknowledge:
                    case I2CDoResult_clock_stretch_timeout:
                    case I2CDoResult_bus_misbehaved:
                    case I2CDoResult_watchdog_expired:
                    case I2CDoResult_bug:
                    default:
                    {
                        need_to_reinitialize = true;
                        yield                = true;
                    } break;

                }

            }

            FREERTOS_delay_ms(100);

        }

    }
}
