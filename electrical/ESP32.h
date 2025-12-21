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



pack_push

    struct PacketLoRa
    {
        u16 sequence_number;
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
        u16 sequence_number;
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
        f32 magnetometer_x;
        f32 magnetometer_y;
        f32 magnetometer_z;
        u8  image_chunk[190];
    };

pack_pop

static_assert(sizeof(struct PacketESP32) == 250);
