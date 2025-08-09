#meta SYSTEM_CONFIGURIZE : SYSTEM_DATABASE

import re, collections, builtins

def SYSTEM_CONFIGURIZE(target, configurations):

    ################################################################################################################################



    # The database is how we will figure out which register to write and where.

    database = SYSTEM_DATABASE[target.mcu]



    # Helper routine to make the output look nice and well divided.

    def put_title(title):
        Meta.line(f'''

            {"/" * 64} {title} {"/" * 64}

        ''')



    # This helper routine can be used to look up a value in "configurations" or look up in the database to find the location of a register;
    # the value in the section-register-field-value tuple can also be changed to something else.
    # e.g.
    # cfgs('flash_latency'        )   ->   configurations['flash_latency']
    # cfgs('flash_latency', ...   )   ->   ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])
    # cfgs('flash_latency', 0b1111)   ->   ('FLASH', 'ACR', 'LATENCY', 0b1111)

    used_configurations = OrderedSet()

    def cfgs(tag, *value, **placeholder_values):



        # Make sure all placeholders in the tag are given.
        # e.g.
        # cfgs('pll{UNIT}_input_range', UNIT = 3)

        placeholders = OrderedSet(re.findall('{(.*?)}', tag))

        if differences := list(OrderedSet(placeholders) - placeholder_values.keys()):
            raise ValueError(f'Tag "{tag}" is missing the value for the placeholder "{differences[0]}".')

        if differences := list(OrderedSet(placeholder_values.keys()) - placeholders):
            raise ValueError(f'Tag "{tag}" has no placeholder "{differences[0]}".')



        # Get the value from "configurations", if needed.
        # e.g.
        # cfgs('flash_latency'        )   ->   configurations['flash_latency']
        # cfgs('flash_latency', ...   )   ->   ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])

        def find_configuration_value():

            nonlocal used_configurations

            configuration = tag.format(**placeholder_values)

            if configuration not in configurations:
                if placeholders:
                    raise ValueError(f'For {target.mcu}, the tag "{tag}" expanded to the undefined configuration "{configuration}".')
                else:
                    raise ValueError(f'For {target.mcu}, configuration "{configuration}" was not provided.')

            used_configurations |= { configuration }

            return configurations[configuration]



        # Find the database entry that has the desired tag and placeholder values, if needed.
        # e.g.
        # cfgs('pll{UNIT}_predivider', UNIT = 2)   ->   (pll{UNIT}_predivider (RCC PLLCKSELR DIVM2) (minmax: 1 63) (UNIT = 2))

        def find_database_entry():


            if tag not in database:
                raise ValueError(f'For {target.mcu}, no system database entry was found with the tag of "{tag}".')

            match len(placeholders):



                # No placeholders, so we just do a direct look-up.
                # cfgs('cpu_divider')   ->   SYSTEM_DATBASE[mcu]['cpu_divider']
                case 0:
                    return database[tag]



                # Single placeholder, so we get the entry based on the solely provided keyword-argument.
                # e.g.
                # cfgs('pll{UNIT}_predivider', UNIT = 2)   ->   SYSTEM_DATBASE[mcu]['pll{UNIT}_predivider'][2]

                case 1:
                    return database[tag][placeholder_values[placeholders[0]]]



                # Multiple placeholders, so we get the entry based on the provided keyword-arguments.
                # e.g.
                # cfgs('pll{UNIT}{CHANNEL}_enable', UNIT = 2, CHANNEL = 'q')   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][Placeholders(UNIT = 2, CHANNEL = 'q')]
                case _:
                    Placeholders = collections.namedtuple('Placeholders', placeholder_values.keys())
                    return database[tag][Placeholders(**placeholder_values)]



        # Format the result in the expected format.

        match value:



            # Get the value from "configurations" directly.
            # cfgs('flash_latency')   ->   configurations['flash_latency']

            case []:
                return find_configuration_value()



            # Get the value from "configurations" and append it to the register's location tuple.
            # cfgs('flash_latency', ...)   ->   ('FLASH', 'ACR', 'LATENCY', configurations['flash_latency'])

            case [builtins.Ellipsis]:
                entry = find_database_entry()
                return (entry.SECTION, entry.REGISTER, entry.FIELD, find_configuration_value())



            # Append the desired value to the register's location tuple.
            # cfgs('flash_latency', 0b1111)   ->   ('FLASH', 'ACR', 'LATENCY', 0b1111)

            case [value]:
                entry = find_database_entry()
                return (entry.SECTION, entry.REGISTER, entry.FIELD, value)



            # Ill-defined arguments.

            case _:
                raise ValueError(f'Either nothing, "...", or a single value should be given for the argument; got: {value}.')



    ################################################################################################################################

    # We have to program a delay for reading the flash as it takes
    # time for the data stored in the flash memory to stablize for read operations;
    # this delay varies based on voltage and clock frequency.
    # @/pg 210/sec 5.3.7/`H7S3rm`.
    # @/pg 159/sec 4.3.8/`H730rm`.

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



    # The power supply setup must be configured first before configuring VOS or the system clock frequency.
    # @/pg 323/sec 6.8.4/`H7S3rm`.
    # @/pg 287/sec 6.8.4/`H730rm`.
    CMSIS_SET(
        cfgs(configuration, ...)
        for configuration in (
            'smps_output_level',
            'smps_forced_on',
            'smps_enable',
            'ldo_enable',
            'power_management_bypass',
        )
        if cfgs(configuration) is not None
    )



    # A higher core voltage means higher power consumption, but better performance in terms of max clock speed.
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

        sets += [cfgs('pll_clock_source', ...)]



        # Configure each PLL.

        for unit, channels in database['PLL_UNITS'].VALUE:



            # Set the PLL's predividers.

            predivider = cfgs('pll{UNIT}_predivider', UNIT = unit)

            if predivider is not None:
                sets += [cfgs('pll{UNIT}_predivider', ..., UNIT = unit)]



            # Set each PLL unit's expected input frequency range.

            input_range = cfgs('pll{UNIT}_input_range', UNIT = unit)

            if input_range is not None:
                sets += [cfgs('pll{UNIT}_input_range', ..., UNIT = unit)]



            # Set each PLL unit's multipler.

            multiplier = cfgs('pll{UNIT}_multiplier', UNIT = unit)

            if multiplier is not None:
                sets += [cfgs('pll{UNIT}_multiplier', f'{multiplier} - 1', UNIT = unit)]



            # Set each PLL unit's output divider and enable the channel.

            for channel in channels:

                divider = cfgs('pll{UNIT}_{CHANNEL}_divider', UNIT = unit, CHANNEL = channel)

                if divider is not None:
                    sets += [cfgs('pll{UNIT}{CHANNEL}_divider', f'{divider} - 1', UNIT = unit, CHANNEL = channel)]
                    sets += [cfgs('pll{UNIT}{CHANNEL}_enable' , True            , UNIT = unit, CHANNEL = channel)]



    # Enable each PLL unit that is to be used.

    CMSIS_SET(cfgs('pll{UNIT}_enable', ..., UNIT = unit) for unit, channels in database['PLL_UNITS'].VALUE)



    # Ensure each enabled PLL unit has stabilized.

    for unit, channels in database['PLL_UNITS'].VALUE:

        pllx_enable = cfgs('pll{UNIT}_enable', UNIT = unit)

        if pllx_enable:
            CMSIS_SPINLOCK(cfgs('pll{UNIT}_ready', True, UNIT = unit))



    ################################################################################################################################



    put_title('System Clock Generation Unit')



    # Configure the SCGU registers.

    CMSIS_SET(

        # Divider for the CPU clock.
        cfgs('cpu_divider', ...),

        # There's then the AXI/AHB bus divider.
        cfgs('axi_ahb_divider', ...),

        # Then after that, we have the dividers for APBs.
        *(cfgs('apb{UNIT}_divider', ..., UNIT = unit) for unit in database['APB_UNITS'].VALUE),

    )



    # Now switch system clock to the desired source.

    CMSIS_SET(cfgs('scgu_clock_source', ...))



    # Wait until the desired source has been selected.

    CMSIS_SPINLOCK(cfgs('effective_scgu_clock_source', cfgs('scgu_clock_source')))



    ################################################################################################################################



    if cfgs('systick_enable'):

        put_title('SysTick')

        CMSIS_SET(
            cfgs('systick_reload'          , ... ), # Modulation of the counter.
            cfgs('systick_use_cpu_ck'      , ... ), # Use CPU clock or the vendor-provided one.
            cfgs('systick_counter'         , 0   ), # "The SYST_CVR value is UNKNOWN on reset." @/pg 620/sec B3.3.1/`ARMv7-M`.
            cfgs('systick_interrupt_enable', True), # Enable SysTick interrupt, triggered at every overflow.
            cfgs('systick_enable'          , True), # Enable SysTick counter.
        )



    ################################################################################################################################


    # Ensure we've used all the configurations given.

    if leftovers := OrderedSet(key for key, value in configurations.items() if value is not None) - used_configurations:
        log(f'[WARNING] There are leftover {target.mcu} configurations: {leftovers}.', ansi = 'fg_yellow')
