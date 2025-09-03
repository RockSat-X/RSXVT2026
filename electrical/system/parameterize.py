#meta SYSTEM_PARAMETERIZE : SYSTEM_DATABASE

def SYSTEM_PARAMETERIZE(target):



    ################################################################################################################################



    # The database is how we will figure out what needs to be parameterized.

    database = SYSTEM_DATABASE[target.mcu]



    # As we parameterize, we keep track of clock
    # frequencies we have so far in the clock-tree.
    # e.g:
    # >
    # >    tree['PLL2_Q_CK']   ->   200_000_000 Hz
    # >

    tree = AllocatingNamespace(
        { None : 0 }, # So that: tree[None] -> 0 Hz.
    )

    def log_tree(): # For debugging the clock-tree if needed.
        log()
        for just in justify(
            (
                ('<', key          ),
                ('<', f'{value :,}'),
            )
            for key, value in tree
        ):
            log(ANSI('| {} | {} Hz |', 'fg_yellow'), *just)
        log()



    # To ensure we use every option in `target.clock_tree`,
    # we have a helper function to record every access to it
    # that we then verify at the very end; a default value can
    # also be supplied if the option is not in `target.clock_tree`.

    options      = target.clock_tree
    used_options = OrderedSet()

    def opts(option, default = ...):

        nonlocal used_options



        # Mark option as used.

        used_options |= { option }



        # We found the option in `target.clock_tree`.

        if option in options:
            return options[option]



        # Fallback to the default.

        elif default is not ...:
            return default



        # No option in SYSTEM_OPTION nor fallback default!

        else:
            raise ValueError(
                f'For {target.mcu}, '
                f'no system option {repr(option)} was found.'
            )



    # The point of parameterization is to determine what
    # the register values should be in order to initialize
    # the MCU to the specifications of `target.clock_tree`,
    # so we'll be recording that too.
    # e.g:
    # >
    # >    configurations['PLL1_Q_DIVIDER']   ->   256
    # >

    configurations = AllocatingNamespace()

    def log_configurations(): # For debugging the configuration if needed.
        log()
        for row in justify(
            (
                ('<', key  ),
                ('<', value),
            )
            for key, value in configurations
        ):
            log(ANSI('| {} | {} |', 'fg_yellow'), *row)
        log()



    # To brute-force the clock tree, we have to try a lot of possible
    # register values to see what sticks. To do this in a convenient
    # way, `draft` will accumulate configuration values to be eventually
    # inserted into `configurations` itself. Since `draft` is a variable
    # at this scope-level, any function can modify it, no matter how deep
    # we might be in the brute-forcing stack-trace.

    draft = None

    def brute(function, configuration_names):

        nonlocal draft, configurations

        draft = ContainedNamespace(configuration_names)

        success = function()

        if not success:
            raise RuntimeError(
                f'Could not brute-force configurations '
                f'that satisfies the system parameterization.'
            )

        configurations |= draft
        draft           = None # Prevent values being accidentally referenced.



    ################################################################################################################################



    # Some clock frequencies are dictated by `target.clock_tree`,
    # so we can just immediately add them to the tree. We'll check
    # later on to make sure that the frequencies are actually solvable.

    match target.mcu:

        case 'STM32H7S3L8H6':
            clocks = ['CPU_CK', 'SYSTICK_CK', 'AXI_AHB_CK']

        case 'STM32H533RET6':
            clocks = ['CPU_CK', 'SYSTICK_CK']

        case _:
            raise NotImplementedError



    # This includes peripheral busses (e.g. APB3).

    clocks += [
        f'APB{n}_CK'
        for n in database['APBS']
    ]



    # This includes each PLL channel (e.g. PLL2R).

    clocks += [
        f'PLL{n}_{channel}_CK'
        for n, channels in database['PLLS']
        for channel in channels
    ]



    # If `target.clock_tree` doesn't specify the frequency,
    # we just assume it's disabled and won't be used.

    for clock in clocks:
        tree[clock] = opts(clock, 0)



    ################################################################################################################################
    #
    # Determine the flash delays and internal voltage scaling.
    #
    # TODO The settings should be dependent upon the AXI frequency,
    #      but for simplicity right now, we're using the max flash
    #      delay and highest voltage scaling. This means flash will
    #      probably be slower than needed and more power might be
    #      used unnecessarily.
    #



    match target.mcu:

        case 'STM32H7S3L8H6':

            configurations.FLASH_LATENCY            = '0x7'  # @/pg 211/tbl 29/`H7S3rm`.
            configurations.FLASH_PROGRAMMING_DELAY  = '0b11' # "
            configurations.INTERNAL_VOLTAGE_SCALING = {      # @/pg 327/sec 6.8.6/`H7S3rm`.
                'low'  : 0,
                'high' : 1,
            }['high']



        case 'STM32H533RET6':

            configurations.FLASH_LATENCY            = 5      # @/pg 252/tbl 45/`H533rm`.
            configurations.FLASH_PROGRAMMING_DELAY  = '0b10' # "
            configurations.INTERNAL_VOLTAGE_SCALING = {      # @/pg 438/sec 10.11.4/`H533rm`.
                'VOS3' : '0b00',
                'VOS2' : '0b01',
                'VOS1' : '0b10',
                'VOS0' : '0b11',
            }['VOS0']



        case _: raise NotImplementedError



    ################################################################################################################################
    #
    # Determine power-supply setup.
    #
    # TODO We currently only support a system power supply
    #      configuration of LDO. This isn't as power-efficient
    #      as compared to using the SMPS, for instance, but
    #      it's the simplest.
    #



    match target.mcu:

        case 'STM32H7S3L8H6': # @/pg 285/fig 21/`H7S3rm`. @/pg 286/tbl 44/`H7S3rm`.

            configurations.SMPS_OUTPUT_LEVEL       = None
            configurations.SMPS_FORCED_ON          = None
            configurations.SMPS_ENABLE             = False
            configurations.LDO_ENABLE              = True
            configurations.POWER_MANAGEMENT_BYPASS = False



        case 'STM32H533RET6': # @/pg 407/fig 42/`H533rm`.

            # Note that the SMPS is not available. @/pg 402/sec 10.2/`H533rm`.
            configurations.LDO_ENABLE              = True
            configurations.POWER_MANAGEMENT_BYPASS = False



        case _: raise NotImplementedError



    ################################################################################################################################
    #
    # Built-in oscillators.
    #
    # @/pg 354/fig 40/`H7S3rm`.
    #



    match target.mcu:

        case 'STM32H7S3L8H6':

            # General high-speed-internal oscillator.
            # @/pg 361/sec 7.5.2/`H7S3rm`.
            # TODO Handle other frequencies.

            configurations.HSI_ENABLE = opts('HSI_ENABLE', None)
            tree.HSI_CK               = 64_000_000 if configurations.HSI_ENABLE else 0



            # High-speed-internal oscillator (48MHz).
            # @/pg 363/sec 7.5.2/`H7S3rm`.

            configurations.HSI48_ENABLE = opts('HSI48_ENABLE', None)
            tree.HSI48_CK               = 48_000_000 if configurations.HSI48_ENABLE else 0



            # "Clock Security System" oscillator (fixed at ~4MHz).
            # @/pg 362/sec 7.5.2/`H7S3rm`.

            configurations.CSI_ENABLE = opts('CSI_ENABLE', None)
            tree.CSI_CK               = 4_000_000 if configurations.CSI_ENABLE else 0



            # TODO Not implemented yet; here because of the brute-forcing later on.
            tree.HSE_CK = 0
            tree.LSE_CK = 0



        case 'STM32H533RET6':

            # General high-speed-internal oscillator.
            # @/pg 458/sec 11.4.2/`H533rm`.
            # TODO Handle other frequencies.

            configurations.HSI_ENABLE = opts('HSI_ENABLE', None)
            tree.HSI_CK               = 32_000_000 if configurations.HSI_ENABLE else 0



            # High-speed-internal oscillator (48MHz).
            # @/pg 460/sec 11.4.4/`H533rm`.

            configurations.HSI48_ENABLE = opts('HSI48_ENABLE', None)
            tree.HSI48_CK               = 48_000_000 if configurations.HSI48_ENABLE else 0



            # "Clock Security System" oscillator (fixed at ~4MHz).
            # @/pg 459/sec 11.4.3/`H533rm`.

            configurations.CSI_ENABLE = opts('CSI_ENABLE', None)
            tree.CSI_CK               = 4_000_000 if configurations.CSI_ENABLE else 0



            # TODO Not implemented yet; here because of the brute-forcing later on.
            tree.HSE_CK = 0
            tree.LSE_CK = 0



        case _: raise NotImplementedError



    ################################################################################################################################
    #
    # Parameterize peripheral clock.
    # TODO Automate.
    #



    per_ck_source                = opts('PER_CK_SOURCE', None)
    configurations.PER_CK_SOURCE = mk_dict(database['PER_CK_SOURCE'])[per_ck_source]
    tree.PER_CK                  = tree[per_ck_source]



    ################################################################################################################################
    #
    # Parameterize the PLL subsystem.
    #
    # @/pg 371/fig 48/`H7S3rm`. @/pg 354/fig 40/`H7S3rm`.
    # @/pg 461/fig 55/`H533rm`. @/pg 456/fig 52/`H533rm`.
    #



    # Verify the values for the PLL options.

    for unit, channels in database['PLLS']:

        for channel in channels:

            goal_pll_channel_freq = opts(f'PLL{unit}_{channel}_CK', None)

            if goal_pll_channel_freq is None:
                continue # This PLL channel isn't used.

            if goal_pll_channel_freq not in database['PLL_CHANNEL_FREQ']:
                raise ValueError(
                    f'PLL{unit}{channel} output '
                    f'frequency is out-of-range: {goal_pll_channel_freq :_}Hz.'
                )



    # Brute-forcing of a single PLL channel of a particular PLL unit.

    def parameterize_plln_channel(unit, pll_vco_freq, channel):

        goal_pll_channel_freq = opts(f'PLL{unit}_{channel}_CK', None)

        if goal_pll_channel_freq is None:
            return True # This PLL channel isn't used.

        needed_divider = pll_vco_freq / goal_pll_channel_freq

        if not needed_divider.is_integer():
            return False # We won't be able to get a whole number divider.

        needed_divider = int(needed_divider)

        if needed_divider not in database[f'PLL{unit}{channel}_DIVIDER']:
            return False # The divider is not within the specified range.

        draft[f'PLL{unit}_{channel}_DIVIDER'] = needed_divider # We found a divider!

        return True



    # Yield every output frequency a PLL unit can produce given an input frequency.

    def each_pll_vco_freq(unit, pll_clock_source_freq):



        # Try every available predivider that's placed before the PLL unit.

        for draft[f'PLL{unit}_PREDIVIDER'] in database[f'PLL{unit}_PREDIVIDER']:



            # Determine the range of the PLL input frequency.

            pll_reference_freq = pll_clock_source_freq / draft[f'PLL{unit}_PREDIVIDER']

            draft[f'PLL{unit}_INPUT_RANGE'] = next((
                option
                for upper_freq_range, option in database[f'PLL{unit}_INPUT_RANGE']
                if pll_reference_freq < upper_freq_range
            ), None)

            if draft[f'PLL{unit}_INPUT_RANGE'] is None:
                continue # PLL input frequency is out of range.



            # Try every available multiplier that the PLL can handle.

            for draft[f'PLL{unit}_MULTIPLIER'] in database[f'PLL{unit}_MULTIPLIER']:

                pll_vco_freq = pll_reference_freq * draft[f'PLL{unit}_MULTIPLIER']

                if pll_vco_freq not in database['PLL_VCO_FREQ']:
                    continue # The multiplied frequency is out of range.

                yield pll_vco_freq



    # Brute-forcing of a single PLL unit.

    def parameterize_plln(unit, pll_clock_source_freq):



        # See if the PLL unit is even used.

        pll_is_used = not all(
            opts(f'PLL{unit}_{channel}_CK', None) is None
            for channel in mk_dict(database['PLLS'])[unit]
        )

        draft[f'PLL{unit}_ENABLE'] = pll_is_used

        if not pll_is_used:
            return True



        # Try to find a parameterization that satisfies each channel of the PLL unit.

        for pll_vco_freq in each_pll_vco_freq(unit, pll_clock_source_freq):

            every_plln_satisfied = all(
                parameterize_plln_channel(unit, pll_vco_freq, channel)
                for channel in mk_dict(database['PLLS'])[unit]
            )

            if every_plln_satisfied:
                return True



    # Brute-forcing every PLL unit.

    def parameterize_plls():

        match target.mcu:



            # All of the PLL units share the same clock source.

            case 'STM32H7S3L8H6':

                for pll_clock_source_name, draft.PLL_CLOCK_SOURCE in database['PLL_CLOCK_SOURCE']:

                    pll_clock_source_freq = tree[pll_clock_source_name]
                    every_pll_satisfied   = all(
                        parameterize_plln(units, pll_clock_source_freq)
                        for units, channels in database['PLLS']
                    )

                    if every_pll_satisfied:
                        return True



            # Each PLL unit have their own clock source.

            case 'STM32H533RET6':

                for unit, channels in database['PLLS']:

                    plln_satisfied = any(
                        parameterize_plln(unit, tree[plln_clock_source_name])
                        for plln_clock_source_name, draft[f'PLL{unit}_CLOCK_SOURCE'] in database[f'PLL{unit}_CLOCK_SOURCE']
                    )

                    if not plln_satisfied:
                        return False

                return True



            case _: raise NotImplementedError



    # Begin brute-forcing.

    match target.mcu:

        case 'STM32H7S3L8H6':
            draft_configuration_names = ['PLL_CLOCK_SOURCE']

        case 'STM32H533RET6':
            draft_configuration_names = [
                f'PLL{unit}_CLOCK_SOURCE'
                for unit, channels in database['PLLS']
            ]

        case _:
            raise NotImplementedError

    for unit, channels in database['PLLS']:

        for channel in channels:
            draft_configuration_names += [f'PLL{unit}_{channel}_DIVIDER']

        for suffix in ('ENABLE', 'PREDIVIDER', 'MULTIPLIER', 'INPUT_RANGE'):
            draft_configuration_names += [f'PLL{unit}_{suffix}']

    brute(parameterize_plls, draft_configuration_names)



    ################################################################################################################################
    #
    # Parameterize the System Clock Generation Unit.
    #
    # @/pg 378/fig 51/`H7S3rm`.
    #



    # Verify the values for the SCGU options.

    if tree.CPU_CK not in database['CPU_FREQ']:
        raise ValueError(
            f'CPU clock frequency is '
            f'out-of-range: {tree.CPU_CK :_}Hz.'
        )

    for unit in database['APBS']:
        if tree[f'APB{unit}_CK'] not in database['APB_FREQ']:
            raise ValueError(
                f'APB{unit} frequency is '
                f'out-of-bounds: {tree[f'APB{unit}_CK'] :_}Hz.'
            )

    match target.mcu:

        case 'STM32H7S3L8H6':

            if tree.AXI_AHB_CK not in database['AXI_AHB_FREQ']:
                raise ValueError(
                    f'Bus frequency is '
                    f'out-of-bounds: {tree.AXI_AHB_CK :_}Hz.'
                )

        case 'STM32H533RET6':

            pass



    # Brute-forcing of a single APB bus.

    def parameterize_apbx(unit):

        needed_apbx_divider         = tree.AXI_AHB_CK / tree[f'APB{unit}_CK']
        draft[f'APB{unit}_DIVIDER'] = mk_dict(database[f'APB{unit}_DIVIDER']).get(needed_apbx_divider, None)
        apbx_divider_found          = draft[f'APB{unit}_DIVIDER'] is not None

        return apbx_divider_found



    # Brute-forcing of the entire SCGU.

    def parameterize_scgu():

        for scgu_clock_source_name, draft.SCGU_CLOCK_SOURCE in database['SCGU_CLOCK_SOURCE']:



            # Try to parameterize for the CPU.

            needed_cpu_divider = tree[scgu_clock_source_name] / tree.CPU_CK
            draft.CPU_DIVIDER  = mk_dict(database['CPU_DIVIDER']).get(needed_cpu_divider, None)
            cpu_divider_found  = draft.CPU_DIVIDER is not None

            if not cpu_divider_found:
                continue



            # Try to parameterize for the AXI/AHB busses.

            match target.mcu:

                case 'STM32H7S3L8H6':

                    needed_axi_ahb_divider = tree.CPU_CK / tree.AXI_AHB_CK
                    draft.AXI_AHB_DIVIDER  = mk_dict(database['AXI_AHB_DIVIDER']).get(needed_axi_ahb_divider, None)
                    axi_ahb_divider_found  = draft.AXI_AHB_DIVIDER is not None

                    if not axi_ahb_divider_found:
                        continue



                case 'STM32H533RET6':

                    tree.AXI_AHB_CK = tree.CPU_CK



                case _: raise NotImplementedError



            # Try to parameterize for each APB bus.

            every_apb_satisfied = all(
                parameterize_apbx(unit)
                for unit in database['APBS']
            )

            if not every_apb_satisfied:
                continue



            # SCGU successfully brute-forced!

            return True



     # Begin brute-forcing.

    brute(parameterize_scgu, (
        'SCGU_CLOCK_SOURCE',
        'CPU_DIVIDER',
        'AXI_AHB_DIVIDER',
        *(f'APB{unit}_DIVIDER' for unit in database['APBS']),
    ))



    ################################################################################################################################
    #
    # Parameterize SysTick.
    #
    # @/pg 620/sec B3.3/`Armv7-M`.
    # @/pg 297/sec B11.1/`Armv8-M`.
    #



    if options.get('systick_ck', None) is not None and target.use_freertos:
        raise ValueError(
            f'FreeRTOS already uses SysTick for the time-base; '
            f'so for target {repr(target.name)}, '
            f'either remove "systick_ck" from the clock-tree '
            f'configuration or disable FreeRTOS.'
        )



    def parameterize_systick():

        draft.SYSTICK_ENABLE = tree.SYSTICK_CK != 0

        if not draft.SYSTICK_ENABLE:
            return True # SysTick won't be configured.

        for draft.SYSTICK_USE_CPU_CK in database['SYSTICK_USE_CPU_CK']:



            # SysTick will use the CPU's frequency.
            # @/pg 621/sec B3.3.3/`Armv7-M`.
            # @/pg 1859/sec D1.2.238/`Armv8-M`.

            if draft.SYSTICK_USE_CPU_CK:
                frequencies = lambda: [tree.CPU_CK]



            # SysTick will use an implementation defined clock source.

            else:

                match target.mcu:



                    case 'STM32H7S3L8H6': # @/pg 378/fig 51/`H7S3rm`.

                        frequencies = lambda: [tree.CPU_CK / 8]



                    case 'STM32H533RET6': # @/pg 456/fig 52/`H533rm`.

                        # TODO Figure out how to do this.
                        frequencies = lambda: []



                    case _: raise NotImplementedError



            # Try out the different kernel frequencies and see what sticks.

            for tree.SYSTICK_KERNEL_FREQ in frequencies():

                draft.SYSTICK_RELOAD = tree.SYSTICK_KERNEL_FREQ / tree.SYSTICK_CK - 1

                if not draft.SYSTICK_RELOAD.is_integer():
                    continue # SysTick's reload value wouldn't be a whole number.

                draft.SYSTICK_RELOAD = int(draft.SYSTICK_RELOAD)

                if draft.SYSTICK_RELOAD not in database['SYSTICK_RELOAD']:
                    continue # SysTick's reload value would be out of range.

                return True



    brute(parameterize_systick, (
        'SYSTICK_ENABLE',
        'SYSTICK_RELOAD',
        'SYSTICK_USE_CPU_CK'
    ))



    ################################################################################################################################
    #
    # Parameterize the UxART peripherals.
    # TODO Consider maximum kernel frequency.
    #
    # @/pg 394/fig 56/`H7S3rm`.
    #



    # Some UxART peripherals are tied together in hardware where
    # they would all share the same clock source. Each can still
    # have a different baud rate by changing their respective
    # baud-rate divider, but nonetheless, we must process each set
    # of connected UxART peripherals as a whole.

    for uxart_units in database['UXARTS']:



        # See if we can get the baud-divider for this UxART unit.

        def parameterize_uxart(uxart_clock_source_freq, uxart_unit):

            peripheral, number = uxart_unit
            uxartn             = f'{peripheral}{number}'



            # See if this UxART peripheral is even needed.

            needed_baud = opts(f'{uxartn}_BAUD', None)

            if needed_baud is None:
                return True



            # Check if the needed divider is valid.

            needed_divider = uxart_clock_source_freq / needed_baud

            if not needed_divider.is_integer():
                return False

            needed_divider = int(needed_divider)

            if needed_divider not in database['UXART_BAUD_DIVIDER']:
                return False



            # We found the desired divider!

            draft[f'{uxartn}_BAUD_DIVIDER'] = int(needed_divider)

            return True




        # See if we can parameterize this set of UxART peripherals.

        def parameterize_uxarts():



            # Check if this set of UxARTs even needs to be configured for.

            using_uxarts = any(
                opts(f'{peripheral}{number}_BAUD', None) is not None
                for peripheral, number in uxart_units
            )

            if not using_uxarts:
                return True



            # Try every available clock source for this
            # set of UxART peripherals and see what sticks.

            for uxart_clock_source_name, draft[f'UXART_{uxart_units}_CLOCK_SOURCE'] in database[f'UXART_{uxart_units}_CLOCK_SOURCE']:

                uxart_clock_source_freq = tree[uxart_clock_source_name]
                every_uxart_satisfied   = all(
                    parameterize_uxart(uxart_clock_source_freq, uxart_unit)
                    for uxart_unit in uxart_units
                )

                if every_uxart_satisfied:
                    return True



        # Brute force the UxART peripherals to find the needed
        # clock source and the respective baud-dividers.

        brute(parameterize_uxarts, (
            f'UXART_{uxart_units}_CLOCK_SOURCE',
            *(
                f'{peripheral}{number}_BAUD_DIVIDER'
                for peripheral, number in uxart_units
            ),
        ))



    ################################################################################################################################
    #
    # Parameterize the I2C peripherals.
    # TODO Consider maximum kernel frequency.
    #



    for unit in (database['I2CS'] if 'I2CS' in database else ()):

        def parameterize_i2c():

            for i2c_clock_source_name, draft[f'I2C{unit}_CLOCK_SOURCE'] in database[f'I2C{unit}_CLOCK_SOURCE']:

                i2c_clock_source_freq = tree[i2c_clock_source_name]

                best = None

                needed_i2c_baud = opts(f'I2C{unit}_BAUD', None)

                if needed_i2c_baud is None:
                    best = types.SimpleNamespace(
                        error = 0,
                        presc = None,
                        scl   = None,
                    )

                else:

                    for presc in database['I2C_PRESCALER']:

                        scl = round(i2c_clock_source_freq / (presc + 1) / needed_i2c_baud / 2)

                        if scl not in database['I2C_SCH'] or scl not in database['I2C_SCL']:
                            continue

                        actual_i2c_freq = i2c_clock_source_freq / (scl * 2 * (presc + 1) + 1)
                        error           = abs(1 - actual_i2c_freq / needed_i2c_baud)

                        if error > 0.01: # TODO Handle better?
                            continue

                        if best is None or error < best.error:
                            best = types.SimpleNamespace(
                                error = error,
                                presc = presc,
                                scl   = scl,
                            )

                if best is not None:
                    draft[f'I2C{unit}_PRESC'] = best.presc
                    draft[f'I2C{unit}_SCL'  ] = best.scl

                return best is not None



        brute(parameterize_i2c, (
            f'I2C{unit}_CLOCK_SOURCE',
            f'I2C{unit}_PRESC',
            f'I2C{unit}_SCL',
        ))



    ################################################################################################################################
    #
    # Parameterization of the system is done!
    #

    if leftovers := options.keys() - used_options:
        log(ANSI(
            f'[WARNING] There are leftover {target.mcu} options: {leftovers}.',
            'fg_yellow'
        ))

    return dict(configurations), tree



