#define COMPILING_ESP32 true
#include "../system.h"



const static u8 MAIN_ESP32_MAC_ADDRESS[] = { 0x10, 0xB4, 0x1D, 0xE8, 0x97, 0x28 };

static struct PacketESP32 packet_esp32_buffer[128]       = {};
static volatile u32       packet_esp32_writer            = 0;
static volatile u32       packet_esp32_reader            = 0;
static volatile b32       packet_esp32_transmission_busy = false;

// LoRa ring-buffer shouldn't be very deep because
// it's such low throughput. If it was deep, a lot
// of the data might be data far in the past and not
// the more recent stuff.
static struct PacketLoRa  packet_lora_buffer[4]          = {};
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



    // Initialize UART stuff.

    common_init_uart();



    // Initialize ESP-NOW stuff.

    common_init_esp_now();

    esp_now_register_send_cb(packet_esp32_transmission_callback);

    esp_now_peer_info_t info = {};
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

    common_init_lora();

}



extern void
loop(void)
{

    // Heart-beat.

    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));



    // See if we received data over UART.
    // We determine the start of a payload
    // by looking for the magic starting token
    // in place of the sequence number.

    while
    (
        Serial1.available() >= sizeof(struct PacketESP32)
        && Serial1.read() == (PACKET_ESP32_START_TOKEN & 0xFF)
        && Serial1.peek() == ((PACKET_ESP32_START_TOKEN >> 8) & 0xFF)
        && Serial1.read() == ((PACKET_ESP32_START_TOKEN >> 8) & 0xFF)
    )
    {

        // Get the rest of the payload data.

        struct PacketESP32 payload = {};

        Serial1.readBytes((char*) &payload, sizeof(payload));

        Serial.printf("Got payload (%u ms).\n", payload.nonredundant.timestamp_ms);



        // Try pushing packet into ESP-NOW ring-buffer.

        if (packet_esp32_writer - packet_esp32_reader < countof(packet_esp32_buffer))
        {

            struct PacketESP32*                                 packet                  = &packet_esp32_buffer[packet_esp32_writer % countof(packet_esp32_buffer)];
            static typeof(packet->nonredundant.sequence_number) current_sequence_number = 0;

            *packet                               = payload;
            packet->nonredundant.sequence_number  = current_sequence_number;
            current_sequence_number              += 1;
            packet_esp32_writer                  += 1;

        }



        // Try pushing packet into LoRa ring-buffer.

        if (packet_lora_writer - packet_lora_reader < countof(packet_lora_buffer))
        {

            struct PacketLoRa*                     packet                  = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];
            static typeof(packet->sequence_number) current_sequence_number = 0;

            *packet                  = payload.nonredundant;
            packet->sequence_number  = current_sequence_number;
            current_sequence_number += 1;
            packet_lora_writer      += 1;

        }

    }



    // See if there's an ESP32 packet to be sent and
    // that we can transmit one at all.

    if (packet_esp32_reader != packet_esp32_writer && !packet_esp32_transmission_busy)
    {

        packet_esp32_transmission_busy = true;

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_reader % countof(packet_esp32_buffer)];

        Serial.printf("Sending ESP-NOW packet (%u ms).\n", packet->nonredundant.timestamp_ms);

        if (esp_now_send(MAIN_ESP32_MAC_ADDRESS, (u8*) packet, sizeof(*packet)) != ESP_OK)
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

        Serial.printf("Sending LoRa packet (%u ms).\n", packet->timestamp_ms);

        packet_lora_radio.startTransmit((u8*) packet, sizeof(*packet)); // TODO Error checking?

    }

}
