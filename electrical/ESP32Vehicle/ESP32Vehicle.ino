#include "../ESP32.h"



const static u8 MAIN_ESP32_MAC_ADDRESS[] = { 0x10, 0xB4, 0x1D, 0xE8, 0x97, 0x28 };

static struct Message message_buffer[128] = {0};
static volatile u32   message_writer      = 0;
static volatile u32   message_reader      = 0;
static volatile b32   transmission_busy   = false;



extern void
transmission_callback
(
    const wifi_tx_info_t* info,
    esp_now_send_status_t status
)
{
    message_reader    += 1;
    transmission_busy  = false;
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
        Serial.printf("Error initializing ESP-NOW.\n");
        ESP.restart();
        return;
    }

    esp_now_register_send_cb(transmission_callback);

    esp_now_peer_info_t info = {0};
    info.peer_addr[0] = MAIN_ESP32_MAC_ADDRESS[0];
    info.peer_addr[1] = MAIN_ESP32_MAC_ADDRESS[1];
    info.peer_addr[2] = MAIN_ESP32_MAC_ADDRESS[2];
    info.peer_addr[3] = MAIN_ESP32_MAC_ADDRESS[3];
    info.peer_addr[4] = MAIN_ESP32_MAC_ADDRESS[4];
    info.peer_addr[5] = MAIN_ESP32_MAC_ADDRESS[5];
    info.channel      = 1;
    info.encrypt      = false;

    if (esp_now_add_peer(&info) != ESP_OK)
    {
        Serial.printf("Failed to add peer.\n");
        ESP.restart();
        return;
    }

}



extern void
loop(void)
{

    // Space for another packet to be queued up for transmission?

    if (message_writer - message_reader < countof(message_buffer))
    {

        struct Message* message = &message_buffer[message_writer % countof(message_buffer)];



        // Fill in the sequence number.

        static typeof(message->sequence_number) current_sequence_number = {0};

        message->sequence_number = current_sequence_number;

        current_sequence_number += 1;



        // Fill in the payload data.

        // TODO.



        // A new packet is available in the ring-buffer!

        message_writer += 1;

    }



    // See if there's a packet to be sent and
    // that we can transmit one at all.

    if (message_reader != message_writer && !transmission_busy)
    {

        transmission_busy = true;

        if
        (
            esp_now_send
            (
                MAIN_ESP32_MAC_ADDRESS,
                (u8*) &message_buffer[message_reader % countof(message_buffer)],
                sizeof(struct Message)
            ) != ESP_OK
        )
        {
            Serial.printf("Send Error!\n");
            transmission_busy = false;
        }

        digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));

    }

}
