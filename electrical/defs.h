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

//////////////////////////////////////////////////////////////// CMSIS ////////////////////////////////////////////////////////////////

#define _PARENS                                     ()
#define _EXPAND_0(...)                              _EXPAND_1(_EXPAND_1(_EXPAND_1(__VA_ARGS__)))
#define _EXPAND_1(...)                              _EXPAND_2(_EXPAND_2(_EXPAND_2(__VA_ARGS__)))
#define _EXPAND_2(...)                              __VA_ARGS__
#define _MAP__(F, X, A, B, ...)                     F(X, A, B) __VA_OPT__(_MAP_ _PARENS (F, X, __VA_ARGS__))
#define _MAP_()                                     _MAP__
#define _MAP(F, X, ...)                             __VA_OPT__(_EXPAND_0(_MAP__(F, X, __VA_ARGS__)))
#define _ZEROING(SECT_REG, FIELD, VALUE)            | CONCAT(CONCAT(SECT_REG##_, FIELD##_), Msk)
#define _SETTING(SECT_REG, FIELD, VALUE)            | (((VALUE) << CONCAT(CONCAT(SECT_REG##_, FIELD##_), Pos)) & CONCAT(CONCAT(SECT_REG##_, FIELD##_), Msk))
#define CMSIS_READ(SECT_REG, VAR, FIELD)            ((u32) (((VAR) & CONCAT(CONCAT(CONCAT(SECT_REG, _), FIELD##_), Msk)) >> CONCAT(CONCAT(CONCAT(SECT_REG, _), FIELD##_), Pos)))
#define CMSIS_WRITE(SECT_REG, VAR, FIELD_VALUES...) ((void) ((VAR) = ((VAR) & ~(0 _MAP(_ZEROING, SECT_REG, FIELD_VALUES))) _MAP(_SETTING, SECT_REG, FIELD_VALUES)))
#define CMSIS_SET(SECT, REG, FIELD_VALUES...)       CMSIS_WRITE(CONCAT(SECT##_, REG), SECT->REG, FIELD_VALUES)
#define CMSIS_GET(SECT, REG, FIELD)                 CMSIS_READ (CONCAT(SECT##_, REG), SECT->REG, FIELD       )
#define CMSIS_GET_FROM(VAR, SECT, REG, FIELD)       CMSIS_READ (CONCAT(SECT##_, REG), VAR      , FIELD       )

struct CMSISPutTuple
{
    volatile uint32_t* dst;
    i32                pos;
    u32                msk;
};

static mustinline void
CMSIS_PUT(struct CMSISPutTuple tuple, u32 value)
{
    *tuple.dst = ((value) << tuple.pos) & tuple.msk;
}

//////////////////////////////////////////////////////////////// Globals ////////////////////////////////////////////////////////////////

static volatile u32 systick_ms = 0;
