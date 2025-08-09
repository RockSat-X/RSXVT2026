//////////////////////////////////////////////////////////////// Configurations ////////////////////////////////////////////////////////////////



/* #meta SYSTEM_OPTIONS, GPIOS, NVIC_TABLE : MK_GPIOS

    SYSTEM_OPTIONS = {

        'SandboxNucleoH7S3L8' : {
            'hsi_enable'    : True,
            'hsi48_enable'  : True,
            'csi_enable'    : True,
            'per_ck_source' : 'hsi_ck',
            'pll1_p_ck'     : 600_000_000,
            'pll2_s_ck'     : 200_000_000,
            'cpu_ck'        : 600_000_000,
            'axi_ahb_ck'    : 300_000_000,
            'apb1_ck'       : 150_000_000,
            'apb2_ck'       : 150_000_000,
            'apb4_ck'       : 150_000_000,
            'apb5_ck'       : 150_000_000,
        },

    }

    GPIOS = MK_GPIOS({

        'SandboxNucleoH7S3L8' : (
            ('led_red'   , 'B7' , 'output'    , { 'initlvl' : False }),
            ('led_yellow', 'D13', 'output'    , { 'initlvl' : False }),
            ('led_green' , 'D10', 'output'    , { 'initlvl' : False }),
            ('swdio'     , 'A13', 'reserved'  , {                   }),
            ('swclk'     , 'A14', 'reserved'  , {                   }),
        ),

    })

    NVIC_TABLE = {

        'SandboxNucleoH7S3L8' : (
            ('USART3', 0),
        ),

    }

*/



//////////////////////////////////////////////////////////////// Primitives ////////////////////////////////////////////////////////////////



#define false                   0
#define true                    1
#define STRINGIFY_(X)           #X
#define STRINGIFY(X)            STRINGIFY_(X)
#define CONCAT_(X, Y)           X##Y
#define CONCAT(X, Y)            CONCAT_(X, Y)
#define IS_POW_2(X)             ((X) > 0 && ((X) & ((X) - 1)) == 0)
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
#define static_assert(...)      _Static_assert(__VA_ARGS__)
#define static_assert_expr(...) ((void) sizeof(struct { static_assert(__VA_ARGS__); }))
#ifndef offsetof
#define offsetof __builtin_offsetof
#endif

#include "primitives.meta"
/* #meta

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
