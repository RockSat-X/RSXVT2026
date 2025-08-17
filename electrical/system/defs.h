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

#if TARGET_MCU_IS_STM32H533RET6

    #define RCC_CCIPR5_CKPERSEL_ RCC_CCIPR5_CKERPSEL_ // Typo.

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



#include "interrupts.meta"
/* #meta INTERRUPTS



    # Parse the CMSIS header files to get the list of interrupts on the microcontroller.

    INTERRUPTS = {}

    for mcu in MCUS:



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

        for line in MCUS[mcu].cmsis_file_path.read_text().splitlines():

            match line.split():

                case [interrupt_name, '=', interrupt_number, *_] if '_IRQn' in interrupt_name:

                    interrupt_name   = interrupt_name.removesuffix('_IRQn')
                    interrupt_number = int(interrupt_number.removesuffix(','))

                    assert interrupt_number not in irqn_enumeration
                    irqn_enumeration[interrupt_number] = interrupt_name



        # The first interrupt should be the reset exception defined by Armv7-M/Armv8-M.
        # Note that the reset exception has an (exception number) of 1,
        # but the *interrupt number* is defined to be the (exception number - 16).
        # @/pg 525/tbl B1-4/`Armv7-M`.   @/pg 143/sec B3.30/`Armv8-M`.
        # @/pg 625/sec B3.4.1/`Armv7-M`. @/pg 1855/sec D1.2.236/`Armv8-M`.

        irqn_enumeration = sorted(irqn_enumeration.items())
        assert irqn_enumeration[0] == (-15, 'Reset')



        # We then omit the reset interrupt because we will typically handle it specially.

        irqn_enumeration = irqn_enumeration[1:]



        # To get a contigious sequence of interrupt names,
        # including interrupt numbers that are reserved,
        # we create a list for all interrupt numbers between the lowest and highest.

        INTERRUPTS[mcu] = [None] * (irqn_enumeration[-1][0] - irqn_enumeration[0][0] + 1)

        for interrupt_number, interrupt_name in irqn_enumeration:
            INTERRUPTS[mcu][interrupt_number - irqn_enumeration[0][0]] = interrupt_name

        INTERRUPTS[mcu] = tuple(INTERRUPTS[mcu])



    # If trying to define an interrupt handler and one makes a typo,
    # then the function end up not replacing the weak symbol that's
    # in place of the interrupt handler in the interrupt vector table,
    # which can end up as a confusing bug. To prevent that, we will use a
    # macro to set up the function prototype of the interrupt handler,
    # and if a typo is made, then a compiler error will be generated.
    # e.g:
    # >
    # >    INTERRUPT_TIM13           <- If "TIM13" interrupt exists, then ok!
    # >    {
    # >        ...
    # >    }
    # >
    # >    INTERRUPT_TIM14           <- If "TIM14" interrupt doesn't exists, then compiler error here; good!
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

    @Meta.ifs(TARGETS, '#if')
    def _(target):

        yield f'TARGET_NAME_IS_{target.name}'

        for interrupt in ('Default',) + INTERRUPTS[target.mcu]:

            if interrupt is None:
                # This interrupt is reserved.
                continue

            if interrupt in MCUS[target.mcu].freertos_interrupts and 'systick_ck' not in target.clock_tree:
                # This interrupt will be supplied by FreeRTOS,
                # unless we're configuring SysTick which FreeRTOS uses by default,
                # to which we won't be considering FreeRTOS at all.
                continue

            Meta.define(f'INTERRUPT_{interrupt}', f'extern void __INTERRUPT_{interrupt}(void)')

*/



//////////////////////////////////////////////////////////////// System Initialization ////////////////////////////////////////////////////////////////



