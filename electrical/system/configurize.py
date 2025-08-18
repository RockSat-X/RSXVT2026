#meta SYSTEM_CONFIGURIZE : SYSTEM_DATABASE, verify_and_get_field_names_in_tag_order

import collections, builtins

def SYSTEM_CONFIGURIZE(target, configurations):

    ################################################################################################################################



    # The database is how we will figure out which register to write and where.

    database = SYSTEM_DATABASE[target.mcu]



    # This helper routine can be used to look up a value in `configurations`
    # or look up in the database to find the location of a register;
    # the value in the section-register-field-value tuple can also be
    # changed to something else.
    # e.g:
    # >
    # >    cfgs('flash_latency'        ) -> configurations['flash_latency']
    # >    cfgs('flash_latency', ...   ) -> ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])
    # >    cfgs('flash_latency', 0b1111) -> ('FLASH', 'ACR', 'LATENCY', 0b1111)
    # >

    used_configurations = OrderedSet()

    CfgFmt = collections.namedtuple('CfgFmt', ( # @/`About CfgFmt`.
        'database_expansion',
        'configuration_expansion'
    ))

    def cfgs(tag, *value, **fields):



        # @/`About CfgFmt`.

        verify_and_get_field_names_in_tag_order(tag, fields)

        fields_for_configuration = {
            name : value.configuration_expansion if isinstance(value, CfgFmt) else value
            for name, value in fields.items()
        }

        fields_for_database = {
            name : value.database_expansion if isinstance(value, CfgFmt) else value
            for name, value in fields.items()
        }



        # Get the value from `configurations`, if needed.
        # e.g:
        # >
        # >    cfgs('flash_latency'     ) ->                             configurations['flash_latency']
        # >    cfgs('flash_latency', ...) -> ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])
        # >
        #

        def get_configuration_value():

            nonlocal used_configurations

            configuration_name = tag.format(**fields_for_configuration)

            if configuration_name not in configurations:
                if fields:
                    raise ValueError(
                        f'For {target.mcu}, the tag {repr(tag)} expanded to the '
                        f'undefined configuration {repr(configuration_name)}.'
                    )
                else:
                    raise ValueError(
                        f'For {target.mcu}, '
                        f'configuration {repr(configuration_name)} was not provided.'
                    )

            used_configurations |= { configuration_name }

            return configurations[configuration_name]



        # Format the result in the expected format.

        match value:



            # Get the value from `configurations` directly.
            # e.g:
            # >
            # >    cfgs('flash_latency') -> configurations['flash_latency']
            # >

            case []:

                return get_configuration_value()



            # Get the value from `configurations` and append it to the register's location tuple.
            # e.g:
            # >
            # >    cfgs('flash_latency', ...) -> ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])
            # >

            case [builtins.Ellipsis]:

                entry = database[tag.format(**fields_for_database)]

                return (
                    entry.section,
                    entry.register,
                    entry.field,
                    get_configuration_value()
                )



            # Append the desired value to the register's location tuple.
            # e.g:
            # >
            # >    cfgs('flash_latency', 0b1111) -> ('FLASH', 'ACR', 'LATENCY', 0b1111)
            # >

            case [value]:

                entry = database[tag.format(**fields_for_database)]

                return (
                    entry.section,
                    entry.register,
                    entry.field,
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

    # We have to program a delay for reading the flash as it takes
    # time for the data stored in the flash memory to stablize for read operations;
    # this delay varies based on voltage and clock frequency.
    # @/pg 210/sec 5.3.7/`H7S3rm`.

    put_title('Flash')



    # Set the wait-states.

    CMSIS_SET(
        cfgs('flash_latency'          , ...),
        cfgs('flash_programming_delay', ...),
    )



    # Ensure the new number of wait-states is taken into account.

    CMSIS_SPINLOCK(
        cfgs('flash_latency'          , ...),
        cfgs('flash_programming_delay', ...),
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
                'smps_output_level',
                'smps_forced_on',
                'smps_enable',
                'ldo_enable',
                'power_management_bypass',
            )

        case 'STM32H533RET6':
            fields = (
                'ldo_enable',
                'power_management_bypass',
            )

        case _: raise NotImplementedError

    CMSIS_SET(
        cfgs(field, ...)
        for field in fields
        if cfgs(field) is not None
    )



    # A higher core voltage means higher power consumption,
    # but better performance in terms of max clock speed.

    CMSIS_SET(cfgs('internal_voltage_scaling', ...))



    # Ensure the voltage scaling has been selected.

    CMSIS_SPINLOCK(
        cfgs('current_active_vos'      , cfgs('internal_voltage_scaling')),
        cfgs('current_active_vos_ready', True                            ),
    )



    ################################################################################################################################



    put_title('Built-in Oscillators')



    # High-speed-internal.

    if cfgs('hsi_enable'):
        pass # The HSI oscillator is enabled by default after reset.
    else:
        raise NotImplementedError(f'Turning off HSI not implemented yet.')



    # High-speed-internal (48MHz).

    if cfgs('hsi48_enable'):
        CMSIS_SET     (cfgs('hsi48_enable', True))
        CMSIS_SPINLOCK(cfgs('hsi48_ready' , True))



    # Clock-security-internal.

    if cfgs('csi_enable'):
        CMSIS_SET     (cfgs('csi_enable', True))
        CMSIS_SPINLOCK(cfgs('csi_ready' , True))



    ################################################################################################################################



    put_title('Peripheral Clock Source')



    # Set the clock source which will be available for some peripheral to use.

    CMSIS_SET(cfgs('per_ck_source', ...))



    ################################################################################################################################



    put_title('PLLs')



    # Set up the PLL registers.

    with CMSIS_SET as sets:



        # Set the clock source shared for all PLLs.

        if target.mcu == 'STM32H7S3L8H6':
            sets += [cfgs('pll_clock_source', ...)]



        # Configure each PLL.

        for unit, channels in database['PLL_UNITS'].value:



            # Set the clock source for this PLL unit.

            if target.mcu == 'STM32H533RET6':
                sets += [cfgs(f'pll{unit}_clock_source', ...)]



            # Set the PLL's predividers.

            predivider = cfgs(f'pll{unit}_predivider')

            if predivider is not None:
                sets += [cfgs(f'pll{unit}_predivider', ...)]



            # Set each PLL unit's expected input frequency range.

            input_range = cfgs(f'pll{unit}_input_range')

            if input_range is not None:
                sets += [cfgs(f'pll{unit}_input_range', ...)]



            # Set each PLL unit's multipler.

            multiplier = cfgs(f'pll{unit}_multiplier')

            if multiplier is not None:
                sets += [cfgs(f'pll{unit}_multiplier', f'{multiplier} - 1')]



            # Set each PLL unit's output divider and enable the channel.

            for channel in channels:

                divider = cfgs(f'pll{unit}_{channel}_divider')

                if divider is None:
                    continue

                sets += [
                    cfgs(f'pll{unit}{channel}_divider', f'{divider} - 1'),
                    cfgs(f'pll{unit}{channel}_enable' , True            ),
                ]



    # Enable each PLL unit that is to be used.

    CMSIS_SET(
        cfgs(f'pll{unit}_enable', ...)
        for unit, channels in database['PLL_UNITS'].value
    )



    # Ensure each enabled PLL unit has stabilized.

    for unit, channels in database['PLL_UNITS'].value:

        pllx_enable = cfgs(f'pll{unit}_enable')

        if pllx_enable:
            CMSIS_SPINLOCK(cfgs(f'pll{unit}_ready', True))



    ################################################################################################################################



    put_title('System Clock Generation Unit')



    # Configure the SCGU registers.

    match target.mcu:

        case 'STM32H7S3L8H6':
            CMSIS_SET(
                cfgs('cpu_divider', ...),
                cfgs('axi_ahb_divider', ...),
                *(
                    cfgs(f'apb{unit}_divider', ...)
                    for unit in database['APB_UNITS'].value
                ),
            )

        case 'STM32H533RET6':
            CMSIS_SET(
                cfgs('cpu_divider', ...),
                *(
                    cfgs(f'apb{unit}_divider', ...)
                    for unit in database['APB_UNITS'].value
                ),
            )

        case _: raise NotImplementedError



    # Now switch system clock to the desired source.

    CMSIS_SET(cfgs('scgu_clock_source', ...))



    # Wait until the desired source has been selected.

    CMSIS_SPINLOCK(cfgs('effective_scgu_clock_source', cfgs('scgu_clock_source')))



    ################################################################################################################################



    if cfgs('systick_enable'):

        put_title('SysTick')

        # @/pg 621/tbl B3-7/`Armv7-M`.
        # @/pg 1861/sec D1.2.239/`Armv8-M`.
        CMSIS_SET(
            cfgs('systick_reload'          , ... ), # Modulation of the counter.
            cfgs('systick_use_cpu_ck'      , ... ), # Use CPU clock or the vendor-provided one.
            cfgs('systick_counter'         , 0   ), # SYST_CVR value is UNKNOWN on reset.
            cfgs('systick_interrupt_enable', True), # Enable SysTick interrupt, triggered at every overflow.
            cfgs('systick_enable'          , True), # Enable SysTick counter.
        )



    ################################################################################################################################



    for uxart_units in database['UXARTS'].value:



        # See if the set of UxART peripherals is used.

        ns = '_'.join(str(number) for peripheral, number in uxart_units)

        if cfgs(f'uxart_{ns}_clock_source') is None:
            continue

        put_title(' / '.join(
            f'{peripheral.upper()}{number}'
            for peripheral, number in uxart_units
        ))



        # Configure the UxART peripherals' clock source.

        CMSIS_SET(
            cfgs('uxart_{UNITS}_clock_source', ...,
            UNITS = CfgFmt(uxart_units, ns))
        )



        # Output the macros to initialize the baud-dividers.

        for peripheral, number in uxart_units:

            baud_divider = cfgs(f'{peripheral}{number}_baud_divider')

            if baud_divider is None:
                continue

            Meta.define(
                f'{peripheral.upper()}{number}_BRR_BRR_init',
                baud_divider
            )



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



# @/`About CfgFmt`:
#
# The field value can actually be different based on whether or not
# we're looking into `configurations` or if we're looking up in `SYSTEM_DATABASE`.
# Most of the time it's the same, but the subtle requirement for this case is that
# the field value for `configuration` needs to be something that can turn into
# a string that looks nice as a result, but keys into `SYSTEM_DATABASE` can be
# any arbritary value.
#
# e.g:
# >
# >    (uxart_{UNITS}_clock_source (RCC CCIPR2 UART278SEL) (UNITS : ((usart 2) (uart 7) (uart 8))) (.value (...)))
# >                                                                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# >                                                         Let's take this database entry as an example.
# >
# >
# >    database['uxart_{UNITS}_clock_source'][(('usart', 2), ('uart', 7), ('uart' 8))]
# >                                           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# >                                Look-ups into the database is done using a tuple as expected.
# >
# >
# >    configurations["uxart_(('usart', 2), ('uart', 7), ('uart' 8))_clock_source"]
# >                          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# >               If we naively do the same for the key into `configurations`,
# >               we'd be doing something stupid like this instead; while we could
# >               accept this artifact, it's best to format it into something more
# >               readable for debugging reasons.
# >
# >
# >    configurations['uxart_2_7_8_clock_source']
# >                          ^^^^^
# >               Something like this is more understable and
# >               might reflect the reference manual more.
# >
# >
# >    units = (('usart', 2), ('uart', 7), ('uart' 8))
# >    ns    = '2_7_8'
# >    cfgs('uxart_{UNITS}_clock_source', ..., UNITS = CfgFmt(units, ns))
# >                                                    ^^^^^^^^^^^^^^^^^
# >                                          Thus, this is how we'd handle this situation.
# >
# >
#
# Of course, we could elimate this entire issue by accepting the artifact,
# but for now I think the look of this is worth the complication;
# we can revisit this in the future.
