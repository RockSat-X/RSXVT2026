#include "../ESP32.h"



const static u8 MAIN_ESP32_MAC_ADDRESS[] = { 0x10, 0xB4, 0x1D, 0xE8, 0x97, 0x28 };

static struct PacketESP32 packet_esp32_buffer[128]       = {0};
static volatile u32       packet_esp32_writer            = 0;
static volatile u32       packet_esp32_reader            = 0;
static volatile b32       packet_esp32_transmission_busy = false;

static SX1262             packet_lora_radio              = new Module(41, 39, 42, 40);
static struct PacketLoRa  packet_lora_buffer[128]        = {0};
static volatile u32       packet_lora_writer             = 0;
static volatile u32       packet_lora_reader             = 0;
static volatile bool      packet_lora_transmission_busy  = false;



extern void
packet_esp32_transmission_callback
(
    const wifi_tx_info_t* info,
    esp_now_send_status_t status
)
{
    packet_esp32_reader            += 1;
    packet_esp32_transmission_busy  = false;
}



extern void
packet_lora_callback(void)
{
    packet_lora_reader            += 1;
    packet_lora_transmission_busy  = false;
}



extern void
setup(void)
{

    // Debug set-up.

    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);



    // Initialize WiFi stuff.
    // TODO Look more into the specs.
    // TODO Make robust.

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.printf("Error initializing ESP-NOW.\n");
        ESP.restart();
        return;
    }

    esp_now_register_send_cb(packet_esp32_transmission_callback);

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



    // Initialize LoRa stuff.
    // TODO Look more into the specs.
    // TODO Make robust.

    if (packet_lora_radio.begin() != RADIOLIB_ERR_NONE)
    {
        Serial.printf("Failed to initialize radio.\n");
        ESP.restart();
        return;
    }

    packet_lora_radio.setFrequency(915.0);
    packet_lora_radio.setBandwidth(7.8);
    packet_lora_radio.setSpreadingFactor(6);
    packet_lora_radio.setCodingRate(5);
    packet_lora_radio.setOutputPower(22);
    packet_lora_radio.setPreambleLength(8);
    packet_lora_radio.setSyncWord(0x34);
    packet_lora_radio.setCRC(true);

    packet_lora_radio.setDio1Action(packet_lora_callback);

}



extern void
loop(void)
{

    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));



    // Space for another ESP32 packet to be queued up for transmission?

    if (packet_esp32_writer - packet_esp32_reader < countof(packet_esp32_buffer))
    {

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_writer % countof(packet_esp32_buffer)];



        // Fill in the sequence number.

        static typeof(packet->sequence_number) current_sequence_number = {0};

        packet->sequence_number = current_sequence_number;

        current_sequence_number += 1;



        // Fill in the payload data.

        // TODO.



        // A new packet is available in the ring-buffer!

        packet_esp32_writer += 1;

    }



    // Space for another LoRa packet to be queued up for transmission?

    if (packet_lora_writer - packet_lora_reader < countof(packet_lora_buffer))
    {

        struct PacketLoRa* packet_lora = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];



        // Fill in the sequence number.

        static typeof(packet_lora->sequence_number) current_sequence_number = {0};

        packet_lora->sequence_number = current_sequence_number;

        current_sequence_number += 1;



        // Fill in the payload data.

        // TODO.



        // A new packet is available in the ring-buffer!

        packet_lora_writer += 1;

    }



    // See if there's an ESP32 packet to be sent and
    // that we can transmit one at all.

    if (packet_esp32_reader != packet_esp32_writer && !packet_esp32_transmission_busy)
    {

        packet_esp32_transmission_busy = true;

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_reader % countof(packet_esp32_buffer)];

        if
        (
            esp_now_send
            (
                MAIN_ESP32_MAC_ADDRESS,
                (u8*) packet,
                sizeof(*packet)
            ) != ESP_OK
        )
        {
            Serial.printf("Send Error!\n");
            packet_esp32_transmission_busy = false;
        }

    }



    // See if there's an LoRa packet to be sent and
    // that we can transmit one at all.

    if (packet_lora_reader != packet_lora_writer && !packet_lora_transmission_busy)
    {

        packet_lora_transmission_busy = true;

        struct PacketLoRa* packet = &packet_lora_buffer[packet_lora_reader % countof(packet_lora_buffer)];

        packet_lora_radio.startTransmit((u8*) packet, sizeof(*packet));

    }

}
