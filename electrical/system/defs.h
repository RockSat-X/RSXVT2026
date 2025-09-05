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



//////////////////////////////////////////////////////////////// CMSIS ////////////////////////////////////////////////////////////////



//
// Helpers. @/url:`https://github.com/PhucXDoan/phucxdoan.github.io/wiki/Macros-for-Reading-and-Writing-to-Memory%E2%80%90Mapped-Registers`.
//

#define _PARENTHESES                                 ()
#define _EXPAND_0(...)                               _EXPAND_1(_EXPAND_1(_EXPAND_1(__VA_ARGS__)))
#define _EXPAND_1(...)                               _EXPAND_2(_EXPAND_2(_EXPAND_2(__VA_ARGS__)))
#define _EXPAND_2(...)                               __VA_ARGS__
#define _MAP__(FUNCTION, SHARED, FIRST, SECOND, ...) FUNCTION(SHARED, FIRST, SECOND) __VA_OPT__(_MAP_ _PARENTHESES (FUNCTION, SHARED, __VA_ARGS__))
#define _MAP_()                                      _MAP__
#define _MAP(FUNCTION, PERIPHERAL_REGISTER, ...)     __VA_OPT__(_EXPAND_0(_MAP__(FUNCTION, PERIPHERAL_REGISTER, __VA_ARGS__)))

#define _GET_POS(PERIPHERAL_REGISTER, FIELD)         CONCAT(CONCAT(PERIPHERAL_REGISTER##_, FIELD##_), Pos)
#define _GET_MSK(PERIPHERAL_REGISTER, FIELD)         CONCAT(CONCAT(PERIPHERAL_REGISTER##_, FIELD##_), Msk)

#define _CLEAR_BITS(PERIPHERAL_REGISTER, FIELD, VALUE) \
    & ~_GET_MSK(PERIPHERAL_REGISTER, FIELD)

#define _SET_BITS(PERIPHERAL_REGISTER, FIELD, VALUE)     \
    | (((VALUE) << _GET_POS(PERIPHERAL_REGISTER, FIELD)) \
                 & _GET_MSK(PERIPHERAL_REGISTER, FIELD))

#define CMSIS_READ(PERIPHERAL_REGISTER, VARIABLE, FIELD)         \
    ((u32) (((VARIABLE) & _GET_MSK(PERIPHERAL_REGISTER, FIELD))  \
                       >> _GET_POS(PERIPHERAL_REGISTER, FIELD)))

#define CMSIS_WRITE(PERIPHERAL_REGISTER, VARIABLE, ...)                       \
    ((void) ((VARIABLE) =                                                     \
            ((VARIABLE) _MAP(_CLEAR_BITS, PERIPHERAL_REGISTER, __VA_ARGS__))  \
                        _MAP(_SET_BITS  , PERIPHERAL_REGISTER, __VA_ARGS__)))

#define CMSIS_SET(PERIPHERAL, REGISTER, ...)                  CMSIS_WRITE(CONCAT(PERIPHERAL##_, REGISTER), PERIPHERAL->REGISTER, __VA_ARGS__)
#define CMSIS_GET(PERIPHERAL, REGISTER, FIELD)                CMSIS_READ (CONCAT(PERIPHERAL##_, REGISTER), PERIPHERAL->REGISTER, FIELD      )
#define CMSIS_GET_FROM(VARIABLE, PERIPHERAL, REGISTER, FIELD) CMSIS_READ (CONCAT(PERIPHERAL##_, REGISTER), VARIABLE            , FIELD      )

struct CMSISTuple
{
    volatile long unsigned int* destination;
    i32                         position;
    u32                         mask;
};

static mustinline void
CMSIS_PUT(struct CMSISTuple tuple, u32 value)
{

    // Read.
    u32 temporary = *tuple.destination;

    // Modify.
    temporary = (temporary & ~tuple.mask) | ((value << tuple.position) & tuple.mask);

    // Write.
    *tuple.destination = temporary;

}



//
// Patches.
//

#if TARGET_MCU_IS_STM32H7S3L8H6 || TARGET_MCU_IS_STM32H533RET6

    #define USART_BRR_BRR_Pos 0                             // For the full contiguous field.
    #define USART_BRR_BRR_Msk (0xFFFF << USART_BRR_BRR_Pos) // "

#endif

