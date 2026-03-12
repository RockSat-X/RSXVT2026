#include "system.h"
#include "timekeeping.c"
#include "uxart.c"



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

        while (true)
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

                } break;

                case ESP32FindStartTokenResult_lora:
                {
                    sorry
                } break;

                default: sorry

            }

        }

    }
}
