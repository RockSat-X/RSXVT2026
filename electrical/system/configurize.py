#meta SYSTEM_CONFIGURIZE : SYSTEM_DATABASE

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

    def cfgs(tag, *value):



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



    for uxart_units in database['UXARTS']:



        # See if the set of UxART peripherals is used.

        if cfgs(f'UXART_{uxart_units}_CLOCK_SOURCE') is None:
            continue

        put_title(' / '.join(
            f'{peripheral}{number}'
            for peripheral, number in uxart_units
        ))



        # Configure the UxART peripherals' clock source.

        CMSIS_SET(cfgs(f'UXART_{uxart_units}_CLOCK_SOURCE', ...))



        # Output the macros to initialize the baud-dividers.

        for peripheral, number in uxart_units:

            baud_divider = cfgs(f'{peripheral}{number}_BAUD_DIVIDER')

            if baud_divider is None:
                continue

            Meta.define(
                f'{peripheral}{number}_BRR_BRR_init',
                baud_divider
            )



    ################################################################################################################################



    for unit in database.get('I2CS', ()):

        if cfgs(f'I2C{unit}_KERNEL_SOURCE') is None:
            continue

        put_title(f'I2C{unit}')

        Meta.define(f'I2C{unit}_KERNEL_SOURCE_init', cfgs(f'I2C{unit}_KERNEL_SOURCE'))
        Meta.define(f'I2C{unit}_TIMINGR_PRESC_init', cfgs(f'I2C{unit}_PRESC'        ))
        Meta.define(f'I2C{unit}_TIMINGR_SCL_init'  , cfgs(f'I2C{unit}_SCL'          ))



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