#if TARGET_MCU_IS_STM32H533RET6

    #define RCC_CCIPR5_CKPERSEL_ RCC_CCIPR5_CKERPSEL_         // Typo.
    #define USART_TDR_TDR_Pos    0                            // Position and mask not given.
    #define USART_TDR_TDR_Msk    (0x1FF << USART_TDR_TDR_Pos) // "
    #define USART_RDR_TDR_Pos    0                            // "
    #define USART_RDR_TDR_Msk    (0x1FF << USART_RDR_TDR_Pos) // "

#endif



//
// Some more CMSIS stuff.
//

#include "cmsis.meta"
/* #meta



    # Include the CMSIS device header for the STM32 microcontroller.

    for mcu in PER_MCU():

        Meta.line(f'#include <{MCUS[mcu].cmsis_file_path.as_posix()}>')



    # @/`CMSIS Suffix Dropping`:
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



        # Some peripherals furthermore have subunits that
        # also need this suffix-chopping to be done too.
        # e.g:
        # >
        # >    DMAMUX5_RequestGenerator7_   ->   DMAMUX_
        # >    DMAMUX6_RequestGenerator0_   ->   DMAMUX_
        # >    DMAMUX6_RequestGenerator1_   ->   DMAMUX_
        # >

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



// TODO Document.

/* #meta IMPLEMENT_DRIVER_ALIASES : SYSTEM_DATABASE

    def IMPLEMENT_DRIVER_ALIASES(
        *,
        driver_name,
        cmsis_name,
        common_name,
        identifiers,
        cmsis_tuple_tags,
    ):

        for target in PER_TARGET():



            if driver_name not in target.drivers:

                Meta.line(
                    f'#error Target "{target.name}" cannot use the {driver_name} driver '
                    f'without first specifying the peripheral instances to have handles for.'
                )

                continue



            # Have the user be able to specify a specific driver unit.

            Meta.enums(
                f'{driver_name}Handle',
                'u32',
                (handle for handle, instance in target.drivers[driver_name])
            )



            # @/`CMSIS Suffix Dropping`.
            # e.g:
            # >
            # >    I2Cx <-> I2C3
            # >

            Meta.line(f'#define {common_name}_ {cmsis_name}_')



            # Create a look-up table to map the moniker
            # name to the actual underyling value.

            Meta.lut( f'{driver_name}_TABLE', (
                (
                    f'{driver_name}Handle_{handle}',
                    (common_name, instance),
                    *(
                        (identifier.format(common_name), identifier.format(instance))
                        for identifier in identifiers
                    ),
                    *(
                        (
                            cmsis_tuple_tag.format(common_name),
                            CMSIS_TUPLE(SYSTEM_DATABASE[target.mcu][cmsis_tuple_tag.format(instance)])
                        )
                        for cmsis_tuple_tag in cmsis_tuple_tags
                    )
                )
                for handle, instance in target.drivers[driver_name]
            ))



        # Macro to mostly bring stuff in the
        # look-up table into the local scope.

        Meta.line('#undef _EXPAND_HANDLE')

        with Meta.enter('#define _EXPAND_HANDLE'):

            Meta.line(f'''

                if (!(0 <= handle && handle < {driver_name}Handle_COUNT))
                {{
                    panic;
                }}

                struct {driver_name}Driver* const driver = &_{driver_name}_drivers[handle];

            ''')

            for alias in (common_name, *identifiers, *cmsis_tuple_tags):
                Meta.line(f'''
                    auto const {alias.format(common_name)} = {driver_name}_TABLE[handle].{alias.format(common_name)};
                ''')

*/



//////////////////////////////////////////////////////////////// Interrupts ////////////////////////////////////////////////////////////////



