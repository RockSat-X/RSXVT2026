#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_esp32);

    for (i32 iteration = 0;; iteration += 1)
    {

        // Send the payload.

        {
            struct PacketESP32 payload =
                {
                    .nonredundant.sequence_number            = PACKET_ESP32_START_TOKEN,
                    .nonredundant.timestamp_ms               = 12345,
                    .nonredundant.quaternion_i               = 0.1f,
                    .nonredundant.quaternion_j               = 0.2f,
                    .nonredundant.quaternion_k               = 0.3f,
                    .nonredundant.quaternion_r               = 0.4f,
                    .nonredundant.accelerometer_x            = 1.1f,
                    .nonredundant.accelerometer_y            = 1.2f,
                    .nonredundant.accelerometer_z            = 1.3f,
                    .nonredundant.gyro_x                     = 2.1f,
                    .nonredundant.gyro_y                     = 2.2f,
                    .nonredundant.gyro_z                     = 2.3f,
                    .nonredundant.computer_vision_confidence = -1.0f,
                    .magnetometer_x                          = 3.1f,
                    .magnetometer_y                          = 3.2f,
                    .magnetometer_z                          = 3.3f,
                    .image_chunk                             = { 'M', 'E', 'O', 'W', '!', '?' },
                };

            GPIO_HIGH(debug);

            _UXART_tx_raw_nonreentrant(UXARTHandle_esp32, (u8*) &payload, sizeof(payload));

            GPIO_LOW(debug);
        }



        // Get the payload.

        {
            u8  buffer[512] = {0};
            i32 length      = 0;

            for (i32 i = 0; i < 1'000'000; i += 1)
            {

                if (length < countof(buffer))
                {
                    if (UXART_rx(UXARTHandle_esp32, (char*) &buffer[length]))
                    {
                        length += 1;
                    }
                }

            }

            struct PacketESP32* payload = (struct PacketESP32*) buffer;

            stlink_tx("sequence_number            : %u\n", payload->nonredundant.sequence_number);
            stlink_tx("timestamp_ms               : %u\n", payload->nonredundant.timestamp_ms);
            stlink_tx("quaternion_i               : %f\n", payload->nonredundant.quaternion_i);
            stlink_tx("quaternion_j               : %f\n", payload->nonredundant.quaternion_j);
            stlink_tx("quaternion_k               : %f\n", payload->nonredundant.quaternion_k);
            stlink_tx("quaternion_r               : %f\n", payload->nonredundant.quaternion_r);
            stlink_tx("accelerometer_x            : %f\n", payload->nonredundant.accelerometer_x);
            stlink_tx("accelerometer_y            : %f\n", payload->nonredundant.accelerometer_y);
            stlink_tx("accelerometer_z            : %f\n", payload->nonredundant.accelerometer_z);
            stlink_tx("gyro_x                     : %f\n", payload->nonredundant.gyro_x);
            stlink_tx("gyro_y                     : %f\n", payload->nonredundant.gyro_y);
            stlink_tx("gyro_z                     : %f\n", payload->nonredundant.gyro_z);
            stlink_tx("computer_vision_confidence : %f\n", payload->nonredundant.computer_vision_confidence);
            stlink_tx("magnetometer_x             : %f\n", payload->magnetometer_x);
            stlink_tx("magnetometer_y             : %f\n", payload->magnetometer_y);
            stlink_tx("magnetometer_z             : %f\n", payload->magnetometer_z);
            stlink_tx("\n");

        }

    }

}
