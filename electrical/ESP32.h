#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <RadioLib.h>



////////////////////////////////////////////////////////////////////////////////



#define countof(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define pack_push    _Pragma("pack(push, 1)")
#define pack_pop     _Pragma("pack(pop)")

typedef unsigned char      u8;  static_assert(sizeof(u8 ) == 1);
typedef unsigned short     u16; static_assert(sizeof(u16) == 2);
typedef unsigned           u32; static_assert(sizeof(u32) == 4);
typedef unsigned long long u64; static_assert(sizeof(u64) == 8);
typedef signed   char      i8;  static_assert(sizeof(i8 ) == 1);
typedef signed   short     i16; static_assert(sizeof(i16) == 2);
typedef signed             i32; static_assert(sizeof(i32) == 4);
typedef signed   long long i64; static_assert(sizeof(i64) == 8);
typedef signed   char      b8;  static_assert(sizeof(b8 ) == 1);
typedef signed   short     b16; static_assert(sizeof(b16) == 2);
typedef signed             b32; static_assert(sizeof(b32) == 4);
typedef signed   long long b64; static_assert(sizeof(b64) == 8);
typedef float              f32; static_assert(sizeof(f32) == 4);
typedef double             f64; static_assert(sizeof(f64) == 8);



////////////////////////////////////////////////////////////////////////////////



#define PACKET_ESP32_START_TOKEN 0xBABE



pack_push

    struct PacketLoRa
    {
        u16 sequence_number; // When going from FC to ESP32, is set to `PACKET_ESP32_START_TOKEN`.
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

static_assert(sizeof(struct PacketESP32) <= 250);



////////////////////////////////////////////////////////////////////////////////



// TODO Look more into the specs.
// TODO Make robust.
extern void
common_init_esp_now(void)
{

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.printf("Error initializing ESP-NOW.\n");
        ESP.restart();
        return;
    }

}



////////////////////////////////////////////////////////////////////////////////



static SX1262 packet_lora_radio = new Module(41, 39, 42, 40);



// TODO Look more into the specs.
// TODO Make robust.
extern void
common_init_lora()
{

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

    extern void packet_lora_callback(void);

    packet_lora_radio.setDio1Action(packet_lora_callback);

}
