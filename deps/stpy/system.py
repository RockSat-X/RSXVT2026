#include "SYSTEM_init.meta"
#meta



# Macros to control the interrupt in NVIC.
# @/pg 626/tbl B3-8/`Armv7-M`.
# @/pg 1452/tbl D1.1.10/`Armv8-M`.

Meta.define('NVIC_ENABLE'       , ('NAME'), '((void) (NVIC->ISER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))')
Meta.define('NVIC_DISABLE'      , ('NAME'), '((void) (NVIC->ICER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))')
Meta.define('NVIC_SET_PENDING'  , ('NAME'), '((void) (NVIC->ISPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))')
Meta.define('NVIC_CLEAR_PENDING', ('NAME'), '((void) (NVIC->ICPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))')



# Macros to make using GPIOs in C easy.
# TODO Use SYSTEM_DATABASE.

Meta.define('GPIO_HIGH'  , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BS, _NUMBER_FOR_GPIO_WRITE(NAME))))')
Meta.define('GPIO_LOW'   , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BR, _NUMBER_FOR_GPIO_WRITE(NAME))))')
Meta.define('GPIO_TOGGLE', ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->ODR ^= CONCAT(GPIO_ODR_OD , _NUMBER_FOR_GPIO_WRITE(NAME))))')
Meta.define('GPIO_SET'   , ('NAME', 'VALUE'), '((void) ((VALUE) ? GPIO_HIGH(NAME) : GPIO_LOW(NAME)))')
Meta.define('GPIO_READ'  , ('NAME'         ), '(!!(CONCAT(GPIO, _PORT_FOR_GPIO_READ(NAME))->IDR & CONCAT(GPIO_IDR_ID, _NUMBER_FOR_GPIO_READ(NAME))))')



for target in PER_TARGET():

    # For interrupts (used by the target) that
    # are in NVIC, we create an enumeration so
    # that the user can only enable those specific
    # interrupts. Note that some interrupts, like
    # SysTick, are not a part of NVIC.

    Meta.enums(
        'NVICInterrupt',
        'u32',
        (
            (interrupt, f'{interrupt}_IRQn')
            for interrupt, niceness in target.interrupts
            if INTERRUPTS[target.mcu][interrupt] >= 0
        )
    )



# Initialize the target's GPIOs, interrupts, clock-tree, etc.

for target in PER_TARGET():

    with Meta.enter('''
        extern void
        SYSTEM_init(void)
    '''):



        ################################ Clock-Tree ################################



        # Figure out the register values relating to the clock-tree.

        configuration, tree = SYSTEM_PARAMETERIZE(target)



        # Figure out the procedure to set the register values for the clock-tree.

        SYSTEM_CONFIGURIZE(target, configuration)



        # Export the frequencies we found for the clock-tree.

        put_title('Clock-Tree')

        for macro, expansion in justify(
            (
                ('<', f'CLOCK_TREE_FREQUENCY_OF_{name}' ),
                ('>', f'{frequency:,}'.replace(',', "'")),
            )
            for name, frequency in tree
            if name is not None
        ):
            Meta.define(macro, f'({expansion})')
