//////////////////////////////////////////////////////////////// CMSIS Macros ////////////////////////////////////////////////////////////////



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



//////////////////////////////////////////////////////////////// CMSIS Patches ////////////////////////////////////////////////////////////////



#include "cmsis_patches.meta"
/* #meta

    # Define macros to map things like "I2C1_" to "I2C_".
    # This makes using CMSIS_SET/GET a whole lot easier
    # because CMSIS definitions like "I2C_CR1_PE_Pos"
    # do not include the peripheral's suffix (the "1" in "I2C1"),
    # so we need a way to drop the suffix, and this is how we do it.

    import string

    for peripheral in ('UART', 'USART', 'SPI', 'XSPI', 'I2C', 'I3C', 'DMA', 'DMAMUX', 'SDMMC', 'TIM', 'GPIO'):



        # Determine the suffixes.

        match peripheral:
            case 'GPIO' : units = string.ascii_uppercase
            case 'TIM'  : units = range(32)
            case _      : units = range(8)

        units = list(units)



        # Some peripherals furthermore have subunits that also need this suffix-chopping to be done too.
        # e.g.
        # "DMAMUX5_RequestGenerator7_" -> "DMAMUX_"
        # "DMAMUX6_RequestGenerator0_" -> "DMAMUX_"
        # "DMAMUX6_RequestGenerator1_" -> "DMAMUX_"

        PERIPHERAL_WITH_SUBUNITS = (
            ('DMA'   , '{unit}_Stream{subunit}'          ),
            ('DMAMUX', '{unit}_Channel{subunit}'         ),
            ('DMAMUX', '{unit}_RequestGenerator{subunit}'),
        )

        units += [
            template.format(unit = unit, subunit = subunit)
            for peripheral_with_subunits, template in PERIPHERAL_WITH_SUBUNITS
            if peripheral == peripheral_with_subunits
            for unit in units
            for subunit in range(8)
        ]



        # Output the macro substitutions.

        for suffix in units:
            Meta.define(f'{peripheral}{suffix}_', f'{peripheral}_')
*/



//////////////////////////////////////////////////////////////// Interrupts ////////////////////////////////////////////////////////////////



#include "INTERRUPT.meta"
/* #meta

    # @/`Enforcing existence of interrupt handlers`:

    @Meta.ifs(TARGETS.mcus, '#if')
    def _(mcu):

        yield f'TARGET_MCU_IS_{mcu}'

        for interrupt in USER_DEFINED_INTERRUPTS[mcu]:
            Meta.define('INTERRUPT', ('INTERRUPT'), f'extern void __INTERRUPT_{interrupt}(void)', INTERRUPT = interrupt)

*/



//////////////////////////////////////////////////////////////// System Initialization ////////////////////////////////////////////////////////////////



#include "SYSTEM_init.meta"
/* #meta

    @Meta.ifs(TARGETS, '#if')
    def _(target):

        yield f'TARGET_NAME_IS_{target.name}'



        # Output the system initialization routine where we configure the low-level MCU stuff.

        with Meta.enter('''
            extern void
            SYSTEM_init(void)
        '''):



            # Figure out the register values.

            configuration, defines = SYSTEM_PARAMETERIZE(target, SYSTEM_OPTIONS[target.name])



            # Figure out the procedure to set the register values.

            SYSTEM_CONFIGURIZE(target, configuration)



        # We also make defines so that the C code can use the results
        # of the parameterization and configuration procedure (e.g. the resulting CPU frequency).

        for name, expansion in defines:
            Meta.define(name, expansion)
*/



//////////////////////////////////////////////////////////////// Notes ////////////////////////////////////////////////////////////////

// @/`Enforcing existence of interrupt handlers`:
//
// If trying to define an interrupt handler and one makes a typo,
// then the function end up not replacing the weak symbol that's
// in place of the interrupt handler in the interrupt vector table,
// which can end up as a confusing bug. To prevent that, we will use a
// macro to set up the function prototype of the interrupt handler,
// and if a typo is made, then a compiler error will be generated.
// e.g:
// >
// >    INTERRUPT(TIM13)   <- If "TIM13" interrupt exists, then ok!
// >    {
// >        ...
// >    }
// >
// >    INTERRUPT(TIM14)   <- If "TIM14" interrupt doesn't exists, then compiler error here; good!
// >    {
// >        ...
// >    }
// >
// >    extern void
// >    __INTERRUPT_TIM14(void)   <- This will always compile, even if "TIM14" doesn't exist; bad!
// >    {
// >        ...
// >    }
// >
