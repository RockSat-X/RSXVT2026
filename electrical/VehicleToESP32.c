#include "system.h"
#include "uxart.c"

pack_push

    struct PacketLoRa
    {
        u16 sequence_number;
        u16 timestamp_ms;
        f32 quaternion_i;
        f32 quaternion_j;
        f32 quaternion_k;
        f32 quaternion_r;
        f32 accelerometer_x;
        f32 accelerometer_y;
        f32 accelerometer_z;
        f32 gyro_x;
        f32 gyro_y;
        f32 gyro_z;
        f32 computer_vision_confidence;
    };

    struct PacketESP32
    {
        struct PacketLoRa nonredundant;
        f32               magnetometer_x;
        f32               magnetometer_y;
        f32               magnetometer_z;
        u8                image_chunk[190];
    };

pack_pop


extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_esp32);

    for (i32 iteration = 0;; iteration += 1)
    {

        struct PacketESP32 payload =
            {
                .nonredundant.sequence_number            = 0xBABE,
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

        _UXART_tx_raw_nonreentrant(UXARTHandle_esp32, (u8*) &payload, sizeof(payload));

        GPIO_TOGGLE(debug);

    }

}
