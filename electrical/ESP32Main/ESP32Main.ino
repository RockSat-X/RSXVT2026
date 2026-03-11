#define COMPILING_ESP32 true
#define ESPNOW_ENABLE   true
#define LORA_ENABLE     true
#include "../system.h"



#if LORA_ENABLE
static volatile b32       packet_lora_radio_data_available  = false;
static struct LoRaPacket  packet_lora_buffer[128]           = {};
static u32                packet_lora_writer                = 0;
static u32                packet_lora_reader                = 0;
static i32                packet_lora_invalid_length_count  = 0;
static i32                packet_lora_radio_error_count     = 0;
static i32                packet_lora_overrun_count         = 0;
static i32                packet_lora_crc_error_count       = 0;
#endif

#if ESPNOW_ENABLE
static struct ESP32Packet packet_espnow_buffer[128]          = {};
static volatile u32       packet_espnow_writer               = 0;
static volatile u32       packet_espnow_reader               = 0;
static volatile i32       packet_espnow_invalid_length_count = 0;
static volatile i32       packet_espnow_overrun_count        = 0;
static i32                packet_espnow_crc_error_count      = 0;
#endif



#if ESPNOW_ENABLE
extern void
packet_espnow_reception_callback
(
    const esp_now_recv_info* info,
    const u8*                received_data,
    int                      received_amount
)
{

    // Received packet is of the wrong length?

    if (received_amount != sizeof(struct ESP32Packet))
    {
        packet_espnow_invalid_length_count += 1;
    }



    // Not enough space in the ring-buffer?

    else if (packet_espnow_writer - packet_espnow_reader >= countof(packet_espnow_buffer))
    {
        packet_espnow_overrun_count += 1;
    }



    // Push packet into ring-buffer to be processed later.

    else
    {

        struct ESP32Packet* packet = &packet_espnow_buffer[packet_espnow_writer % countof(packet_espnow_buffer)];

        memmove(packet, received_data, sizeof(*packet));

        packet_espnow_writer += 1;

    }

}
#endif



#if LORA_ENABLE
extern void
packet_lora_callback(void)
{
    packet_lora_radio_data_available = true;
}
#endif



extern void
setup(void)
{

    // Debug set-up.

    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);



    // Initialize UART stuff.

    common_init_uart();



    // Initialize ESP-NOW stuff.

    #if ESPNOW_ENABLE
    {

        common_init_esp_now();

        esp_now_register_recv_cb(packet_espnow_reception_callback);

    }
    #endif



    // Initialize LoRa stuff.

    #if LORA_ENABLE
    {

        common_init_lora();

        if (packet_lora_radio.startReceive() != RADIOLIB_ERR_NONE)
        {
            Serial.printf("Failed to start receiving.\n");
            ESP.restart();
            return;
        }

    }
    #endif


}



