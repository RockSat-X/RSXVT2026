#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_esp32);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Enable the peripheral.

    CMSIS_SET(RCC, APB2ENR, TIM1EN, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIM1, PSC, PSC, STPY_TIM1_DIVIDER);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIM1, EGR, UG, true);



    // Enable the timer's counter.

    CMSIS_SET(TIM1, CR1, CEN, true);



    ////////////////////////////////////////////////////////////////////////////////



    for (i32 iteration = 0;; iteration += 1)
    {

        // Send the payload to the vehicle ESP32.

        {

            struct PacketESP32 payload =
                {
                    .magnetometer_x                          = 3.1f,
                    .magnetometer_y                          = 3.2f,
                    .magnetometer_z                          = 3.3f,
                    .image_chunk                             = { 'M', 'E', 'O', 'W', '!', '?' },
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
                    .nonredundant.timestamp_ms               = CMSIS_GET(TIM1, CNT, CNT),
                    .nonredundant.sequence_number            = 0,
                    .nonredundant.crc                        = 0x00,
                };

            payload.nonredundant.crc = calculate_crc((u8*) &payload, sizeof(payload) - sizeof(payload.nonredundant.crc));

            GPIO_HIGH(debug);
            _UXART_tx_raw_nonreentrant(UXARTHandle_esp32, (u8*) &(u16) { PACKET_ESP32_START_TOKEN }, sizeof(u16));
            _UXART_tx_raw_nonreentrant(UXARTHandle_esp32, (u8*) &payload, sizeof(payload));
            GPIO_LOW(debug);

        }



        // Any data sent from the main ESP32 will be redirected through the ST-LINK.

        for (i32 i = 0; i < (iteration / 16 % 2 == 0 ? 1'000 : 100'000); i += 1)
        {

            u8 data = {0};

            if (UXART_rx(UXARTHandle_esp32, (char*) &data))
            {
                _UXART_tx_raw_nonreentrant(UXARTHandle_stlink, &data, sizeof(data));
            }

        }

    }

}
