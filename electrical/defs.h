//////////////////////////////////////////////////////////////// Configurations ////////////////////////////////////////////////////////////////

/* #meta SYSTEM_OPTIONS

    SYSTEM_OPTIONS = Obj(
        SandboxNucleoH7S3L8 = {
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
            'systick_ck'    :       1_000,
        },
    )

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
/* #meta PRIMITIVE_TYPES
    PRIMITIVE_TYPES = Table(
        ('name', 'underlying'        , 'size', 'minmax'),
        ('u8'  , 'unsigned char'     , 1     , True    ),
        ('u16' , 'unsigned short'    , 2     , True    ),
        ('u32' , 'unsigned'          , 4     , True    ),
        ('u64' , 'unsigned long long', 8     , True    ),
        ('i8'  , 'signed char'       , 1     , True    ),
        ('i16' , 'signed short'      , 2     , True    ),
        ('i32' , 'signed'            , 4     , True    ),
        ('i64' , 'signed long long'  , 8     , True    ),
        ('b8'  , 'signed char'       , 1     , False   ),
        ('b16' , 'signed short'      , 2     , False   ),
        ('b32' , 'signed'            , 4     , False   ),
        ('b64' , 'signed long long'  , 8     , False   ),
        ('f32' , 'float'             , 4     , True    ),
        ('f64' , 'double'            , 8     , True    ),
    )

    for type in PRIMITIVE_TYPES:
        Meta.line(f'''
            typedef {type.underlying} {type.name};
            static_assert(sizeof({type.name}) == {type.size});
        ''')
*/