extern void
loop(void)
{

    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));

    #if ESPNOW_ENABLE
    static u32 packet_espnow_bytes_received                    = 0;
    static i32 packet_espnow_consecutive_sequence_number_count = 0;
    static i32 packet_espnow_broken_sequence_number_count      = 0;
    #endif

    #if LORA_ENABLE
    static u32 packet_lora_bytes_received                     = 0;
    static i32 packet_lora_consecutive_sequence_number_count  = 0;
    static i32 packet_lora_broken_sequence_number_count       = 0;
    #endif



    // See if the LoRa module got some data.

    #if LORA_ENABLE
    {
        if (packet_lora_radio_data_available)
        {

            packet_lora_radio_data_available = false;

            struct LoRaPacket* packet = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];



            // Received packet is of the wrong length?
            // TODO Test.

            if (packet_lora_radio.getPacketLength() != sizeof(*packet))
            {

                // TODO This might flush it.

                u8 dummy = 0;
                packet_lora_radio.readData(&dummy, 1);

                packet_lora_invalid_length_count += 1;

            }



            // Not enough space in the ring-buffer?

            else if (packet_lora_writer - packet_lora_reader >= countof(packet_lora_buffer))
            {
                packet_lora_overrun_count += 1;
            }



            // Failed to receive the LoRa data?

            else if (packet_lora_radio.readData((u8*) packet, sizeof(*packet)) != RADIOLIB_ERR_NONE)
            {
                packet_lora_radio_error_count += 1;
            }



            // The LoRa packet is now in the ring-buffer to be processed later.

            else
            {
                packet_lora_writer += 1;
            }

        }
    }
    #endif



    // See if there's a new packet in the ESP-NOW ring-buffer.

    #if ESPNOW_ENABLE
    {
        if (packet_espnow_reader != packet_espnow_writer)
        {

            struct ESP32Packet* packet = &packet_espnow_buffer[packet_espnow_reader % countof(packet_espnow_buffer)];



            // Check CRC checksum.

            u8 digest = ESP32_calculate_crc((u8*) packet, sizeof(*packet));

            if (digest)
            {
                packet_espnow_crc_error_count += 1;
            }
            else
            {

                // Check sequence number. @/`ESP32 Sequence Numbers`.

                static typeof(packet->nonredundant.rolling_sequence_number) expected_rolling_sequence_number = 0;

                if (packet->nonredundant.rolling_sequence_number == expected_rolling_sequence_number)
                {
                    packet_espnow_consecutive_sequence_number_count += 1;
                }
                else
                {
                    expected_rolling_sequence_number            = packet->nonredundant.rolling_sequence_number;
                    packet_espnow_broken_sequence_number_count += 1;
                }

                expected_rolling_sequence_number += 1;



                // Count the amount of data we got.

                packet_espnow_bytes_received += sizeof(*packet);



                // Send the data over to the main flight computer.

                u16 token = PACKET_ESP32_START_TOKEN;
                Serial1.write((u8*) &token, sizeof(token));
                Serial1.write((u8*) packet, sizeof(*packet));

            }



            // We're done with the packet.

            packet_espnow_reader += 1;

        }
    }
    #endif



    // See if there's a new packet in the LoRa ring-buffer.

    #if LORA_ENABLE
    {
        if (packet_lora_reader != packet_lora_writer)
        {

            struct LoRaPacket* packet = &packet_lora_buffer[packet_lora_reader % countof(packet_lora_buffer)];



            // Check CRC checksum.

            u8 digest = ESP32_calculate_crc((u8*) packet, sizeof(*packet));

            if (digest)
            {
                packet_lora_crc_error_count += 1;
            }
            else
            {

                // Check sequence number. @/`ESP32 Sequence Numbers`.

                static typeof(packet->rolling_sequence_number) expected_rolling_sequence_number = 0;

                if (packet->rolling_sequence_number == expected_rolling_sequence_number)
                {
                    packet_lora_consecutive_sequence_number_count += 1;
                }
                else
                {
                    expected_rolling_sequence_number          = packet->rolling_sequence_number;
                    packet_lora_broken_sequence_number_count += 1;
                }

                expected_rolling_sequence_number += 1;



                // Count the amount of data we got.

                packet_lora_bytes_received += sizeof(*packet);



                // Send the data over to the main flight computer.

                u16 token = PACKET_LORA_START_TOKEN;
                Serial1.write((u8*) &token, sizeof(token));
                Serial1.write((u8*) packet, sizeof(*packet));

            }



            // We're done with the packet.

            packet_lora_reader += 1;

        }
    }
    #endif



    // Output statistics at regular intervals.

    static u32 last_statistic_timestamp_ms = 0;

    u32 current_timestamp_ms = millis();

    if (current_timestamp_ms - last_statistic_timestamp_ms >= 500)
    {

        last_statistic_timestamp_ms = current_timestamp_ms;

        u64 mac_address = ESP.getEfuseMac();

        Serial.printf
        (
            "MAC address                                             : { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X }" "\n",
            (int) ((mac_address >> (0 * 8)) & 0xFF),
            (int) ((mac_address >> (1 * 8)) & 0xFF),
            (int) ((mac_address >> (2 * 8)) & 0xFF),
            (int) ((mac_address >> (3 * 8)) & 0xFF),
            (int) ((mac_address >> (4 * 8)) & 0xFF),
            (int) ((mac_address >> (5 * 8)) & 0xFF)
        );

        #if ESPNOW_ENABLE
        {
            Serial.printf("(ESP32) Payload bytes received                          : %u"   " bytes" "\n", packet_espnow_bytes_received);
            Serial.printf("(ESP32) Average throughput since power-on               : %.0f" " KiB/s" "\n", packet_espnow_bytes_received / (millis() / 1000.0f) / 1024.0f);
            Serial.printf("(ESP32) Packets of invalid length so far                : %d"            "\n", packet_espnow_invalid_length_count);
            Serial.printf("(ESP32) Packets dropped from ring-buffer so far         : %d"            "\n", packet_espnow_overrun_count);
            Serial.printf("(ESP32) Packets with consecutive sequence number so far : %d"            "\n", packet_espnow_consecutive_sequence_number_count);
            Serial.printf("(ESP32) Packets with broken sequence number so far      : %d"            "\n", packet_espnow_broken_sequence_number_count);
            Serial.printf("(ESP32) Packets with checksum error so far              : %d"            "\n", packet_espnow_crc_error_count);
        }
        #endif

        #if LORA_ENABLE
        {
            Serial.printf("(LoRa)  Payload bytes received                          : %u"   " bytes" "\n", packet_lora_bytes_received);
            Serial.printf("(LoRa)  Average throughput since power-on               : %.0f" " B/s"   "\n", packet_lora_bytes_received / (millis() / 1000.0f));
            Serial.printf("(LoRa)  Packets of invalid length so far                : %d"            "\n", packet_lora_invalid_length_count);
            Serial.printf("(LoRa)  Packets with radio error so far                 : %d"            "\n", packet_lora_radio_error_count);
            Serial.printf("(LoRa)  Packets dropped from ring-buffer so far         : %d"            "\n", packet_lora_overrun_count);
            Serial.printf("(LoRa)  Packets with consecutive sequence number so far : %d"            "\n", packet_lora_consecutive_sequence_number_count);
            Serial.printf("(LoRa)  Packets with broken sequence number so far      : %d"            "\n", packet_lora_broken_sequence_number_count);
            Serial.printf("(LoRa)  Packets with checksum error so far              : %d"            "\n", packet_lora_crc_error_count);
        }
        #endif

        Serial.printf("\n");

    }

}