#include "SYSTEM_init.meta"
/* #meta



    # Macros to make using GPIOs in C easy. TODO Use SYSTEM_DATABASE.

    Meta.define('GPIO_HIGH'  , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BS, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_LOW'   , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BR, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_TOGGLE', ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->ODR ^= CONCAT(GPIO_ODR_OD , _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_SET'   , ('NAME', 'VALUE'), '((void) ((VALUE) ? GPIO_HIGH(NAME) : GPIO_LOW(NAME)))')
    Meta.define('GPIO_READ'  , ('NAME'         ), '(!!(CONCAT(GPIO, _PORT_FOR_GPIO_READ(NAME))->IDR & CONCAT(GPIO_IDR_ID, _NUMBER_FOR_GPIO_READ(NAME))))')



    # Initialize the target's GPIOs, interrupts, clock-tree, etc.

    @Meta.ifs(TARGETS, '#if')
    def _(target):

        yield f'TARGET_NAME_IS_{target.name}'

        with Meta.enter('''
            extern void
            SYSTEM_init(void)
        '''):



            ################################ GPIOs ################################

            put_title('GPIOs')

            gpios = PROCESS_GPIOS(target)



            # Macros to make GPIOs easy to use.

            for gpio in gpios:

                if gpio.pin is None:
                    continue

                if gpio.mode in ('input', 'alternate'):
                    Meta.define('_PORT_FOR_GPIO_READ'  , ('NAME'), gpio.port  , NAME = gpio.name)
                    Meta.define('_NUMBER_FOR_GPIO_READ', ('NAME'), gpio.number, NAME = gpio.name)

                if gpio.mode == 'output':
                    Meta.define('_PORT_FOR_GPIO_WRITE'  , ('NAME'), gpio.port  , NAME = gpio.name)
                    Meta.define('_NUMBER_FOR_GPIO_WRITE', ('NAME'), gpio.number, NAME = gpio.name)



            # Enable GPIO ports that have defined pins.

            CMSIS_SET(
                ('RCC', SYSTEM_DATABASE[target.mcu]['GPIO_PORT_ENABLE_REGISTER'].value, f'GPIO{port}EN', True)
                for port in sorted(OrderedSet(gpio.port for gpio in gpios if gpio.pin is not None))
            )



            # Set output type (push-pull/open-drain).

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'OTYPER', f'OT{gpio.number}', gpio.open_drain)
                for gpio in gpios
                if gpio.pin is not None and gpio.open_drain is not None
            )



            # Set initial output level.

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'ODR', f'OD{gpio.number}', gpio.initlvl)
                for gpio in gpios
                if gpio.pin is not None and gpio.initlvl is not None
            )



            # Set drive strength.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'OSPEEDR',
                    f'OSPEED{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_SPEED'].value)[gpio.speed]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.speed is not None
            )



            # Set pull configuration.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'PUPDR',
                    f'PUPD{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_PULL'].value)[gpio.pull]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.pull is not None
            )



            # Set alternative function; must be done before setting pin mode
            # so that the alternate function pin will start off properly.

            CMSIS_WRITE(
                (
                    f'GPIO_AFR{('L', 'H')[gpio.number // 8]}',
                    f'GPIO{gpio.port}->AFR[{gpio.number // 8}]',
                    f'AFSEL{gpio.number}',
                    gpio.afsel
                )
                for gpio in gpios
                if gpio.afsel is not None
            )



            # Set pin mode.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'MODER',
                    f'MODE{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_MODE'].value)[gpio.mode]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.mode not in (None, 'reserved')
            )



            ################################ NVIC ################################

            put_title('NVIC')



            # Make macros to control the interrupt in NVIC.
            # @/pg 626/tbl B3-8/`Armv7-M`.
            # @/pg 1452/tbl D1.1.10/`Armv8-M`.

            for interrupt, niceness in target.interrupt_priorities:
                for macro, register in (
                    ('NVIC_ENABLE'       , 'ISER'),
                    ('NVIC_DISABLE'      , 'ICER'),
                    ('NVIC_SET_PENDING'  , 'ISPR'),
                    ('NVIC_CLEAR_PENDING', 'ICPR'),
                ):
                    Meta.define(
                        macro, ('INTERRUPT'),
                        f'((void) (NVIC->{register}[{interrupt}_IRQn / 32] = 1 << ({interrupt}_IRQn % 32)))',
                        INTERRUPT = interrupt
                    )



            # Set the priorities of the defined NVIC interrupts.
            # Note that the Armv7-M architecture guarantees minimum of 3 priority bits,
            # while the Armv8-M guarantees minimum of 2; for now, we'll appeal
            # to the lowest common denominator here.
            # @/pg 526/sec B1.5.4/`Armv7-M`.
            # @/pg 86/sec B3.9/`Armv8-M`.

            for interrupt, niceness in target.interrupt_priorities:
                assert 0b00 <= niceness <= 0b11
                Meta.line(f'NVIC->IPR[{interrupt}_IRQn] = {niceness} << 6;')



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

*/



