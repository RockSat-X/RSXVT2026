#define COMPILING_ESP32        true
#define ESPNOW_ENABLE          true
#define LORA_ENABLE            true
#define GENERATE_DUMMY_PACKETS true
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



#if ESPNOW_ENABLE || 1 // TODO Forced to be defined until we get rid of `TRAP_INVALID_RING_BUFFER_CONDITION`.
static struct ESP32Packet packet_espnow_buffer[128]       = {};
static volatile u32       packet_espnow_writer            = 0;
static volatile u32       packet_espnow_reader            = 0;
static volatile b32       packet_espnow_transmission_busy = false;
#endif

#if LORA_ENABLE || 1 // TODO Forced to be defined until we get rid of `TRAP_INVALID_RING_BUFFER_CONDITION`.
// LoRa ring-buffer shouldn't be very deep because
// it's such low throughput. If it was deep, a lot
// of the data might be data far in the past and not
// the more recent stuff.
static struct LoRaPacket packet_lora_buffer[4]         = {};
static volatile u32      packet_lora_writer            = 0;
static volatile u32      packet_lora_reader            = 0;
#endif

static i32 last_uart_packet_timestamp_ms = 0;
static i32 packet_uart_packet_count      = 0;
static i32 packet_uart_crc_error_count   = 0;

#if ESPNOW_ENABLE
static i32 packet_espnow_packet_count  = 0;
static i32 packet_espnow_overrun_count = 0;
#endif

#if LORA_ENABLE
static i32 packet_lora_packet_count  = 0;
static i32 packet_lora_overrun_count = 0;
#endif



// TODO There seems to be a bug where the ring-buffer reader
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

#define TRAP_INVALID_RING_BUFFER_CONDITION(PERIOD_MS)                                    \
    do                                                                                   \
    {                                                                                    \
        if (packet_espnow_writer - packet_espnow_reader > countof(packet_espnow_buffer)) \
        {                                                                                \
            for (;;)                                                                     \
            {                                                                            \
                digitalWrite(BUILTIN_LED, true);                                         \
                delay(10);                                                               \
                digitalWrite(BUILTIN_LED, false);                                        \
                delay(PERIOD_MS);                                                        \
            }                                                                            \
        }                                                                                \
                                                                                         \
        if (packet_lora_writer - packet_lora_reader > countof(packet_lora_buffer))       \
        {                                                                                \
            for (;;)                                                                     \
            {                                                                            \
                digitalWrite(BUILTIN_LED, false);                                        \
                delay(10);                                                               \
                digitalWrite(BUILTIN_LED, true);                                         \
                delay(PERIOD_MS);                                                        \
            }                                                                            \
        }                                                                                \
    }                                                                                    \
    while (false)



#if ESPNOW_ENABLE
extern void
packet_espnow_transmission_callback
(
    const wifi_tx_info_t* info,
    esp_now_send_status_t status
)
{

    TRAP_INVALID_RING_BUFFER_CONDITION(1000);

    packet_espnow_reader            += 1;
    packet_espnow_transmission_busy  = false;
    espnow_report_success();

}
#endif



#if LORA_ENABLE
extern void
packet_lora_callback(void)
{

    TRAP_INVALID_RING_BUFFER_CONDITION(2000);

    packet_lora_reader            += 1;
    packet_lora_transmission_busy  = false;
    lora_report_success();

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

        esp_now_register_send_cb(packet_espnow_transmission_callback);

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
            espnow_initialized = false;
            return;
        }

    }
    #endif



    // Initialize LoRa stuff.

    #if LORA_ENABLE
    {
        common_init_lora();

        lora_watchdog_arm();
    }
    #endif

}



extern void
process_payload(struct ESP32Packet* payload)
{

    u8 digest = ESP32_calculate_crc((u8*) payload, sizeof(*payload));

    last_uart_packet_timestamp_ms  = payload->nonredundant.timestamp_ms;
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

        #if ESPNOW_ENABLE
        {

            if (packet_espnow_writer - packet_espnow_reader < countof(packet_espnow_buffer))
            {

                struct ESP32Packet* packet = &packet_espnow_buffer[packet_espnow_writer % countof(packet_espnow_buffer)];

                static typeof(packet->nonredundant.rolling_sequence_number) current_rolling_sequence_number = 0; // @/`ESP32 Sequence Numbers`.

                *packet                                       = *payload;
                packet->nonredundant.rolling_sequence_number  = current_rolling_sequence_number;
                packet->nonredundant.crc                      = ESP32_calculate_crc((u8*) packet, sizeof(*packet) - sizeof(packet->nonredundant.crc));
                current_rolling_sequence_number              += 1;
                packet_espnow_writer                         += 1;
                packet_espnow_packet_count                   += 1;

                i32 free_space = countof(packet_espnow_buffer) - (packet_espnow_writer - packet_espnow_reader);

                if (free_space <= -1)
                {
                    free_space = 0;
                }

                // Indicate to the VFC that we're still transmitting over ESP-NOW.
                // The VFC will also throttle the transmission as needed in order
                // to prevent over-runs.
                Serial1.write((u8) free_space);

            }
            else
            {
                packet_espnow_overrun_count += 1;
            }

        }
        #endif



        // Try pushing packet into LoRa ring-buffer.

        #if LORA_ENABLE
        {
            if (packet_lora_writer - packet_lora_reader < countof(packet_lora_buffer))
            {

                struct LoRaPacket* packet = &packet_lora_buffer[packet_lora_writer % countof(packet_lora_buffer)];

                static typeof(packet->rolling_sequence_number) current_rolling_sequence_number = 0; // @/`ESP32 Sequence Numbers`.

                *packet                           = payload->nonredundant;
                packet->rolling_sequence_number   = current_rolling_sequence_number;
                packet->crc                       = ESP32_calculate_crc((u8*) packet, sizeof(*packet) - sizeof(packet->crc));
                current_rolling_sequence_number  += 1;
                packet_lora_writer               += 1;
                packet_lora_packet_count         += 1;

                lora_last_activity_ms = millis();

            }
            else
            {
                packet_lora_overrun_count += 1;
            }
        }
        #endif

    }

    TRAP_INVALID_RING_BUFFER_CONDITION(100);

}