#include "interrupts.meta"
/* #meta INTERRUPTS



    # Parse the CMSIS header files to get the
    # list of interrupts on the microcontroller.

    INTERRUPTS = {}

    for mcu in MCUS:



        # The CMSIS header for the microcontroller will define its
        # interrupts with an enumeration; we find the enumeration
        # members so we can automatically get the list of interrupt
        # names and the corresponding numbers.
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
        # we create a list for all interrupt numbers between
        # the lowest and highest.

        INTERRUPTS[mcu] = [None] * (irqn_enumeration[-1][0] - irqn_enumeration[0][0] + 1)

        for interrupt_number, interrupt_name in irqn_enumeration:
            INTERRUPTS[mcu][interrupt_number - irqn_enumeration[0][0]] = interrupt_name

        INTERRUPTS[mcu] = tuple(INTERRUPTS[mcu])



    for target in PER_TARGET():



        # If trying to define an interrupt handler and one makes a typo,
        # then the function end up not replacing the weak symbol that's
        # in place of the interrupt handler in the interrupt vector table,
        # which can end up as a confusing bug. To prevent that, we will use a
        # macro to set up the function prototype of the interrupt handler,
        # and if a typo is made, then a compiler error will be generated.
        # e.g:
        # >
        # >    INTERRUPT_TIM13           <- If "TIM13" interrupt exists,
        # >    {                            then ok!
        # >        ...
        # >    }
        # >
        # >    INTERRUPT_TIM14           <- If "TIM14" interrupt doesn't exists,
        # >    {                            then compiler error here; good!
        # >        ...
        # >    }
        # >
        # >    extern void
        # >    __INTERRUPT_TIM14(void)   <- This will always compile,
        # >    {                            even if "TIM14" doesn't exist; bad!
        # >        ...
        # >    }
        # >

        for interrupt in ('Default',) + INTERRUPTS[target.mcu]:

            if interrupt is None:
                continue

            if target.use_freertos and interrupt in MCUS[target.mcu].freertos_interrupts:
                continue

            Meta.define(f'INTERRUPT_{interrupt}', f'extern void __INTERRUPT_{interrupt}(void)')



        # Create an enumeration for each interrupt the target is using
        # so that the NVIC macros will only work with those specific interrupts.

        Meta.enums(
            'NVICInterrupt',
            'u32',
            (
                (interrupt, f'{interrupt}_IRQn')
                for interrupt, niceness in target.interrupt_priorities
            )
        )
*/



// Macros to control the interrupt in NVIC.
// @/pg 626/tbl B3-8/`Armv7-M`.
// @/pg 1452/tbl D1.1.10/`Armv8-M`.

#define NVIC_ENABLE(NAME)        ((void) (NVIC->ISER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))
#define NVIC_DISABLE(NAME)       ((void) (NVIC->ICER[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))
#define NVIC_SET_PENDING(NAME)   ((void) (NVIC->ISPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))
#define NVIC_CLEAR_PENDING(NAME) ((void) (NVIC->ICPR[NVICInterrupt_##NAME / 32] = 1 << (NVICInterrupt_##NAME % 32)))



//////////////////////////////////////////////////////////////// System Initialization ////////////////////////////////////////////////////////////////