//////////////////////////////////////////////////////////////// GPIO Construction ////////////////////////////////////////////////////////////////



/* #meta PROCESS_GPIOS

    import csv



    # We create a table to map GPIO pin and alternate function names to alternate function codes.
    # e.g:
    # >
    # >    ('A', 0, 'UART4_TX')   ->   0b1000
    # >

    GPIO_AFSEL = {}

    for mcu in MCUS:

        GPIO_AFSEL[mcu] = []



        # Find the CSV file that STM32CubeMX can provide to get all of the alternate functions for GPIOs.

        gpio_afsel_file_path = root(f'./deps/mcu/{mcu}_gpios.csv')

        if not gpio_afsel_file_path.is_file():
            raise RuntimeError(
                f'File "{gpio_afsel_file_path}" does not exist; '
                f'use STM32CubeMX to generate the CSV file (also clear the pinout!).'
            )



        # Process the CSV file.

        for entry in csv.DictReader(gpio_afsel_file_path.read_text().splitlines()):

            match entry['Type']:



                # Most GPIOs we care about are the I/O ones.

                case 'I/O':

                    # Some GPIO names are suffixed with additional things, like for instance:
                    #     PC14-OSC32_IN(OSC32_IN)
                    #     PH1-OSC_OUT(PH1)
                    #     PA14(JTCK/SWCLK)
                    #     PC2_C
                    # So we need to format it slightly so that we just get the port letter and pin number.

                    pin    = entry['Name']
                    pin    = pin.split('-', 1)[0]
                    pin    = pin.split('(', 1)[0]
                    pin    = pin.split('_', 1)[0]
                    port   = pin[1]
                    number = int(pin[2:])

                    assert pin.startswith('P') and ('A' <= port <= 'Z')



                    # Gather all the alternate functions of the GPIO, if any.

                    for afsel_code in range(16):

                        if not entry[f'AF{afsel_code}']: # Skip empty cells.
                            continue

                        GPIO_AFSEL[mcu] += [
                            ((port, number, afsel_name), afsel_code)
                            for afsel_name in entry[f'AF{afsel_code}'].split('/') # Some have multiple names for the same AFSEL code (e.g. "I2S3_CK/SPI3_SCK").
                        ]




                # TODO Maybe use this information to ensure the PCB footprint is correct?

                case 'Power' | 'Reset' | 'Boot':
                    pass



                # TODO I have no idea what this is.

                case 'MonoIO':
                    pass



                # Unknown GPIO type in the CSV; doesn't neccessarily mean an error though.

                case _:
                    pass # TODO Warn.



        # Done processing the CSV file; now we have a mapping of GPIO pin
        # and alternate function name to the alternate function code.

        GPIO_AFSEL[mcu] = mk_dict(GPIO_AFSEL[mcu])



    # The routine to create a single GPIO instance.

    def mk_gpio(target, entry):

        name, pin, mode, properties = entry



        # The layout of a GPIO instance.

        gpio = types.SimpleNamespace(
            name       = name,
            pin        = pin,
            mode       = mode,
            port       = None,
            number     = None,
            speed      = None,
            pull       = None,
            open_drain = None,
            initlvl    = None,
            altfunc    = None,
            afsel      = None,
        )



        # If the pin of the GPIO is given, split it into its port and number parts (e.g. 'A10' -> ('A', 10)).
        # The pin can be left unspecified as `None`, which is useful for when we don't know where
        # the pin will be end up being at but we want to at least have it still be in the table.

        if pin is not None:
            gpio.port   = pin[0]
            gpio.number = int(pin[1:])



        # Handle the GPIO mode.

        match mode:



            # A simple input GPIO to read digital voltage levels.

            case 'input':

                # The pull-direction must be specified in order to prevent accidentally defining a floating GPIO pin.
                gpio.pull = properties.pop('pull')



            # A simple output GPIO that can be driven low or high.

            case 'output':

                # The initial voltage level must be specified so the user remembers to take it into consideration.
                gpio.initlvl = properties.pop('initlvl')

                # Other optional properties.
                gpio.speed      = properties.pop('speed'     , None)
                gpio.open_drain = properties.pop('open_drain', None)




            # This GPIO would typically be used for some peripheral functionality (e.g. SPI clock output).

            case 'alternate':

                # Alternate GPIOs are denoted by a string like "UART8_TX" to indicate its alternate function.
                gpio.altfunc = properties.pop('altfunc')

                # Other optional properties.
                gpio.speed      = properties.pop('speed'     , None)
                gpio.pull       = properties.pop('pull'      , None)
                gpio.open_drain = properties.pop('open_drain', None)



            # An analog GPIO would have its Schmit trigger function disabled;
            # this obviously allows for ADC/DAC usage, but it can also serve as a power-saving measure.

            case 'analog':
                raise NotImplementedError



            # A GPIO that's marked as "reserved" is often useful for marking a particular pin as something
            # that shouldn't be used because it has an important functionality (e.g. JTAG debug).

            case 'reserved':
                properties = {} # Don't care what properties this reserved GPIO has.



            # Unknown GPIO mode.

            case unknown:
                raise ValueError(f'GPIO "{name}" has unknown mode: {repr(unknown)}.')



        # Determine the GPIO's alternate function code.

        if gpio.pin is not None and gpio.altfunc is not None:

            gpio.afsel = GPIO_AFSEL[target.mcu].get((gpio.port, gpio.number, gpio.altfunc), None)

            if gpio.afsel is None:
                raise ValueError(
                    f'GPIO pin {repr(gpio.pin)} for {target.mcu} ({target.name}) '
                    'has no alternate function {repr(gpio.altfunc)}.'
                )



        # Done processing this GPIO entry!

        if properties:
            raise ValueError(f'GPIO "{name}" has leftover properties: {properties}.')

        return gpio



    # The routine to ensure the list of GPIOs make sense for the target.

    def PROCESS_GPIOS(target):

        gpios = tuple(mk_gpio(target, entry) for entry in target.gpios)

        if (name := find_dupe(gpio.name for gpio in gpios)) is not ...:
            raise ValueError(f'GPIO name {repr(name)} used more than once.')

        if (pin := find_dupe(gpio.pin for gpio in gpios if gpio.pin is not None)) is not ...:
            raise ValueError(f'GPIO pin {repr(pin)} used more than once.')

        return gpios

*/



