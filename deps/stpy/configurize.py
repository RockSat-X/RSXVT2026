#meta SYSTEM_CONFIGURIZE, INTERRUPTS, INTERRUPTS_THAT_MUST_BE_DEFINED, IMPLEMENT_DRIVER_ALIASES : SYSTEM_DATABASE



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

    gpio_afsel_file_path = root(f'./deps/stpy/mcu/{mcu}_gpios.csv')

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



# Parse the CMSIS header files to get the
# list of interrupts on the microcontroller.

INTERRUPTS = {}

for mcu in MCUS:



    # The CMSIS header for the microcontroller
    # will define its interrupts with an enumeration;
    # we find the enumeration members so we can
    # automatically get the list of interrupt
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

    irqs = {}

    for line in MCUS[mcu].cmsis_file_path.read_text().splitlines():

        match line.split():

            case [name, '=', number, *_] if '_IRQn' in name:

                name   = name.removesuffix('_IRQn')
                number = int(number.removesuffix(','))

                assert number not in irqs
                irqs[number] = name



    # The first interrupt should be the reset exception defined
    # by the Arm architecture. Note that the reset exception has
    # an (exception number) of 1, but the *interrupt number* is
    # defined to be (exception number - 16).
    #
    # @/pg 525/tbl B1-4/`Armv7-M`.
    # @/pg 625/sec B3.4.1/`Armv7-M`.
    # @/pg 143/sec B3.30/`Armv8-M`.
    # @/pg 1855/sec D1.2.236/`Armv8-M`.

    irqs = sorted(irqs.items())

    assert irqs[0] == (-15, 'Reset')

    INTERRUPTS[mcu] = {
        interrupt_name : interrupt_number
        for interrupt_number, interrupt_name in irqs
    }



# These interrupt routines
# will always be around even
# if the target didn't explcitly
# state their usage.

INTERRUPTS_THAT_MUST_BE_DEFINED = (
    'Default',
    'MemManage',
    'BusFault',
    'UsageFault'
)



# We create some interrupt-specific stuff
# to make working with interrupts fun!

for target in TARGETS:

    # Check to make sure the interrupts
    # to be used by the target eists.

    for interrupt, niceness in target.interrupts:
        if interrupt not in INTERRUPTS[target.mcu]:

            import difflib

            raise ValueError(
                f'For target {repr(target.name)}, '
                f'no such interrupt {repr(interrupt)} '
                f'exists on {repr(target.mcu)}; '
                f'did you mean any of the following? : '
                f'{difflib.get_close_matches(interrupt, INTERRUPTS[mcu].keys(), n = 5, cutoff = 0)}'
            )



import collections, builtins

