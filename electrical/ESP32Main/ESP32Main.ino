#include "../ESP32.h"



static struct Message message_buffer[8]            = {0};
static volatile u32   message_writer               = {0};
static volatile u32   message_reader               = {0};
static volatile i32   message_invalid_length_count = {0};
static volatile i32   message_overrun_count        = {0};



extern void
reception_callback
(
    const esp_now_recv_info* info,
    const u8*                received_data,
    int                      received_amount
)
{

    // Received packet is of the wrong length?

    if (received_amount != sizeof(struct Message))
    {
        message_invalid_length_count += 1;
    }



    // Not enough space in the ring-buffer?

    else if (message_writer - message_reader >= countof(message_buffer))
    {
        message_overrun_count += 1;
    }



    // Push packet into ring-buffer to be processed later.

    else
    {
        memcpy
        (
            &message_buffer[message_writer % countof(message_buffer)],
            received_data,
            received_amount
        );
        message_writer += 1;
    }

}



extern void
setup(void)
{

    // Debug set-up.

    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);



    // Initialize WiFi stuff.
    // TODO Look more into the specs.

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.printf("Error initializing ESP-NOW\n");
        ESP.restart();
        return;
    }

    esp_now_register_recv_cb(reception_callback);



    // Give the MAC address so that the vehicle
    // ESP32 can be programmed to directly message
    // this main ESP32.

    Serial.printf("Receiver MAC Address: ");
    Serial.print(WiFi.macAddress());
    Serial.printf("\n");

}



extern void
loop(void)
{

    static u32 payload_bytes_received            = {0};
    static u32 consecutive_sequence_number_count = {0};
    static u32 broken_sequence_number_count      = {0};



    // See if there's a new packet in the ring-buffer.

    if (message_reader != message_writer)
    {

        struct Message* message = &message_buffer[message_reader % countof(message_buffer)];



        // Check sequence number.

        static u8 expected_sequence_number = {0};

        if (expected_sequence_number == message->sequence_number)
        {
            consecutive_sequence_number_count += 1;
        }
        else
        {
            expected_sequence_number      = message->sequence_number;
            broken_sequence_number_count += 1;
        }

        expected_sequence_number += 1;



        // Count the amount of useful we got.

        payload_bytes_received += sizeof(message->payload);



        // We're done with the packet.

        message_reader += 1;

    }



    // Output statistics at regular intervals.

    static u32 last_statistic_timestamp_ms = {0};

    u32 current_timestamp_ms = millis();

    if (current_timestamp_ms - last_statistic_timestamp_ms >= 500)
    {

        last_statistic_timestamp_ms = current_timestamp_ms;

        Serial.printf("Payload bytes received                          : %u"   " bytes" "\n", payload_bytes_received);
        Serial.printf("Average throughput since power-on               : %.0f" " KiB/S" "\n", payload_bytes_received / (millis() / 1000.0f) / 1024.0f);
        Serial.printf("Packets of invalid length so far                : %d"            "\n", message_invalid_length_count);
        Serial.printf("Packets dropped from ring-buffer so far         : %d"            "\n", message_overrun_count);
        Serial.printf("Packets with consecutive sequence number so far : %d"            "\n", consecutive_sequence_number_count);
        Serial.printf("Packets with broken sequence number so far      : %d"            "\n", broken_sequence_number_count);
        Serial.printf("\n");

    }

}