//////////////////////////////////////////////////////////////// Low-Level Routines ////////////////////////////////////////////////////////////////



#define nop() __asm__("nop")



static void
spinlock_nop(u32 count)
{
    // This is unrolled so that the branch penalty will be reduced.
    // Otherwise, the amount of delay that this procedure will produce
    // will be more inconsistent (potentially due to flash cache reasons?).
    for (u32 nop = 0; nop < count; nop += 256)
    {
        #define NOP4   nop(); nop(); nop(); nop();
        #define NOP16  NOP4   NOP4   NOP4   NOP4
        #define NOP64  NOP16  NOP16  NOP16  NOP16
        #define NOP256 NOP64  NOP64  NOP64  NOP64
        NOP256
        #undef NOP4
        #undef NOP16
        #undef NOP64
        #undef NOP256
    }
}



#if false

    static volatile u32 epoch_ms = 0;

    INTERRUPT_SysTick
    {
        epoch_ms += 1;
    }

    static void
    spinlock_ms(u32 duration_ms)
    {
        u32 start_ms = epoch_ms;
        while (epoch_ms - start_ms < duration_ms);
    }

#endif



#define sorry halt_(false); // @/`Halting`.
#define panic halt_(true)   // "
static noret void           // "
halt_(b32 panicking)        // "
{
    __disable_irq();


    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        if (panicking)
        {
            GPIO_HIGH(led_red);
        }
        else
        {
            GPIO_HIGH(led_yellow);
        }

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 8; j += 1)
                {
                    spinlock_nop(i);

                    if (panicking)
                    {
                        GPIO_TOGGLE(led_red);
                    }
                    else
                    {
                        GPIO_TOGGLE(led_yellow);
                    }
                }
            }
        }

    #endif

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 4; j += 1)
                {
                    GPIO_HIGH(led_green);
                    spinlock_nop(i);

                    GPIO_LOW(led_green);
                    spinlock_nop(i * (panicking ? 1 : 4));
                }
            }
        }

    #endif

    for(;;); // Panic! Something horrible has happened!
}