#include "SYSTEM_init.meta"
/* #meta



    # Macros to make using GPIOs in C easy.
    # TODO Use SYSTEM_DATABASE.

    Meta.define('GPIO_HIGH'  , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BS, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_LOW'   , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BR, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_TOGGLE', ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->ODR ^= CONCAT(GPIO_ODR_OD , _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_SET'   , ('NAME', 'VALUE'), '((void) ((VALUE) ? GPIO_HIGH(NAME) : GPIO_LOW(NAME)))')
    Meta.define('GPIO_READ'  , ('NAME'         ), '(!!(CONCAT(GPIO, _PORT_FOR_GPIO_READ(NAME))->IDR & CONCAT(GPIO_IDR_ID, _NUMBER_FOR_GPIO_READ(NAME))))')



    # Initialize the target's GPIOs, interrupts, clock-tree, etc.

    for target in PER_TARGET():

        with Meta.enter('''
            extern void
            SYSTEM_init(void)
        '''):



            ################################ GPIOs ################################

            put_title('GPIOs') # @/`How GPIOs Are Made`:

            gpios = PROCESS_GPIOS(target)



            # Macros to make GPIOs easy to use.

            for gpio in gpios:

                if gpio.pin is None:
                    continue

                if gpio.mode in ('INPUT', 'ALTERNATE'):
                    Meta.define('_PORT_FOR_GPIO_READ'  , ('NAME'), gpio.port  , NAME = gpio.name)
                    Meta.define('_NUMBER_FOR_GPIO_READ', ('NAME'), gpio.number, NAME = gpio.name)

                if gpio.mode == 'OUTPUT':
                    Meta.define('_PORT_FOR_GPIO_WRITE'  , ('NAME'), gpio.port  , NAME = gpio.name)
                    Meta.define('_NUMBER_FOR_GPIO_WRITE', ('NAME'), gpio.number, NAME = gpio.name)



            # Enable GPIO ports that have defined pins.

            CMSIS_SET(
                (
                    SYSTEM_DATABASE[target.mcu][f'GPIO{port}_ENABLE'].peripheral,
                    SYSTEM_DATABASE[target.mcu][f'GPIO{port}_ENABLE'].register,
                    SYSTEM_DATABASE[target.mcu][f'GPIO{port}_ENABLE'].field,
                    True
                )
                for port in sorted(OrderedSet(
                    gpio.port
                    for gpio in gpios
                    if gpio.pin is not None
                ))
            )



            # Set output type (push-pull/open-drain).

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'OTYPER', f'OT{gpio.number}', gpio.open_drain)
                for gpio in gpios
                if gpio.pin        is not None
                if gpio.open_drain is not None
            )



            # Set initial output level.

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'ODR', f'OD{gpio.number}', gpio.initlvl)
                for gpio in gpios
                if gpio.pin     is not None
                if gpio.initlvl is not None
            )



            # Set drive strength.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'OSPEEDR',
                    f'OSPEED{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_SPEED'])[gpio.speed]
                )
                for gpio in gpios
                if gpio.pin   is not None
                if gpio.speed is not None
            )



            # Set pull configuration.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'PUPDR',
                    f'PUPD{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_PULL'])[gpio.pull]
                )
                for gpio in gpios
                if gpio.pin  is not None
                if gpio.pull is not None
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
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_MODE'])[gpio.mode]
                )
                for gpio in gpios
                if gpio.pin  is not None
                if gpio.mode not in (None, 'RESERVED')
            )



            ################################ NVIC ################################

            put_title('NVIC')



            # Set the priorities of the defined NVIC interrupts;
            # the amount of bits that can be used to specify the priorities
            # vary between implementations.
            # @/pg 526/sec B1.5.4/`Armv7-M`.
            # @/pg 86/sec B3.9/`Armv8-M`.

            for interrupt, niceness in target.interrupt_priorities:
                Meta.line(f'''
                    static_assert(0 <= {niceness} && {niceness} < (1 << __NVIC_PRIO_BITS));
                    NVIC->IPR[{interrupt}_IRQn] = {niceness} << __NVIC_PRIO_BITS;
                ''')



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



    # We create a table to map GPIO pin and alternate
    # function names to alternate function codes.
    # e.g:
    # >
    # >    ('A', 0, 'UART4_TX')   ->   0b1000
    # >

    GPIO_AFSEL = {}

    for mcu in MCUS:

        GPIO_AFSEL[mcu] = []



        # Find the CSV file that STM32CubeMX can provide to
        # get all of the alternate functions for GPIOs.

        gpio_afsel_file_path = root(f'./deps/mcu/{mcu}_gpios.csv')

        if not gpio_afsel_file_path.is_file():
            raise RuntimeError(
                f'File "{gpio_afsel_file_path}" does not exist; '
                f'use STM32CubeMX to generate the CSV file '
                f'(also clear the pinout!).'
            )



        # Process the CSV file.

        for entry in csv.DictReader(
            gpio_afsel_file_path.read_text().splitlines()
        ):

            match entry['Type']:



                # Most GPIOs we care about are the I/O ones.

                case 'I/O':

                    # Some GPIO names are suffixed with
                    # additional things, like for instance:
                    #
                    #     PC14-OSC32_IN(OSC32_IN)
                    #     PH1-OSC_OUT(PH1)
                    #     PA14(JTCK/SWCLK)
                    #     PC2_C
                    #
                    # So we need to format it slightly so that
                    # we just get the port letter and pin number.

                    pin    = entry['Name']
                    pin    = pin.split('-', 1)[0]
                    pin    = pin.split('(', 1)[0]
                    pin    = pin.split('_', 1)[0]
                    port   = pin[1]
                    number = int(pin[2:])

                    assert pin.startswith('P') and ('A' <= port <= 'Z')



                    # Gather all the alternate functions of the GPIO, if any.

                    for afsel_code in range(16):



                        # Skip empty cells.

                        if not entry[f'AF{afsel_code}']:
                            continue



                        # Some have multiple names for the
                        # same AFSEL code (e.g. "I2S3_CK/SPI3_SCK").

                        GPIO_AFSEL[mcu] += [
                            ((port, number, afsel_name), afsel_code)
                            for afsel_name in entry[f'AF{afsel_code}'].split('/')
                        ]




                # TODO Maybe use this information to ensure
                #      the PCB footprint is correct?

                case 'Power' | 'Reset' | 'Boot':
                    pass



                # TODO I have no idea what this is.

                case 'MonoIO':
                    pass



                # Unknown GPIO type in the CSV;
                # doesn't neccessarily mean an error though.

                case _:
                    pass # TODO Warn.



        # Done processing the CSV file;
        # now we have a mapping of GPIO pin
        # and alternate function name to the
        # alternate function code.

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



        # If the pin of the GPIO is given, split it into
        # its port and number parts (e.g. 'A10' -> ('A', 10)).
        # The pin can be left unspecified as `None`, which is
        # useful for when we don't know where the pin will be
        # end up being at but we want to at least have it still
        # be in the table.

        if pin is not None:
            gpio.port   = pin[0]
            gpio.number = int(pin[1:])



        # Handle the GPIO mode.

        match mode:



            # A simple input GPIO to read digital voltage levels.

            case 'INPUT':



                # The pull-direction must be specified in order to
                # prevent accidentally defining a floating GPIO pin.

                gpio.pull = properties.pop('pull')



            # A simple output GPIO that can be driven low or high.

            case 'OUTPUT':



                # The initial voltage level must be specified
                # so the user remembers to take it into consideration.

                gpio.initlvl = properties.pop('initlvl')



                # Other optional properties.

                gpio.speed      = properties.pop('speed'     , None)
                gpio.open_drain = properties.pop('open_drain', None)




            # This GPIO would typically be used for some
            # peripheral functionality (e.g. SPI clock output).

            case 'ALTERNATE':



                # Alternate GPIOs are denoted by a string like "UART8_TX"
                # to indicate its alternate function.

                gpio.altfunc = properties.pop('altfunc')



                # Other optional properties.

                gpio.speed      = properties.pop('speed'     , None)
                gpio.pull       = properties.pop('pull'      , None)
                gpio.open_drain = properties.pop('open_drain', None)



            # An analog GPIO would have its Schmit trigger function
            # disabled; this obviously allows for ADC/DAC usage,
            # but it can also serve as a power-saving measure.

            case 'ANALOG':
                raise NotImplementedError



            # A GPIO that's marked as "RESERVED" is often useful
            # for marking a particular pin as something that
            # shouldn't be used because it has an important
            # functionality (e.g. JTAG debug).
            # We ignore any properties the reserved pin may have.

            case 'RESERVED':
                properties = {}



            # Unknown GPIO mode.

            case unknown:
                raise ValueError(
                    f'GPIO "{name}" has unknown '
                    f'mode: {repr(unknown)}.'
                )



        # Determine the GPIO's alternate function code.

        if gpio.pin is not None and gpio.altfunc is not None:

            gpio.afsel = GPIO_AFSEL[target.mcu].get(
                (gpio.port, gpio.number, gpio.altfunc),
                None
            )

            if gpio.afsel is None:
                raise ValueError(
                    f'GPIO pin {repr(gpio.pin)} '
                    f'for {target.mcu} ({target.name}) '
                    f'has no alternate function {repr(gpio.altfunc)}.'
                )



        # Done processing this GPIO entry!

        if properties:
            raise ValueError(
                f'GPIO "{name}" has leftover properties: {properties}.'
            )

        return gpio



    # The routine to ensure the list of GPIOs make sense for the target.

    def PROCESS_GPIOS(target):

        gpios = tuple(
            mk_gpio(target, entry)
            for entry in target.gpios
        )

        if (name := find_dupe(
            gpio.name
            for gpio in gpios
        )) is not ...:
            raise ValueError(
                f'GPIO name {repr(name)} used more than once.'
            )

        if (pin := find_dupe(
            gpio.pin
            for gpio in gpios
            if gpio.pin is not None
        )) is not ...:
            raise ValueError(
                f'GPIO pin {repr(pin)} used more than once.'
            )

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

    #else // We're going to assume there's an LED we can toggle.

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

}



INTERRUPT_UsageFault
{

    // @/pg 611/sec B3.2.15/`Armv7-M`.
    // @/pg 1901/sec D1.2.267/`Armv8-M`.
    u32 usage_fault_status = CMSIS_GET(SCB, CFSR, USGFAULTSR);
    b32 stack_overflow     = (usage_fault_status >> 4) & 1; // This is only defined on Armv8-M.

    panic; // See the values above to determine what caused this UsageFault.

}

INTERRUPT_BusFault
{

    // @/pg 611/sec B3.2.15/`Armv7-M`.
    // @/pg 1472/sec D1.2.7/`Armv8-M`.
    u32 bus_fault_status             = CMSIS_GET(SCB, CFSR, BUSFAULTSR);
    b32 bus_fault_address_valid      = (bus_fault_status >> 7) & 1;
    b32 imprecise_data_access        = (bus_fault_status >> 2) & 1;
    b32 precise_data_access          = (bus_fault_status >> 1) & 1;
    b32 instruction_access_violation = (bus_fault_status >> 0) & 1;

    // @/pg 614/sec B3.2.18/`Armv7-M`.
    // @/pg 1471/sec D1.2.6/`Armv8-M`.
    u32 bus_fault_address = SCB->BFAR;

    panic; // See the values above to determine what caused this BusFault.

}



INTERRUPT_Default
{

    // @/pg 599/sec B3.2.4/`Armv7-M`.
    // @/pg 1680/sec D1.2.125/`Armv8-M`.
    i32 interrupt_number = CMSIS_GET(SCB, ICSR, VECTACTIVE) - 16;

    switch (interrupt_number)
    {
        case HardFault_IRQn:
        {
            panic; // There was a HardFault for some unhandled reason...
        } break;

        default:
        {
            panic; // Unknown interrupt! TODO Make an enumeration so we know which it is easily.
        } break;
    }

}



//////////////////////////////////////////////////////////////// FreeRTOS ////////////////////////////////////////////////////////////////



// FreeRTOS headers here so that we can still compile
// tasks that use FreeRTOS functions; if the function
// actually gets used, there'll be a link error anyways.

#include <deps/FreeRTOS_Kernel/include/FreeRTOS.h>
#include <deps/FreeRTOS_Kernel/include/task.h>
#include <deps/FreeRTOS_Kernel/include/semphr.h>



// This macro just expands to the function prototype used
// by all FreeRTOS tasks, but the main reason why we use
// this is because it allows us to have a meta-directive to
// search for instances of this macro and from there be able
// to determine the target's list of tasks. It is not a
// fool-proof strategy, however, because we can do something
// dumb like wrap a task with #if and the meta-directive wouldn't
// know that, but I think edge-cases like these can be ignored.
// We can, however, assert that the meta-directive picked up
// the `FREERTOS_TASK`; if we don't, the task might be silently
// defined but not be registered by the task-scheduler.

#define FREERTOS_TASK(NAME, STACK_SIZE, PRIORITY)                                                             \
    static_assert(                                                                                            \
        FreeRTOSTask_##NAME >= 0 || "The `FREERTOS_TASK` macro is being invoked with an unrecognized syntax!" \
    );                                                                                                        \
    static noret void NAME(void*)



#include "freertos.meta"
/* #meta

    import re

    for target in PER_TARGET():



        # Macros for whether or not the target will be using FreeRTOS.

        Meta.define('TARGET_USES_FREERTOS', target.use_freertos)



        # Find all the FreeRTOS tasks defined by the target.
        # Note that if `FREERTOS_TASK` is used in a file that's
        # included elsewhere, we would be missing that. In other
        # words, `FREERTOS_TASK` can only be used at the root of
        # the source file. This is pretty much fine for all use-cases.

        tasks = []

        for source_file_path in target.source_file_paths:

            for match in re.findall(
                r'FREERTOS_TASK\s*\((\s*[a-zA-Z0-9_]+\s*,\s*[0-9\']+\s*,\s*[a-zA-Z0-9_]+\s*)\)',
                '\n'.join(
                    line
                    for line in source_file_path.read_text().splitlines()
                    if not line.lstrip().startswith('//')
                )
            ):

                task_name, stack_size, priority = (field.strip() for field in match.split(','))

                tasks += [types.SimpleNamespace(
                    name       = task_name,
                    stack_size = int(stack_size),
                    priority   = priority
                )]



        # Create an enumeration of all FreeRTOS tasks for the target
        # so that if the meta-directive missed a FREERTOS_TASK
        # somewhere, compilation will fail.

        Meta.enums('FreeRTOSTask', 'u32', (task.name for task in tasks))



        # Rest of the stuff is only when the target actually uses FreeRTOS.

        if not target.use_freertos:
            continue



        # Declare some stuff for the tasks.

        for task in tasks:
            Meta.line(f'''
                static noret void   {task.name}(void*);
                static StackType_t  {task.name}_stack[{task.stack_size} / sizeof(StackType_t)] = {{0}};
                static StaticTask_t {task.name}_buffer = {{0}};
            ''')



        # Create the procedure that'll initialize and run the FreeRTOS scheduler.

        with Meta.enter('''
            static noret void
            FREERTOS_init(void)
        '''):

            for task in tasks:
                Meta.line(f'''
                    TaskHandle_t {task.name}_handle =
                        xTaskCreateStatic
                        (
                            {task.name},
                            "{task.name}",
                            countof({task.name}_stack),
                            nullptr,
                            {task.priority},
                            {task.name}_stack,
                            &{task.name}_buffer
                        );
                ''')

            Meta.line('''
                vTaskStartScheduler();
                panic;
            ''')



    # The necessary include-directives to compile with FreeRTOS.

    for mcu in PER_MCU():

        with Meta.enter('#if TARGET_USES_FREERTOS'):

            for header in MCUS[mcu].freertos_source_files:
                Meta.line(f'#include <{header}>')

*/



