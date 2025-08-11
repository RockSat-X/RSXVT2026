#meta SYSTEM_PARAMETERIZE : SYSTEM_DATABASE

def SYSTEM_PARAMETERIZE(target):



    ################################################################################################################################



    # The database is how we will figure out what needs to be parameterized.

    database = SYSTEM_DATABASE[target.mcu]



    # As we parameterize, we keep track of clock frequencies we have so far in the clock-tree.
    # e.g:
    # >
    # >    tree['pll2_q_ck']   ->   200_000_000 Hz
    # >

    tree = AllocatingNamespace({ None : 0 }) # So that: tree[None] -> 0 Hz.

    def log_tree(): # Routine to help debug the clock tree if needed.
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



    # To ensure we use every option in `target.clock_tree`, we have a helper function to record every access to it
    # that we then verify at the very end; a default value can also be supplied if the option is not in `target.clock_tree`.

    options      = target.clock_tree
    used_options = OrderedSet()

    def opts(option, *default):

        nonlocal used_options



        # Mark option as used.

        used_options |= { option }



        # The default value could be None, so allow that, we need to use varadic arguments.
        # e.g:
        # >
        # >    opts('cpu_ck'      )   ->   There must be 'cpu_ck' in `target.clock_tree`.
        # >    opts('cpu_ck', None)   ->   If no 'cpu_ck' in `target.clock_tree`, then `None`.
        # >

        if len(default) >= 2:
            raise ValueError(f'Either zero or one argument should be given for the default value; got {len(default)}: {default}.')



        # We found the option in `target.clock_tree`.

        if option in options:
            return options[option]



        # Fallback to the default.

        elif default:
            return default[0]



        # No option in SYSTEM_OPTION nor fallback default!

        else:
            raise ValueError(f'For {target.mcu}, no system option "{option}" was found.')



    # The point of parameterization is to determine what the register values should be in order
    # to initialize the MCU to the specifications of `target.clock_tree`, so we'll be recording that too.
    # e.g:
    # >
    # >    configurations['pll1_q_divider']   ->   256
    # >

    configurations = AllocatingNamespace()

    def log_configurations(): # Routine to help debug the configuration record if needed.
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



    # To brute-force the clock tree, we have to try a lot of possible register values to see what sticks.
    # To do this in a convenient way, `draft` will accumulate configuration values to be eventually
    # inserted into `configurations` itself. Since `draft` is a variable at this scope-level, any function
    # can modify it, no matter how deep we might be in the brute-forcing stack-trace.

    draft = None

    def brute(function, configuration_names):

        nonlocal draft, configurations

        draft = ContainedNamespace(configuration_names)

        success = function()

        if not success:
            raise RuntimeError(f'Could not brute-force configurations that satisfies the system parameterization.')

        configurations |= draft
        draft           = None  # Clear so brute-forced configuration values won't accidentally be referenced.



    # Helper routines for database entries that are min-max ranges.
    # e.g:
    # >
    # >    (sdmmc_kernel_freq (minmax: 0 200_000_000))
    # >

    def in_minmax(value, entry):
        return entry.min <= value <= entry.max

    def range_minmax(entry):
        return range(entry.min, entry.max + 1)



    ################################################################################################################################



    # Some clock frequencies are dictated by `target.clock_tree`, so we can just immediately add them to the tree.
    # We'll check later on to make sure that the frequencies are actually solvable.

    match target.mcu:
        case 'STM32H7S3L8H6' : clocks = ['cpu_ck', 'systick_ck', 'axi_ahb_ck']
        case 'STM32H533RET6' : clocks = ['cpu_ck', 'systick_ck']
        case _               : raise NotImplementedError



    # This includes peripheral busses (e.g. APB3).

    clocks += [f'apb{n}_ck' for n in database['APB_UNITS'].value]



    # This includes each PLL channel (e.g. PLL2R).

    clocks += [f'pll{n}_{channel}_ck' for n, channels in database['PLL_UNITS'].value for channel in channels]



    # If `target.clock_tree` doesn't specify the frequency, we just assume it's disabled and won't be used.

    for clock in clocks:
        tree[clock] = opts(clock, 0)



    ################################################################################################################################
    #
    # Determine the flash delays and internal voltage scaling.
    #
    # TODO The settings should be dependent upon the AXI frequency, but for simplicity right now,
    #      we're using the max flash delay and highest voltage scaling.
    #      This means flash will probably be slower than needed and more power might be used unnecessarily.
    #



    match target.mcu:

        case 'STM32H7S3L8H6':

            configurations.flash_latency            = '0x7'  # @/pg 211/tbl 29/`H7S3rm`.
            configurations.flash_programming_delay  = '0b11' # "
            configurations.internal_voltage_scaling = {      # @/pg 327/sec 6.8.6/`H7S3rm`.
                'low'  : 0,
                'high' : 1,
            }['high']



        case 'STM32H533RET6':

            configurations.flash_latency            = 5      # @/pg 252/tbl 45/`H533rm`.
            configurations.flash_programming_delay  = '0b10' # "
            configurations.internal_voltage_scaling = {      # @/pg 438/sec 10.11.4/`H533rm`.
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
    # TODO We currently only support a system power supply configuration of LDO.
    #      This isn't as power-efficient as compared to using the SMPS, for instance,
    #      but it's the simplest.
    #



    match target.mcu:

        case 'STM32H7S3L8H6': # @/pg 285/fig 21/`H7S3rm`. @/pg 286/tbl 44/`H7S3rm`.

            configurations.smps_output_level       = None
            configurations.smps_forced_on          = None
            configurations.smps_enable             = False
            configurations.ldo_enable              = True
            configurations.power_management_bypass = False



        case 'STM32H533RET6': # @/pg 407/fig 42/`H533rm`.

            # Note that the SMPS is not available. @/pg 402/sec 10.2/`H533rm`.
            configurations.ldo_enable              = True
            configurations.power_management_bypass = False



        case _: raise NotImplementedError



    ################################################################################################################################
    #
    # Built-in oscillators.
    #
    # @/pg 354/fig 40/`H7S3rm`.
    #



    match target.mcu:

        case 'STM32H7S3L8H6':

            # General high-speed-internal oscillator. TODO Handle other frequencies.
            # @/pg 361/sec 7.5.2/`H7S3rm`.

            configurations.hsi_enable = opts('hsi_enable', None)
            tree.hsi_ck               = 64_000_000 if configurations.hsi_enable else 0



            # High-speed-internal oscillator (48MHz).
            # @/pg 363/sec 7.5.2/`H7S3rm`.

            configurations.hsi48_enable = opts('hsi48_enable', None)
            tree.hsi48_ck               = 48_000_000 if configurations.hsi48_enable else 0



            # "Clock Security System" oscillator (fixed at ~4MHz).
            # @/pg 362/sec 7.5.2/`H7S3rm`.

            configurations.csi_enable = opts('csi_enable', None)
            tree.csi_ck               = 4_000_000 if configurations.csi_enable else 0



            # TODO Not implemented yet; here because of the brute-forcing later on.

            tree.hse_ck = 0
            tree.lse_ck = 0



        case 'STM32H533RET6':

            # General high-speed-internal oscillator. TODO Handle other frequencies.
            # @/pg 458/sec 11.4.2/`H533rm`.

            configurations.hsi_enable = opts('hsi_enable', None)
            tree.hsi_ck               = 32_000_000 if configurations.hsi_enable else 0



            # High-speed-internal oscillator (48MHz).
            # @/pg 460/sec 11.4.4/`H533rm`.

            configurations.hsi48_enable = opts('hsi48_enable', None)
            tree.hsi48_ck               = 48_000_000 if configurations.hsi48_enable else 0



            # "Clock Security System" oscillator (fixed at ~4MHz).
            # @/pg 459/sec 11.4.3/`H533rm`.

            configurations.csi_enable = opts('csi_enable', None)
            tree.csi_ck               = 4_000_000 if configurations.csi_enable else 0



            # TODO Not implemented yet; here because of the brute-forcing later on.

            tree.hse_ck = 0
            tree.lse_ck = 0



        case _: raise NotImplementedError



    ################################################################################################################################
    #
    # Parameterize peripheral clock. TODO Automate.
    #



    per_ck_source                = opts('per_ck_source', None)
    configurations.per_ck_source = mk_dict(database['per_ck_source'].value)[per_ck_source]
    tree.per_ck                  = tree[per_ck_source]



    ################################################################################################################################
    #
    # Parameterize the PLL subsystem.
    #
    # @/pg 371/fig 48/`H7S3rm`. @/pg 354/fig 40/`H7S3rm`.
    # @/pg 461/fig 55/`H533rm`. @/pg 456/fig 52/`H533rm`.
    #



    # Verify the values for the PLL options.

    for pll_unit, pll_channels in database['PLL_UNITS'].value:

        for pll_channel in pll_channels:

            goal_channel_freq = opts(f'pll{pll_unit}_{pll_channel}_ck', None)

            if goal_channel_freq is None:
                continue # This PLL channel isn't used.

            if not in_minmax(goal_channel_freq, database['pll_channel_freq']):
                raise ValueError(
                    f'PLL{pll_unit}{pll_channel.upper()} output frequency is out-of-range: {goal_channel_freq :_}Hz.'
                )



    # Brute-forcing of a single PLL channel of a particular PLL unit.

    def parameterize_plln_channel(pll_unit, pll_vco_freq, pll_channel):

        goal_channel_freq = opts(f'pll{pll_unit}_{pll_channel}_ck', None)

        if goal_channel_freq is None:
            return True # This PLL channel isn't used.

        needed_divider = pll_vco_freq / goal_channel_freq

        if not needed_divider.is_integer():
            return False # We won't be able to get a whole number divider.

        needed_divider = int(needed_divider)

        if not in_minmax(needed_divider, database['pll{UNIT}{CHANNEL}_divider'][(pll_unit, pll_channel)]):
            return False # The divider is not within the specified range.

        draft[f'pll{pll_unit}_{pll_channel}_divider'] = needed_divider # We found a divider!

        return True



    # Yield every output frequency a PLL unit can produce given an input frequency.

    def each_pll_vco_freq(pll_unit, pll_clock_source_freq):



        # Try every available predivider that's placed before the PLL unit.

        for draft[f'pll{pll_unit}_predivider'] in range_minmax(database['pll{UNIT}_predivider'][pll_unit]):



             # Determine the range of the PLL input frequency.

            pll_reference_freq = pll_clock_source_freq / draft[f'pll{pll_unit}_predivider']

            draft[f'pll{pll_unit}_input_range'] = next((
                option
                for upper_freq_range, option in database['pll{UNIT}_input_range'][pll_unit].value
                if pll_reference_freq < upper_freq_range
            ), None)

            if draft[f'pll{pll_unit}_input_range'] is None:
                continue # PLL input frequency is out of range.



            # Try every available multiplier that the PLL can handle.

            for draft[f'pll{pll_unit}_multiplier'] in range_minmax(database['pll{UNIT}_multiplier'][pll_unit]):

                pll_vco_freq = pll_reference_freq * draft[f'pll{pll_unit}_multiplier']

                if not in_minmax(pll_vco_freq, database['pll_vco_freq']):
                    continue # The multiplied frequency is out of range.

                yield pll_vco_freq



    # Brute-forcing of a single PLL unit.

    def parameterize_plln(pll_unit, pll_clock_source_freq):



        # See if the PLL unit is even used.

        pll_is_used = not all(
            opts(f'pll{pll_unit}_{pll_channel}_ck', None) is None
            for pll_channel in mk_dict(database['PLL_UNITS'].value)[pll_unit]
        )

        draft[f'pll{pll_unit}_enable'] = pll_is_used

        if not pll_is_used:
            return True



        # Try to find a parameterization that satisfies each channel of the PLL unit.

        for pll_vco_freq in each_pll_vco_freq(pll_unit, pll_clock_source_freq):

            every_plln_satisfied = all(
                parameterize_plln_channel(pll_unit, pll_vco_freq, pll_channel)
                for pll_channel in mk_dict(database['PLL_UNITS'].value)[pll_unit]
            )

            if every_plln_satisfied:
                return True



    # Brute-forcing every PLL unit.

    def parameterize_plls():

        match target.mcu:



            # All of the PLL units share the same clock source.

            case 'STM32H7S3L8H6':

                for pll_clock_source_name, draft.pll_clock_source in database['pll_clock_source'].value:

                    pll_clock_source_freq = tree[pll_clock_source_name]
                    every_pll_satisfied   = all(
                        parameterize_plln(units, pll_clock_source_freq)
                        for units, channels in database['PLL_UNITS'].value
                    )

                    if every_pll_satisfied:
                        return True



            # Each PLL unit have their own clock source.

            case 'STM32H533RET6':

                for pll_unit, pll_clock_sources in database['pll{UNIT}_clock_source'].items():

                    plln_satisfied = any(
                        parameterize_plln(pll_unit, tree[pll_clock_source_name])
                        for pll_clock_source_name, draft[f'pll{pll_unit}_clock_source'] in pll_clock_sources.value
                    )

                    if not plln_satisfied:
                        return False

                return True



            case _: raise NotImplementedError



    # Begin brute-forcing.

    match target.mcu:
        case 'STM32H7S3L8H6' : draft_configuration_names = ['pll_clock_source']
        case 'STM32H533RET6' : draft_configuration_names = [f'pll{unit}_clock_source' for unit, channels in database['PLL_UNITS'].value]
        case _               : raise NotImplementedError

    for unit, channels in database['PLL_UNITS'].value:

        for channel in channels:
            draft_configuration_names += [f'pll{unit}_{channel}_divider']

        for suffix in ('enable', 'predivider', 'multiplier', 'input_range'):
            draft_configuration_names += [f'pll{unit}_{suffix}']

    brute(parameterize_plls, draft_configuration_names)



    ################################################################################################################################
    #
    # Parameterize the System Clock Generation Unit.
    #
    # @/pg 378/fig 51/`H7S3rm`.
    #



    # Verify the values for the SCGU options.

    if not in_minmax(tree.cpu_ck, database['cpu_freq']):
        raise ValueError(f'CPU clock frequency is out-of-range: {tree.cpu_ck :_}Hz.')

    for unit in database['APB_UNITS'].value:
        if not in_minmax(tree[f'apb{unit}_ck'], database['apb_freq']):
            raise ValueError(f'APB{unit} frequency is out-of-bounds: {tree[f'apb{unit}_ck'] :_}Hz.')

    match target.mcu:

        case 'STM32H7S3L8H6':

            if not in_minmax(tree.axi_ahb_ck, database['axi_ahb_freq']):
                raise ValueError(f'Bus frequency is out-of-bounds: {tree.axi_ahb_ck :_}Hz.')

        case 'STM32H533RET6':

            pass



    # Brute-forcing of a single APB bus.

    def parameterize_apbx(unit):

        needed_apbx_divider         = tree.axi_ahb_ck / tree[f'apb{unit}_ck']
        draft[f'apb{unit}_divider'] = mk_dict(database['apb{UNIT}_divider'][unit].value).get(needed_apbx_divider, None)
        apbx_divider_found          = draft[f'apb{unit}_divider'] is not None

        return apbx_divider_found



    # Brute-forcing of the entire SCGU.

    def parameterize_scgu():

        for scgu_clock_source_name, draft.scgu_clock_source in database['scgu_clock_source'].value:



            # Try to parameterize for the CPU.

            needed_cpu_divider = tree[scgu_clock_source_name] / tree.cpu_ck
            draft.cpu_divider  = mk_dict(database['cpu_divider'].value).get(needed_cpu_divider, None)
            cpu_divider_found  = draft.cpu_divider is not None

            if not cpu_divider_found:
                continue



            # Try to parameterize for the AXI/AHB busses.

            match target.mcu:

                case 'STM32H7S3L8H6':

                    needed_axi_ahb_divider = tree.cpu_ck / tree.axi_ahb_ck
                    draft.axi_ahb_divider  = mk_dict(database['axi_ahb_divider'].value).get(needed_axi_ahb_divider, None)
                    axi_ahb_divider_found  = draft.axi_ahb_divider is not None

                    if not axi_ahb_divider_found:
                        continue



                case 'STM32H533RET6':

                    tree.axi_ahb_ck = tree.cpu_ck



                case _: raise NotImplementedError



            # Try to parameterize for each APB bus.

            every_apb_satisfied = all(parameterize_apbx(unit) for unit in database['APB_UNITS'].value)

            if not every_apb_satisfied:
                continue



            # SCGU successfully brute-forced!

            return True



     # Begin brute-forcing.

    brute(parameterize_scgu, (
        'scgu_clock_source',
        'cpu_divider',
        'axi_ahb_divider', # TODO.
        *(f'apb{unit}_divider' for unit in database['APB_UNITS'].value),
    ))



    ################################################################################################################################
    #
    # Parameterize SysTick.
    #
    # @/pg 620/sec B3.3/`Armv7-M`.
    # @/pg 297/sec B11.1/`Armv8-M`.
    #



    def parameterize_systick():

        draft.systick_enable = tree.systick_ck != 0

        if not draft.systick_enable:
            return True # SysTick won't be configured.

        for draft.systick_use_cpu_ck in database['systick_use_cpu_ck'].value:



            # SysTick will use the CPU's frequency.
            # @/pg 621/sec B3.3.3/`Armv7-M`.
            # @/pg 1859/sec D1.2.238/`Armv8-M`.

            if draft.systick_use_cpu_ck:
                frequencies = lambda: [tree.cpu_ck]



            # SysTick will use an implementation defined clock source.

            else:

                match target.mcu:



                    case 'STM32H7S3L8H6': # @/pg 378/fig 51/`H7S3rm`.

                        frequencies = lambda: [tree.cpu_ck / 8]



                    case 'STM32H533RET6': # @/pg 456/fig 52/`H533rm`.

                        # TODO Figure out how to do this.
                        frequencies = lambda: []



                    case _: raise NotImplementedError



            # Try out the different kernel frequencies and see what sticks.

            for tree.systick_kernel_freq in frequencies():

                draft.systick_reload = tree.systick_kernel_freq / tree.systick_ck - 1

                if not draft.systick_reload.is_integer():
                    continue # SysTick's reload value wouldn't be a whole number.

                draft.systick_reload = int(draft.systick_reload)

                if not in_minmax(draft.systick_reload, database['systick_reload']):
                    continue # SysTick's reload value would be out of range.

                return True



    brute(parameterize_systick, ('systick_enable', 'systick_reload', 'systick_use_cpu_ck'))



    ################################################################################################################################
    #
    # Parameterize the UxART peripherals. TODO Consider maximum kernel frequency.
    #
    # @/pg 394/fig 56/`H7S3rm`.
    #



    # Some UxART peripherals are tied together in hardware where they would all share the same clock source.
    # Each can still have a different baud rate by changing their respective baud-rate divider,
    # but nonetheless, we must process each set of connected UxART peripherals as a whole.

    for uxart_units in database['UXARTS'].value:

        ns = '_'.join(str(number) for peripheral, number in uxart_units)



        # See if we can get the baud-divider for this UxART unit.

        def parameterize_uxart(uxart_clock_source_freq, uxart_unit):

            peripheral, number = uxart_unit
            uxartn             = f'{peripheral}{number}'



            # See if this UxART peripheral is even needed.

            needed_baud = opts(f'{uxartn}_baud', None)

            if needed_baud is None:
                return True



            # Check if the needed divider is valid.

            needed_divider = uxart_clock_source_freq / needed_baud

            if not needed_divider.is_integer():
                return False

            needed_divider = int(needed_divider)

            if not in_minmax(needed_divider, database['uxart_baud_divider']):
                return False



            # We found the desired divider!

            draft[f'{uxartn}_baud_divider'] = int(needed_divider)

            return True




        # See if we can parameterize this set of UxART peripherals.

        def parameterize_uxarts():



            # Check if this set of UxARTs even needs to be configured for.

            using_uxarts = any(
                opts(f'{peripheral}{number}_baud', None) is not None
                for peripheral, number in uxart_units
            )

            if not using_uxarts:
                return True



            # Try every available clock source for this set of UxART peripherals and see what sticks.

            for uxart_clock_source_name, draft[f'uxart_{ns}_clock_source'] in database['uxart_{UNITS}_clock_source'][uxart_units].value:

                uxart_clock_source_freq = tree[uxart_clock_source_name]
                every_uxart_satisfied   = all(parameterize_uxart(uxart_clock_source_freq, uxart_unit) for uxart_unit in uxart_units)

                if every_uxart_satisfied:
                    return True



        # Brute force the UxART peripherals to find the needed
        # clock source and the respective baud-dividers.

        brute(parameterize_uxarts, (
            f'uxart_{ns}_clock_source',
            *(f'{peripheral}{number}_baud_divider' for peripheral, number in uxart_units),
        ))



    ################################################################################################################################
    #
    # Parameterization of the system is done!
    #

    if leftovers := options.keys() - used_options:
        log(ANSI(f'[WARNING] There are leftover {target.mcu} options: {leftovers}.', 'fg_yellow'))

    return dict(configurations), tree
