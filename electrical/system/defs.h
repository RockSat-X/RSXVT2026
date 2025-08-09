#include "cmsis.c"

#include "INTERRUPT.meta"
/* #meta

    # @/`Enforcing existence of interrupt handlers`:

    @Meta.ifs(TARGETS.mcus, '#if')
    def _(mcu):

        yield f'TARGET_MCU_IS_{mcu}'

        for interrupt in USER_DEFINED_INTERRUPTS[mcu]:
            Meta.define('INTERRUPT', ('INTERRUPT'), f'extern void __INTERRUPT_{interrupt}(void)', INTERRUPT = interrupt)

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
