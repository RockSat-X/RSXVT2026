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



#if TARGET_MCU_IS_STM32H7S3L8H6

    // For the full contiguous field. @/pg 2476/sec 53.8.6/`H7S3rm`.
    #define USART_BRR_BRR_Pos 0
    #define USART_BRR_BRR_Msk (0xFFFF << USART_BRR_BRR_Pos)

#endif



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



#define NVIC_ENABLE(NAME)        ((void) (NVIC->ISER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32))) // @/pg 628/sec B3.4.4/`Armv7-M`.
#define NVIC_DISABLE(NAME)       ((void) (NVIC->ICER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32))) // @/pg 628/sec B3.4.4/`Armv7-M`.
#define NVIC_SET_PENDING(NAME)   ((void) (NVIC->ISPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32))) // @/pg 629/sec B3.4.6/`Armv7-M`.
#define NVIC_CLEAR_PENDING(NAME) ((void) (NVIC->ICPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32))) // @/pg 630/sec B3.4.7/`Armv7-M`.



#include "interrupts.meta"
/* #meta INTERRUPTS, INTERRUPTS_FOR_FREERTOS : NVIC_TABLE



    # Parse the targets' CMSIS header files to get the list of interrupts on the microcontroller.

    INTERRUPTS = {}

    for target in TARGETS:



        # The CMSIS header for the microcontroller will define its interrupts with an enumeration;
        # we find the enumeration members so we can automatically get the list of interrupt names
        # and the corresponding numbers.
        # e.g:
        # >
        # >    typedef enum
        # >    {
        # >        Reset_IRQn          = -15,
        # >        NonMaskableInt_IRQn = -14,
        # >        ...
        # >        FDCAN2_IT0_IRQn     = 154,
        # >        FDCAN2_IT1_IRQn     = 155
        # >    } IRQn_Type;
        # >

        irqn_enumeration = {}

        for line in target.cmsis_file_path.read_text().splitlines():

            match line.split():

                case [interrupt_name, '=', interrupt_number, *_] if '_IRQn' in interrupt_name:

                    interrupt_name   = interrupt_name.removesuffix('_IRQn')
                    interrupt_number = int(interrupt_number.removesuffix(','))

                    assert interrupt_number not in irqn_enumeration
                    irqn_enumeration[interrupt_number] = interrupt_name



        # The first interrupt should be the reset exception defined by Armv7-M.
        # Note that the reset exception has an *exception number* of 1 (@/pg 525/tbl B1-4/`Armv7-M`),
        # but the *interrupt number* is defined to be the *exception number - 16* (@/pg 625/sec B3.4.1/`Armv7-M`).

        irqn_enumeration = sorted(irqn_enumeration.items())
        assert irqn_enumeration[0] == (-15, 'Reset')



        # We then omit the reset interrupt because we will typically handle it specially.

        irqn_enumeration = irqn_enumeration[1:]



        # To get a contigious sequence of interrupt names,
        # including interrupt numbers that are reserved,
        # we create a list for all interrupt numbers between the lowest and highest.

        INTERRUPTS[target.mcu] = [None] * (irqn_enumeration[-1][0] - irqn_enumeration[0][0] + 1)

        for interrupt_number, interrupt_name in irqn_enumeration:
            INTERRUPTS[target.mcu][interrupt_number - irqn_enumeration[0][0]] = interrupt_name

        INTERRUPTS[target.mcu] = tuple(INTERRUPTS[target.mcu])



    # Some interrupts will be hijacked by FreeRTOS.

    INTERRUPTS_FOR_FREERTOS = {

        'STM32H7S3L8H6' : {
            'SysTick' : 'xPortSysTickHandler',
            'SVCall'  : 'vPortSVCHandler'    ,
            'PendSV'  : 'xPortPendSVHandler' ,
        },

        'STM32H533RET6' : {
            'SysTick' : 'SysTick_Handler',
            'SVCall'  : 'SVC_Handler'    ,
            'PendSV'  : 'PendSV_Handler' ,
        },

    }



    # If trying to define an interrupt handler and one makes a typo,
    # then the function end up not replacing the weak symbol that's
    # in place of the interrupt handler in the interrupt vector table,
    # which can end up as a confusing bug. To prevent that, we will use a
    # macro to set up the function prototype of the interrupt handler,
    # and if a typo is made, then a compiler error will be generated.
    # e.g:
    # >
    # >    INTERRUPT(TIM13)          <- If "TIM13" interrupt exists, then ok!
    # >    {
    # >        ...
    # >    }
    # >
    # >    INTERRUPT(TIM14)          <- If "TIM14" interrupt doesn't exists, then compiler error here; good!
    # >    {
    # >        ...
    # >    }
    # >
    # >    extern void
    # >    __INTERRUPT_TIM14(void)   <- This will always compile, even if "TIM14" doesn't exist; bad!
    # >    {
    # >        ...
    # >    }
    # >

    @Meta.ifs(TARGETS.mcus, '#if')
    def _(mcu):

        yield f'TARGET_MCU_IS_{mcu}'

        for interrupt in ('Default',) + INTERRUPTS[mcu]:

            if interrupt is None:
                continue

            if interrupt in INTERRUPTS_FOR_FREERTOS:
                continue

            Meta.define('INTERRUPT', ('INTERRUPT'), f'extern void __INTERRUPT_{interrupt}(void)', INTERRUPT = interrupt)



    # Set up NVIC.

    @Meta.ifs(TARGETS, '#if')
    def _(target):

        yield f'TARGET_NAME_IS_{target.name}'



        # Enumeration to make the NVIC macros only
        # work on interrupts defined in NVIC_TABLE.

        Meta.enums(
            'NVICInterrupt',
            'u32',
            ((interrupt, f'{interrupt}_IRQn') for interrupt, niceness in NVIC_TABLE[target.name]),
        )



        # Initialize the priorities of the defined NVIC interrupts.

        with Meta.enter('''
            extern void
            NVIC_init(void)
        '''):

            for interrupt, niceness in NVIC_TABLE[target.name]:



                # We'll be safe and use the fact that Armv7-M supports
                # at least 3 bits for the interrupt priority starting MSb. @/pg 526/sec B1.5.4/`Armv7-M`.

                assert 0b000 <= niceness <= 0b111



                # Set priority of NVIC interrupt.

                Meta.line(f'NVIC->IPR[NVICInterrupt_{interrupt}] = {niceness} << 5;')

*/



//////////////////////////////////////////////////////////////// System Initialization ////////////////////////////////////////////////////////////////



#include "SYSTEM_init.meta"
/* #meta

    @Meta.ifs(TARGETS, '#if')
    def _(target):

        yield f'TARGET_NAME_IS_{target.name}'


        if target.mcu == 'STM32H533RET6':
            return # TODO.


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