#if TARGET_USES_FREERTOS

    #define MUTEX_TAKE(MUTEX)                                          \
        do                                                             \
        {                                                              \
            if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) \
            {                                                          \
                if (!xSemaphoreTake((MUTEX), portMAX_DELAY))           \
                {                                                      \
                    panic;                                             \
                }                                                      \
            }                                                          \
        }                                                              \
        while (false)

    #define MUTEX_GIVE(MUTEX)                                          \
        do                                                             \
        {                                                              \
            if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) \
            {                                                          \
                if (!xSemaphoreGive((MUTEX)))                          \
                {                                                      \
                    panic;                                             \
                }                                                      \
            }                                                          \
        }                                                              \
        while (false)

#else

    #define MUTEX_TAKE(MUTEX)
    #define MUTEX_GIVE(MUTEX)

#endif



//////////////////////////////////////////////////////////////// Notes /////////////////////////////////////////////////////////////////



// This file (and this includes the entire folder) contains
// definitions for stuff that's quite low-level and technical.
// Most of it is just infrastructure to make programming the
// higher-level stuff on STM32s very pain-free and fun.



// @/`Halting`:
//
// A `sorry` is used for code-paths that haven't been implemented yet.
// Eventually, when we want things to be production-ready, we replace
// all `sorry`s that we can with proper code, or at the very least,
// a `panic`.
// e.g:
// >
// >    if (is_sd_card_mounted_yet())
// >    {
// >        do_stuff();
// >    }
// >    else
// >    {              Note that `sorry` is specifically defined to be able to
// >        sorry   <- be written without the terminating semicolon. This is
// >    }              purely aesthetic; it makes `sorry` look very out-of-place.
// >
// >
//
// A `panic` is for absolute irrecoverable error conditions,
// like stack overflows or a function given ill-formed parameters.
// In other words, stuff like: "something horrible happened.
// I don't know how we got here. We need to reset!". It is useful
// to make this distinction from `sorry` because we can scan through
// the code-base and see what stuff is work-in-progress or just an
// irrecoverable error condition.
// e.g:
// >
// >    switch (option)
// >    {
// >        case Option_A: ...     The compiler will enforce switch-statements to be exhaustive
// >        case Option_B: ...     for enumerations. The compiler will also enforce all
// >        case Option_C: ...     switch-statements to have a `default` case even if we are
// >        default: panic;     <- exhaustive. In the event that the enumeration `option` is
// >    }                          somehow not any of the valid values, we will trigger a `panic`
// >                               rather than have the switch-statement silently pass.
// >
//
// When a `sorry` or `panic` is triggered during deployment, the
// microcontroller will undergo a reset through the watchdog timer (TODO).
// During development, however, we'd like for the controller to actually
// stop dead in its track (rather than reset) so that we can debug.
// To make it even more useful, the microcontroller can also blink an
// LED indicating whether or not a `panic` or a `sorry` condition has
// occured; how this is implemented, if at all, is entirely dependent
// upon the target.