def SYSTEM_CONFIGURIZE(target, configurations):

    ################################################################################################################################



    # The database is how we will figure out which register to write and where.

    database = SYSTEM_DATABASE[target.mcu]



    # This helper routine can be used to look up a value in `configurations`
    # or look up in the database to find the location of a register;
    # the value in the peripheral-register-field-value tuple can also be
    # changed to something else.

    used_configurations = OrderedSet()

    def cfgs(tag, *value): # TODO We can make a wrapper around configurations.



        # Mark the configuration as used if we are accessing it.

        def get_configuration_value():

            nonlocal used_configurations

            if tag not in configurations:
                raise ValueError(
                    f'For {target.mcu}, '
                    f'configuration {repr(tag)} was not provided.'
                )

            used_configurations |= { tag }

            return configurations[tag]



        # Format the output depending on the arguments given.

        match value:



            # Get the value from `configurations` directly.
            # e.g:
            # >
            # >              cfgs('FLASH_LATENCY')
            # >                   ~~~~~~~~~~~~~~~
            # >                          |
            # >                   ~~~~~~~~~~~~~~~
            # >    configurations['FLASH_LATENCY']
            # >

            case []:
                return get_configuration_value()



            # Get the value from `configurations` and
            # append it to the register's location tuple.
            # e.g:
            # >
            # >                       cfgs('FLASH_LATENCY', ...)
            # >                                             ~~~
            # >                                              |
            # >                                ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            # >    ('FLASH', 'ACR', 'LATENCY', configurations['FLASH_LATENCY'])
            # >

            case [builtins.Ellipsis]:
                return (
                    database[tag].peripheral,
                    database[tag].register,
                    database[tag].field,
                    get_configuration_value()
                )



            # Append the desired value to the register's location tuple.
            # e.g:
            # >
            # >          cfgs('FLASH_LATENCY', 0b1111)
            # >                                ~~~~~~
            # >                                   |
            # >                                ~~~~~~
            # >    ('FLASH', 'ACR', 'LATENCY', 0b1111)
            # >

            case [value]:
                return (
                    database[tag].peripheral,
                    database[tag].register,
                    database[tag].field,
                    value
                )



            # Ill-defined arguments.

            case _:
                raise ValueError(
                    f'Either nothing, "...", '
                    f'or a single value should be given for the argument; '
                    f'got: {value}.'
                )



    ################################################################################################################################

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



    ################################################################################################################################



    put_title('Interrupts')



    # @/`Defining Interrupt Handlers`.

    for routine in OrderedSet((
        *INTERRUPTS_THAT_MUST_BE_DEFINED,
        *INTERRUPTS[target.mcu]
    )):



        # Skip reserved interrupts.

        if routine is None:
            continue



        # Skip unused interrupts.

        if routine not in (
            *INTERRUPTS_THAT_MUST_BE_DEFINED,
            *(name for name, niceness in target.interrupts)
        ):
            continue



        # The target and FreeRTOS shouldn't be
        # in contention with the same interrupt.

        if target.use_freertos and routine in MCUS[target.mcu].freertos_interrupts:
            raise RuntimeError(
                f'FreeRTOS is already using the interrupt {repr(routine)}; '
                f'either disable FreeRTOS for target {repr(target.name)} or '
                f'just not use {repr(routine)}.'
            )



        # The macro will ensure only the
        # expected ISRs can be defined.

        Meta.define(
            f'INTERRUPT_{routine}',
            f'extern void INTERRUPT_{routine}(void)'
        )



    for interrupt, niceness in target.interrupts:



        # The amount of bits that can be used to specify
        # the priorities vary between implementations.
        # @/pg 526/sec B1.5.4/`Armv7-M`.
        # @/pg 86/sec B3.9/`Armv8-M`.

        Meta.line(f'''
            static_assert(0 <= {niceness} && {niceness} < (1 << __NVIC_PRIO_BITS));
        ''')



        # Set the Arm-specific interrupts' priorities.

        if INTERRUPTS[target.mcu][interrupt] <= -1:

            assert interrupt in (
                'MemoryManagement',
                'BusFault',
                'UsageFault',
                'SVCall',
                'DebugMonitor',
                'PendSV',
                'SysTick',
            )

            Meta.line(f'''
                SCB->SHPR[{interrupt}_IRQn + 12] = {niceness} << __NVIC_PRIO_BITS;
            ''')



        # Set the MCU-specific interrupts' priorities within NVIC.

        else:

            Meta.line(f'''
                NVIC->IPR[NVICInterrupt_{interrupt}] = {niceness} << __NVIC_PRIO_BITS;
            ''')



    ################################################################################################################################



    CMSIS_SET(
        ('SCB', 'SHCSR', 'BUSFAULTENA', True), # Enable the BusFault exception.
        ('SCB', 'SHCSR', 'MEMFAULTENA', True), # Enable the MemFault exception.
        ('SCB', 'SHCSR', 'USGFAULTENA', True), # Enable the UsageFault exception.
    )



    ################################################################################################################################

    # We have to program a delay for reading the flash as it takes time
    # for the data stored in the flash memory to stablize for read operations;
    # this delay varies based on voltage and clock frequency.
    # @/pg 210/sec 5.3.7/`H7S3rm`.

    put_title('Flash')



    # Set the wait-states.

    CMSIS_SET(
        cfgs('FLASH_LATENCY'          , ...),
        cfgs('FLASH_PROGRAMMING_DELAY', ...),
    )



    # Ensure the new number of wait-states is taken into account.

    CMSIS_SPINLOCK(
        cfgs('FLASH_LATENCY'          , ...),
        cfgs('FLASH_PROGRAMMING_DELAY', ...),
    )



    ################################################################################################################################



    # The way the power supply is configured can determine the
    # internal voltage level of the MCU, which can impact the maximum
    # clock speeds of peripherals for instance.

    put_title('Power Supply')



    # The power supply setup must be configured first
    # before configuring VOS or the system clock frequency.
    # @/pg 323/sec 6.8.4/`H7S3rm`.

    match target.mcu:

        case 'STM32H7S3L8H6':
            fields = (
                'SMPS_OUTPUT_LEVEL',
                'SMPS_FORCED_ON',
                'SMPS_ENABLE',
                'LDO_ENABLE',
                'POWER_MANAGEMENT_BYPASS',
            )

        case 'STM32H533RET6':
            fields = (
                'LDO_ENABLE',
                'POWER_MANAGEMENT_BYPASS',
            )

        case _: raise NotImplementedError

    CMSIS_SET(
        cfgs(field, ...)
        for field in fields
        if cfgs(field) is not None
    )



    # A higher core voltage means higher power consumption,
    # but better performance in terms of max clock speed.

    CMSIS_SET(cfgs('INTERNAL_VOLTAGE_SCALING', ...))



    # Ensure the voltage scaling has been selected.

    CMSIS_SPINLOCK(
        cfgs('CURRENT_ACTIVE_VOS'      , cfgs('INTERNAL_VOLTAGE_SCALING')),
        cfgs('CURRENT_ACTIVE_VOS_READY', True                            ),
    )



    ################################################################################################################################



    put_title('Built-in Oscillators')



    # High-speed-internal.

    if cfgs('HSI_ENABLE'):
        pass # The HSI oscillator is enabled by default after reset.
    else:
        raise NotImplementedError(
            f'Turning off HSI not implemented yet.'
        )



    # High-speed-internal (48MHz).

    if cfgs('HSI48_ENABLE'):
        CMSIS_SET     (cfgs('HSI48_ENABLE', True))
        CMSIS_SPINLOCK(cfgs('HSI48_READY' , True))



    # Clock-security-internal.

    if cfgs('CSI_ENABLE'):
        CMSIS_SET     (cfgs('CSI_ENABLE', True))
        CMSIS_SPINLOCK(cfgs('CSI_READY' , True))



    ################################################################################################################################



    put_title('Peripheral Clock Source')



    # Set the clock source which will be
    # available for some peripheral to use.

    CMSIS_SET(cfgs('PER_CK_SOURCE', ...))



    ################################################################################################################################



    put_title('PLLs')



    # Set up the PLL registers.

    with CMSIS_SET as sets:



        # Set the clock source shared for all PLLs.

        if target.mcu == 'STM32H7S3L8H6':
            sets += [cfgs('PLL_CLOCK_SOURCE', ...)]



        # Configure each PLL.

        for unit, channels in database['PLLS']:



            # Set the clock source for this PLL unit.

            if target.mcu == 'STM32H533RET6':
                sets += [cfgs(f'PLL{unit}_CLOCK_SOURCE', ...)]



            # Set the PLL's predividers.

            predivider = cfgs(f'PLL{unit}_PREDIVIDER')

            if predivider is not None:
                sets += [cfgs(f'PLL{unit}_PREDIVIDER', ...)]



            # Set each PLL unit's expected input frequency range.

            input_range = cfgs(f'PLL{unit}_INPUT_RANGE')

            if input_range is not None:
                sets += [cfgs(f'PLL{unit}_INPUT_RANGE', ...)]



            # Set each PLL unit's multipler.

            multiplier = cfgs(f'PLL{unit}_MULTIPLIER')

            if multiplier is not None:
                sets += [cfgs(f'PLL{unit}_MULTIPLIER', f'{multiplier} - 1')]



            # Set each PLL unit's output divider and enable the channel.

            for channel in channels:

                divider = cfgs(f'PLL{unit}_{channel}_DIVIDER')

                if divider is None:
                    continue

                sets += [
                    cfgs(f'PLL{unit}{channel}_DIVIDER', f'{divider} - 1'),
                    cfgs(f'PLL{unit}{channel}_ENABLE' , True            ),
                ]



    # Enable each PLL unit that is to be used.

    CMSIS_SET(
        cfgs(f'PLL{unit}_ENABLE', ...)
        for unit, channels in database['PLLS']
    )



    # Ensure each enabled PLL unit has stabilized.

    for unit, channels in database['PLLS']:

        pllx_enable = cfgs(f'PLL{unit}_ENABLE')

        if pllx_enable:
            CMSIS_SPINLOCK(cfgs(f'PLL{unit}_READY', True))



    ################################################################################################################################



    put_title('System Clock Generation Unit')



    # Configure the SCGU registers.

    match target.mcu:

        case 'STM32H7S3L8H6':
            CMSIS_SET(
                cfgs('CPU_DIVIDER', ...),
                cfgs('AXI_AHB_DIVIDER', ...),
                *(
                    cfgs(f'APB{unit}_DIVIDER', ...)
                    for unit in database['APBS']
                ),
            )

        case 'STM32H533RET6':
            CMSIS_SET(
                cfgs('CPU_DIVIDER', ...),
                *(
                    cfgs(f'APB{unit}_DIVIDER', ...)
                    for unit in database['APBS']
                ),
            )

        case _: raise NotImplementedError



    # Now switch system clock to the desired source.

    CMSIS_SET(cfgs('SCGU_CLOCK_SOURCE', ...))



    # Wait until the desired source has been selected.

    CMSIS_SPINLOCK(
        cfgs('EFFECTIVE_SCGU_CLOCK_SOURCE', cfgs('SCGU_CLOCK_SOURCE'))
    )



    ################################################################################################################################



    if cfgs('SYSTICK_ENABLE'):

        put_title('SysTick')

        # @/pg 621/tbl B3-7/`Armv7-M`.
        # @/pg 1861/sec D1.2.239/`Armv8-M`.
        CMSIS_SET(
            cfgs('SYSTICK_RELOAD'          , ... ), # Modulation of the counter.
            cfgs('SYSTICK_USE_CPU_CK'      , ... ), # Use CPU clock or the vendor-provided one.
            cfgs('SYSTICK_COUNTER'         , 0   ), # SYST_CVR value is UNKNOWN on reset.
            cfgs('SYSTICK_INTERRUPT_ENABLE', True), # Enable SysTick interrupt, triggered at every overflow.
            cfgs('SYSTICK_ENABLE'          , True), # Enable SysTick counter.
        )



    ################################################################################################################################



    for instances in database.get('UXARTS', ()):

        if cfgs(f'UXART_{instances}_KERNEL_SOURCE') is None:
            continue

        put_title(' / '.join(f'{peripheral}{number}' for peripheral, number in instances))

        # TODO I honestly don't know how to feel about doing it this way.
        for peripheral, unit in instances:
            Meta.define(f'{peripheral}{unit}_KERNEL_SOURCE_init', cfgs(f'UXART_{instances}_KERNEL_SOURCE'))

        # TODO Deprecate...?
        Meta.define(
            f'UXART_{'_'.join(str(number) for peripheral, number in instances)}_KERNEL_SOURCE_init',
            cfgs(f'UXART_{instances}_KERNEL_SOURCE')
        )

        for peripheral, number in instances:

            baud_divider = cfgs(f'{peripheral}{number}_BAUD_DIVIDER')

            if baud_divider is None:
                continue

            # TODO More consistent naming?
            Meta.define(f'{peripheral}{number}_BRR_BRR_init', baud_divider)

        # TODO Deprecate.
        CMSIS_SET(cfgs(f'UXART_{instances}_KERNEL_SOURCE', ...))



    ################################################################################################################################



    for unit in database.get('I2CS', ()):

        if cfgs(f'I2C{unit}_KERNEL_SOURCE') is None:
            continue

        put_title(f'I2C{unit}')

        Meta.define(f'I2C{unit}_KERNEL_SOURCE_init', cfgs(f'I2C{unit}_KERNEL_SOURCE'))
        Meta.define(f'I2C{unit}_TIMINGR_PRESC_init', cfgs(f'I2C{unit}_PRESC'        ))
        Meta.define(f'I2C{unit}_TIMINGR_SCL_init'  , cfgs(f'I2C{unit}_SCL'          ))



    ################################################################################################################################



    if configurations.get('GLOBAL_TIMER_PRESCALER', None) is not None:

        put_title('Timers')

        Meta.define(f'GLOBAL_TIMER_PRESCALER_init', cfgs('GLOBAL_TIMER_PRESCALER'))

        for unit in database.get('TIMERS', ()):

            if configurations.get(f'TIM{unit}_DIVIDER', None) is not None:
                Meta.define(f'TIM{unit}_DIVIDER_init', f'({cfgs(f'TIM{unit}_DIVIDER')} - 1)')

            if configurations.get(f'TIM{unit}_MODULATION', None) is not None:
                Meta.define(f'TIM{unit}_MODULATION_init', f'({cfgs(f'TIM{unit}_MODULATION')} - 1)')



    ################################################################################################################################



    # Ensure we've used all the configurations given.

    defined_configurations = OrderedSet(
        key
        for key, value in configurations.items()
        if value is not None
    )

    if unused := defined_configurations - used_configurations:
        log(ANSI(
            f'[WARNING] For {target.name}, '
            f'there are leftover configurations: {repr(unused)}.',
            'fg_yellow'
        ))



################################################################################################################################



# In this meta-directive, we take the configuration
# values from `SYSTEM_PARAMETERIZE` and generate
# code to set the registers in the right order.
#
# Order matters because some clock sources depend
# on other clock sources, so we have to respect that.
#
# More details at @/`About Parameterization`.