extern void
loop(void)
{
    // Handle RF Channel in case of failed initialization or TX/RX error
    handle_espnow();
    handle_lora();

    // Heart-beat.

    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));



    // See if we received data over UART; we determine the start
    // of a payload by looking for the magic starting token.

    if (Serial1.available() >= strlen(ESP32_TOKEN_START) + sizeof(struct ESP32Packet))
    {

        b32 found_start_token = true;

        for (i32 i = 0; ESP32_TOKEN_START[i]; i += 1)
        {
            if (Serial1.peek() == ESP32_TOKEN_START[i])
            {
                Serial1.read(); // Matching so far...
            }
            else // Nope; not the starting token we're looking for.
            {

                if (i == 0)
                {
                    Serial1.read(); // Progress into the buffer at least once.
                }

                found_start_token = false;
                break;

            }
        }

        if (found_start_token)
        {

            struct ESP32Packet payload = {};

            Serial1.readBytes((char*) &payload, sizeof(payload));

            process_payload(&payload);

        }

    }



    // For testing purposes, the ESP32 will make up payloads
    // so an external MCU with UART connection isn't necessary.

    #if GENERATE_DUMMY_PACKETS
    {

        b32 make_dummy_packet = false;

        #if ESPNOW_ENABLE
        {

            // ESP-NOW will have higher throughput than LoRa,
            // so to prevent excessive overruns, we generate a
            // dummy payload whenever the ESP-NOW buffer can handle it.

            make_dummy_packet = packet_espnow_writer - packet_espnow_reader < countof(packet_espnow_buffer);
        }
        #elif LORA_ENABLE
        {
            make_dummy_packet = packet_lora_writer - packet_lora_reader < countof(packet_lora_buffer);
        }
        #endif

        if (make_dummy_packet)
        {
            struct ESP32Packet payload = {};

            payload.nonredundant.timestamp_ms = millis();
            payload.nonredundant.crc          = ESP32_calculate_crc((u8*) &payload, sizeof(payload) - sizeof(payload.nonredundant.crc));

            process_payload(&payload);
        }

    }
    #endif



    // See if there's an ESP32 packet to be sent and
    // that we can transmit one at all.

    #if ESPNOW_ENABLE
    {
        if (packet_espnow_reader != packet_espnow_writer && !packet_espnow_transmission_busy)
        {

            packet_espnow_transmission_busy = true;

            struct ESP32Packet* packet = &packet_espnow_buffer[packet_espnow_reader % countof(packet_espnow_buffer)];

            if (esp_now_send(MAIN_ESP32_MAC_ADDRESS, (u8*) packet, sizeof(*packet)) != ESP_OK)
            {
                Serial.printf("Send Error!\n");
                packet_espnow_transmission_busy = false;
                espnow_report_error();
            }

        }
    }
    #endif



    // See if there's an LoRa packet to be sent and
    // that we can transmit one at all.

    #if LORA_ENABLE
    {
        if (packet_lora_reader != packet_lora_writer && !packet_lora_transmission_busy)
        {

            packet_lora_transmission_busy = true;

            struct LoRaPacket* packet = &packet_lora_buffer[packet_lora_reader % countof(packet_lora_buffer)];

            //packet_lora_radio.startTransmit((u8*) packet, sizeof(*packet)); // duplicate start Transmit call?

            if (packet_lora_radio.startTransmit((u8*) packet, sizeof(*packet)) != RADIOLIB_ERR_NONE)
            {
            packet_lora_transmission_busy = false;
            lora_report_error();
            }

        }
    }
    #endif



    // Output statistics at regular intervals.

    static u32 last_statistic_timestamp_ms = 0;

    u32 current_timestamp_ms = millis();

    if (current_timestamp_ms - last_statistic_timestamp_ms >= 500)
    {

        last_statistic_timestamp_ms = current_timestamp_ms;

        Serial.printf("Last timestamp         : %d ms" "\n", last_uart_packet_timestamp_ms);
        Serial.printf("UART packets received  : %d"    "\n", packet_uart_packet_count);
        Serial.printf("UART CRC mismatches    : %d"    "\n", packet_uart_crc_error_count);

        #if ESPNOW_ENABLE
        {
            Serial.printf("ESP32 packets queued   : %d"    "\n", packet_espnow_packet_count);
            Serial.printf("ESP32 packets in queue : %d"    "\n", (u32) (packet_espnow_writer - packet_espnow_reader));
            Serial.printf("ESP32 packet overruns  : %d"    "\n", packet_espnow_overrun_count);
        }
        #endif

        #if LORA_ENABLE
        {
            Serial.printf("LoRa packets queued    : %d"    "\n", packet_lora_packet_count);
            Serial.printf("LoRa packets in queue  : %d"    "\n", (u32) (packet_lora_writer - packet_lora_reader));
            Serial.printf("LoRa packet overruns   : %d"    "\n", packet_lora_overrun_count);
        }
        #endif

        Serial.printf("\n");

    }

}



// TODO I think after a while, ESP-NOW packets stop being sent/received. Investigate.