#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include <esp_wifi.h>



////////////////////////////////////////////////////////////////////////////////



#define countof(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))

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



struct Message
{
    u8 sequence_number;
    u8 payload[239];
} __attribute__((packed));

static_assert(sizeof(struct Message) == 240);
