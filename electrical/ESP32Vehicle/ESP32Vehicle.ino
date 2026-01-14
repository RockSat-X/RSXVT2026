#define COMPILING_ESP32 true
#include "../system.h"



const static u8 MAIN_ESP32_MAC_ADDRESS[] =
    {
        #if __has_include("MAIN_ESP32_MAC_ADDRESS.txt")
            #include "MAIN_ESP32_MAC_ADDRESS.txt"
        #else
            #error "Make the file './electrical/ESP32Vehicle/MAIN_ESP32_MAC_ADDRESS.txt' with the MAC address of the other ESP32 please! Example of what the file should only contain: '0x10, 0xB4, 0x1D, 0xE8, 0x97, 0x28'."
        #endif
    };

static_assert(countof(MAIN_ESP32_MAC_ADDRESS) == 6);



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

static i32                last_uart_packet_timestamp_ms  = 0;
static i32                packet_uart_packet_count       = 0;
static i32                packet_uart_crc_error_count    = 0;
static i32                packet_esp32_packet_count      = 0;
static i32                packet_esp32_overrun_count     = 0;
static i32                packet_lora_packet_count       = 0;
static i32                packet_lora_overrun_count      = 0;



extern void
packet_esp32_transmission_callback
(
    const wifi_tx_info_t* info,
    esp_now_send_status_t status
)
{

    if (packet_esp32_writer == packet_esp32_reader) // TODO @/`Ring-Buffer Bug`.
    {
        for (;;)
        {
            digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
            delay(1000);
        }
    }

    packet_esp32_reader            += 1;
    packet_esp32_transmission_busy  = false;

}



extern void
packet_lora_callback(void)
{

    if (packet_lora_writer == packet_lora_reader) // TODO @/`Ring-Buffer Bug`.
    {
        for (;;)
        {
            digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
            delay(2000);
        }
    }

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

        u8 digest = ESP32_calculate_crc((u8*) &payload, sizeof(payload));

        last_uart_packet_timestamp_ms  = payload.nonredundant.timestamp_ms;
        packet_uart_packet_count      += 1;



        // If there's no corruption between vehicle FC and ESP32,
        // we're ready to send the packet from the vehicle ESP32 to the main ESP32.

        if (digest)
        {
            packet_uart_crc_error_count += 1;
        }
        else
        {

            // Try pushing packet into ESP-NOW ring-buffer.

            if (packet_esp32_writer - packet_esp32_reader < countof(packet_esp32_buffer))
            {

                struct PacketESP32*                                 packet                  = &packet_esp32_buffer[packet_esp32_writer % countof(packet_esp32_buffer)];
                static typeof(packet->nonredundant.sequence_number) current_sequence_number = 0;

                *packet                               = payload;
                packet->nonredundant.sequence_number  = current_sequence_number;
                packet->nonredundant.crc              = ESP32_calculate_crc((u8*) packet, sizeof(*packet) - sizeof(packet->nonredundant.crc));
                current_sequence_number              += 1;
                packet_esp32_writer                  += 1;
                packet_esp32_packet_count            += 1;

            }
            else
            {
                packet_esp32_overrun_count += 1;
            }



            // Try pushing packet into LoRa ring-buffer.

            if (packet_lora_writer - packet_lora_reader < countof(packet_lora_buffer))
            {

                struct PacketLoRa*                     packet                  = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];
                static typeof(packet->sequence_number) current_sequence_number = 0;

                *packet                   = payload.nonredundant;
                packet->sequence_number   = current_sequence_number;
                packet->crc               = ESP32_calculate_crc((u8*) packet, sizeof(*packet) - sizeof(packet->crc));
                current_sequence_number  += 1;
                packet_lora_writer       += 1;
                packet_lora_packet_count += 1;

            }
            else
            {
                packet_lora_overrun_count += 1;
            }

        }

    }

    if (packet_esp32_writer - packet_esp32_reader > countof(packet_esp32_buffer)) // TODO @/`Ring-Buffer Bug`.
    {

        Serial.println(packet_esp32_writer);
        Serial.println(packet_esp32_reader);
        Serial.println(packet_esp32_writer - packet_esp32_reader);
        Serial.println(countof(packet_esp32_buffer));
        Serial.println(packet_esp32_writer - packet_esp32_reader > countof(packet_esp32_buffer));

        for (;;)
        {
            digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
            delay(100);
        }

    }

    if (packet_lora_writer - packet_lora_reader > countof(packet_lora_buffer)) // TODO @/`Ring-Buffer Bug`.
    {

        Serial.println(packet_lora_writer);
        Serial.println(packet_lora_reader);
        Serial.println(packet_lora_writer - packet_lora_reader);
        Serial.println(countof(packet_lora_buffer));
        Serial.println(packet_lora_writer - packet_lora_reader > countof(packet_lora_buffer));

        for (;;)
        {
            digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
            delay(500);
        }

    }



    // See if there's an ESP32 packet to be sent and
    // that we can transmit one at all.

    if (packet_esp32_reader != packet_esp32_writer && !packet_esp32_transmission_busy)
    {

        packet_esp32_transmission_busy = true;

        struct PacketESP32* packet = &packet_esp32_buffer[packet_esp32_reader % countof(packet_esp32_buffer)];

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

        packet_lora_radio.startTransmit((u8*) packet, sizeof(*packet)); // TODO Error checking?

    }



    // Output statistics at regular intervals.

    static u32 last_statistic_timestamp_ms = 0;

    u32 current_timestamp_ms = millis();

    if (current_timestamp_ms - last_statistic_timestamp_ms >= 500)
    {

        last_statistic_timestamp_ms = current_timestamp_ms;

        Serial.printf("Last timestamp        : %d ms" "\n", last_uart_packet_timestamp_ms);
        Serial.printf("UART packets received : %d"    "\n", packet_uart_packet_count);
        Serial.printf("UART CRC mismatches   : %d"    "\n", packet_uart_crc_error_count);
        Serial.printf("ESP32 packets queued  : %d"    "\n", packet_esp32_packet_count);
        Serial.printf("ESP32 packet overruns : %d"    "\n", packet_esp32_overrun_count);
        Serial.printf("LoRa packets queued   : %d"    "\n", packet_lora_packet_count);
        Serial.printf("LoRa packet overruns  : %d"    "\n", packet_lora_overrun_count);
        Serial.printf("\n");

    }

}



// @/`Ring-Buffer Bug`:
// There seems to be a bug where the ring-buffer reader
// gets ahead of the writer somehow, and as a result, the
// buffer seems to be always full but the vehicle will
// end up transmitting stale packet data.
// There are some if-statements with infinite loops in them
// to catch these instances. I, however, haven't been able
// to seen the bug reproduce at all yet.
// If need be, we can easily patch this by ensuring the reader
// is always behind the writer if we find it not to be so;
// this is just a band-aid to the underlying issue, but given
// it's so rare, it just might be fine.



// TODO I think after a while, ESP-NOW packets stop being sent/received. Investigate.