//////////////////////////////////////////////////////////////// Notes /////////////////////////////////////////////////////////////////



// This file (and this includes the entire folder) contains
// definitions for stuff that's quite low-level and technical.
// Most of it is just infrastructure to make programming the
// higher-level stuff on STM32s very pain-free and fun.



// @/`Halting`:
//
//
//
// A `sorry` is used for code-paths that haven't been implemented yet.
// Eventually, when we want things to be production-ready, we replace
// all `sorry`s that we can with proper code, or at the very least, a `panic`.
// e.g:
// >
// >    if (is_sd_card_mounted_yet())
// >    {
// >        do_stuff();
// >    }
// >    else
// >    {
// >        sorry   <- Note that `sorry` is specifically defined to be able to
// >                   be written without the terminating semicolon. This is
// >                   purely aesthetic; it makes `sorry` look very out-of-place.
// >    }
// >
//
//
//
// A `panic` is for absolute irrecoverable error conditions,
// like stack overflows or a function given ill-formed parameters.
// In other words, stuff like: "something horrible happened.
// I don't know how we got here. We need to reset!".
// It is useful to make this distinction from `sorry` because we can
// scan through the code-base and see what stuff is work-in-progress or just
// an irrecoverable error condition.
// e.g:
// >
// >    switch (option)
// >    {
// >        case Option_A: ...
// >        case Option_B: ...
// >        case Option_C: ...
// >        default: panic;     <- The compiler will enforce switch-statements to be exhaustive
// >                               for enumerations. The compiler will also enforce all
// >                               switch-statements to have a `default` case even if we are
// >                               exhaustive. In the event that the enumeration `option` is somehow
// >                               not any of the valid values, we will trigger a `panic` rather than
// >                               have the switch-statement silently pass.
// >    }
// >
//
//
//
// When a `sorry` or `panic` is triggered during deployment, the microcontroller will undergo a reset
// through the watchdog timer (TODO). During development, however, we'd like for the controller to actually
// stop dead in its track (rather than reset) so that we can debug. To make it even more useful,
// the microcontroller can also blink an LED indicating whether or not a `panic` or a `sorry` condition has
// occured; how this is implemented, if at all, is entirely dependent upon the target.