################################################################################################################################



# @/`About Parameterization`:
# This meta-directive figures out the register values needed
# to configure the MCU's clock-tree, but without necessarily
# worrying about the proper order that the register values
# should be written in.
#
# The latter is for `SYSTEM_CONFIGURIZE` in <./electrical/system/configurize.py>
# to do, but here in `SYSTEM_PARAMETERIZE`, we essentially
# perform brute-forcing so that we have the CPU be clocked
# at the desired frequency, the SPI clock be clocking at the
# rate we want, and so forth.
#
# As it turns out, the algorithm to brute-force the clock-tree
# the very similar across all STM32 microcontrollers. Of course,
# there are some differences, but most of the logic is heavily
# overlapped. This is especially true when we have `SYSTEM_DATABASE`
# to abstract over the details like the exact min/max frequencies
# allowed and what range is multipliers/dividers are permitted.
#
# While this meta-directive would have one of the most complicated jobs,
# what it exactly does is still pretty straight-forward and modular.
# It is divided into small, independent sections, so if you're looking
# into extending this meta-directive, look at how the existing logic
# work, copy-paste it, and adjust the logic through trial and error.
# Remember, the goal of this meta-directive is to figure out what the
# register values should be (which often mean you need to add new
# entries to the MCU's database); once you have that down, you can move
# onto `SYSTEM_CONFIGURIZE` and generate the appropriate code.
#
# This process is more time-consuming than scary really.
# Obviously, before you do any of this, you should have existing
# code that proves the parameterization and configuration works
# (e.g. manually configuring I2C clock sources and dividers before
# using meta-directives to automate it).
#
# In an ideal world, there'd be a GUI to configure the clock-tree,
# rather than what is being done now with this Python code stuff.
# This is what STM32CubeMX does, actually, but it's incredibly slow
# and hostile towards the user, so I suggest we make something better
# than Cube. This side-mission, however, will take a lot of effort
# considering the design challenges. For instance, every microcontroller
# has really niche constraints; one example might be how a SDMMC
# divider can only be an even number (or 1) if a PLL divider is 1 (or
# something like that) because a 50% duty cycle is required. Really
# oddly specific stuff like that. So if we were to create a new and
# better GUI, it'd have to be extremely flexible with its logic, but
# in doing so, we must also ensure it's absurdly fast when it comes
# to brute-forcing. Like, so fast that we always do brute-forcing every
# time a build the project! One day. Dream big.
