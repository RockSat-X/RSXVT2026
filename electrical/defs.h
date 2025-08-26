//////////////////////////////////////////////////////////////// Primitives ////////////////////////////////////////////////////////////////



#define false                   0
#define true                    1
#define STRINGIFY_(X)           #X
#define STRINGIFY(X)            STRINGIFY_(X)
#define CONCAT_(X, Y)           X##Y
#define CONCAT(X, Y)            CONCAT_(X, Y)
#define IS_POWER_OF_TWO(X)      ((X) > 0 && ((X) & ((X) - 1)) == 0)
#define sizeof(...)             ((signed) sizeof(__VA_ARGS__))
#define countof(...)            (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define bitsof(...)             (sizeof(__VA_ARGS__) * 8)
#define implies(P, Q)           (!(P) || (Q))
#define iff(P, Q)               (!!(P) == !!(Q))
#define useret                  __attribute__((warn_unused_result))
#define mustinline              __attribute__((always_inline)) inline
#define noret                   __attribute__((noreturn))
#define fallthrough             __attribute__((fallthrough))
#define pack_push               _Pragma("pack(push, 1)")
#define pack_pop                _Pragma("pack(pop)")
#define memeq(X, Y)             (static_assert_expr(sizeof(X) == sizeof(Y)), !memcmp(&(X), &(Y), sizeof(Y)))
#define memzero(X)              memset((X), 0, sizeof(*(X)))
#define static_assert(...)      _Static_assert(__VA_ARGS__, #__VA_ARGS__)
#define static_assert_expr(...) ((void) sizeof(struct { static_assert(__VA_ARGS__, #__VA_ARGS__); }))
#ifndef offsetof
#define offsetof __builtin_offsetof
#endif

#include "primitives.meta"
/* #meta

    # We could just use the definitions from <stdint.h>,
    # but they won't necessarily match up well
    # with `printf` well at all. For example, we'd like
    # for the "%d" specifier to work with `i32`

    for name, underlying, size in (
        ('u8'  , 'unsigned char'     , 1),
        ('u16' , 'unsigned short'    , 2),
        ('u32' , 'unsigned'          , 4),
        ('u64' , 'unsigned long long', 8),
        ('i8'  , 'signed char'       , 1),
        ('i16' , 'signed short'      , 2),
        ('i32' , 'signed'            , 4),
        ('i64' , 'signed long long'  , 8),
        ('b8'  , 'signed char'       , 1),
        ('b16' , 'signed short'      , 2),
        ('b32' , 'signed'            , 4),
        ('b64' , 'signed long long'  , 8),
        ('f32' , 'float'             , 4),
        ('f64' , 'double'            , 8),
    ):
        Meta.line(f'''
            typedef {underlying} {name};
            static_assert(sizeof({name}) == {size});
        ''')

*/



//////////////////////////////////////////////////////////////// Miscellaneous ////////////////////////////////////////////////////////////////



#include "system/defs.h"



//////////////////////////////////////////////////////////////// jig.c ////////////////////////////////////////////////////////////////



struct Jig
{
    volatile u32  reception_reader;
    volatile u32  reception_writer;
    volatile char reception_buffer[1 << 5];

    #if TARGET_USES_FREERTOS
        StaticSemaphore_t transmission_mutex_data;
        SemaphoreHandle_t transmission_mutex;
        StaticSemaphore_t reception_mutex_data;
        SemaphoreHandle_t reception_mutex;
    #endif
};



//////////////////////////////////////////////////////////////// Notes ////////////////////////////////////////////////////////////////



// This header file is where the majority of all definitions, across all targets, should be placed.
//
// The way compilation works for this project is that we do a unity build.
// This means rather than have a bunch of `.h` and `.c` files that are compiled to create object files that we then all link together,
// we rather just have one big thing that we just compile as an entire program (although we do have some smaller object files we
// nonetheless link to for technical reasons, see "Startup.S").
//
// The benefit of this approach is that things are much, much simpler. No more forward declarations to do. Just define the function
// and types somewhere and it'll work out. Making changes is much easier, and the overall compilation is straight-forward.
// Yes, I understand the idea behind the parallelism of compilation and incremental builds, but I personally believe these techniques
// and tools are completely misused, especially in an educational context like this.
// I'd go on a rant here, but I'll save it for another day.
//
// The only tricky thing to look out for a build like this is that we're always building for multiple targets.
// That is, we'll develop firmware for multiple flight computers, camera subsystems, even small sandboxes.
// Thus, if someone introduces a refactor or a half-baked feature for a particular target, it'll technically affect ALL targets.
// For example, if someone refactored the SPI driver while working on the flight computer MCU, then without any `#if` to do otherwise,
// this refactored SPI driver will also be in the camera subsystem MCU, which is untested for the new driver.
//
// Overall, this is just the nature of things. We'll be implementing a lot of code from the ground up,
// and in doing so, we'll be doing a lot of tests to ensure quality!
