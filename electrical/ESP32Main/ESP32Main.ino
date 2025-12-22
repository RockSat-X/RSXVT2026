#include "../ESP32.h"



static struct PacketLoRa  packet_lora_buffer[128]           = {};
static volatile u32       packet_lora_writer                = 0;
static volatile u32       packet_lora_reader                = 0;
static volatile b32       packet_lora_radio_data_available  = false;
static volatile i32       packet_lora_invalid_length_count  = 0;
static volatile i32       packet_lora_radio_error_count     = 0;
static volatile i32       packet_lora_overrun_count         = 0;

static struct PacketESP32 packet_esp32_buffer[128]          = {};
static volatile u32       packet_esp32_writer               = 0;
static volatile u32       packet_esp32_reader               = 0;
static volatile i32       packet_esp32_invalid_length_count = 0;
static volatile i32       packet_esp32_overrun_count        = 0;



extern void
packet_esp32_reception_callback
(
    const esp_now_recv_info* info,
    const u8*                received_data,
    int                      received_amount
)
{

    // Received packet is of the wrong length?

    if (received_amount != sizeof(struct PacketESP32))
    {
        packet_esp32_invalid_length_count += 1;
    }



    // Not enough space in the ring-buffer?

    else if (packet_esp32_writer - packet_esp32_reader >= countof(packet_esp32_buffer))
    {
        packet_esp32_overrun_count += 1;
    }



    // Push packet into ring-buffer to be processed later.

    else
    {

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_writer % countof(packet_esp32_buffer)];

        memmove(packet, received_data, sizeof(*packet));

        packet_esp32_writer += 1;

    }

}



extern void
packet_lora_callback(void)
{
    packet_lora_radio_data_available = true;
}



extern void
setup(void)
{

    // Debug set-up.

    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);



    // Initialize ESP-NOW stuff.

    common_init_esp_now();

    esp_now_register_recv_cb(packet_esp32_reception_callback);



    // Initialize LoRa stuff.

    common_init_lora();

    if (packet_lora_radio.startReceive() != RADIOLIB_ERR_NONE)
    {
        Serial.printf("Failed to start receiving.\n");
        ESP.restart();
        return;
    }

}



extern void
loop(void)
{

    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));

    static u32 packet_esp32_bytes_received                    = 0;
    static i32 packet_esp32_consecutive_sequence_number_count = 0;
    static i32 packet_esp32_broken_sequence_number_count      = 0;

    static u32 packet_lora_bytes_received                     = 0;
    static i32 packet_lora_consecutive_sequence_number_count  = 0;
    static i32 packet_lora_broken_sequence_number_count       = 0;



    // See if there's a new packet in the ESP-NOW ring-buffer.

    if (packet_esp32_reader != packet_esp32_writer)
    {

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_reader % countof(packet_esp32_buffer)];



        // Check sequence number.

        static typeof(packet->nonredundant.sequence_number) expected_sequence_number = 0;

        if (packet->nonredundant.sequence_number == expected_sequence_number)
        {
            packet_esp32_consecutive_sequence_number_count += 1;
        }
        else
        {
            expected_sequence_number                   = packet->nonredundant.sequence_number;
            packet_esp32_broken_sequence_number_count += 1;
        }

        expected_sequence_number += 1;



        // Count the amount of data we got.

        packet_esp32_bytes_received += sizeof(*packet);



        // We're done with the packet.

        packet_esp32_reader += 1;

    }



    // See if the LoRa module got some data.

    if (packet_lora_radio_data_available)
    {

        packet_lora_radio_data_available = false;

        struct PacketLoRa* packet = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];



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



    // See if there's a new packet in the LoRa ring-buffer.

    if (packet_lora_reader != packet_lora_writer)
    {

        struct PacketLoRa* packet = &packet_lora_buffer[packet_lora_reader % countof(packet_lora_buffer)];



        // Check sequence number.

        static typeof(packet->sequence_number) expected_sequence_number = 0;

        if (packet->sequence_number == expected_sequence_number)
        {
            packet_lora_consecutive_sequence_number_count += 1;
        }
        else
        {
            expected_sequence_number                  = packet->sequence_number;
            packet_lora_broken_sequence_number_count += 1;
        }

        expected_sequence_number += 1;



        // Count the amount of data we got.

        packet_lora_bytes_received += sizeof(*packet);



        // We're done with the packet.

        packet_lora_reader += 1;

    }



    // Output statistics at regular intervals.

    static u32 last_statistic_timestamp_ms = 0;

    u32 current_timestamp_ms = millis();

    if (current_timestamp_ms - last_statistic_timestamp_ms >= 500)
    {

        last_statistic_timestamp_ms = current_timestamp_ms;

        Serial.printf("MAC Address : { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X }"        "\n", WiFi.macAddress()[0], WiFi.macAddress()[1], WiFi.macAddress()[2], WiFi.macAddress()[3], WiFi.macAddress()[4], WiFi.macAddress()[5]);
        Serial.printf("(ESP32) Payload bytes received                          : %u"   " bytes" "\n", packet_esp32_bytes_received);
        Serial.printf("(ESP32) Average throughput since power-on               : %.0f" " KiB/s" "\n", packet_esp32_bytes_received / (millis() / 1000.0f) / 1024.0f);
        Serial.printf("(ESP32) Packets of invalid length so far                : %d"            "\n", packet_esp32_invalid_length_count);
        Serial.printf("(ESP32) Packets dropped from ring-buffer so far         : %d"            "\n", packet_esp32_overrun_count);
        Serial.printf("(ESP32) Packets with consecutive sequence number so far : %d"            "\n", packet_esp32_consecutive_sequence_number_count);
        Serial.printf("(ESP32) Packets with broken sequence number so far      : %d"            "\n", packet_esp32_broken_sequence_number_count);

        Serial.printf("(LoRa)  Payload bytes received                          : %u"   " bytes" "\n", packet_lora_bytes_received);
        Serial.printf("(LoRa)  Average throughput since power-on               : %.0f" " B/s"   "\n", packet_lora_bytes_received / (millis() / 1000.0f));
        Serial.printf("(LoRa)  Packets of invalid length so far                : %d"            "\n", packet_lora_invalid_length_count);
        Serial.printf("(LoRa)  Packets with radio error so far                 : %d"            "\n", packet_lora_radio_error_count);
        Serial.printf("(LoRa)  Packets dropped from ring-buffer so far         : %d"            "\n", packet_lora_overrun_count);
        Serial.printf("(LoRa)  Packets with consecutive sequence number so far : %d"            "\n", packet_lora_consecutive_sequence_number_count);
        Serial.printf("(LoRa)  Packets with broken sequence number so far      : %d"            "\n", packet_lora_broken_sequence_number_count);

        Serial.printf("\n");

    }

}
