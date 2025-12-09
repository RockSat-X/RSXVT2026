#! /usr/bin/env python3

import types, pathlib, collections

def checkPCBs(
    *,
    require,
    ExitCode,
    build,
    log,
    COUPLED_CONNECTORS,
    root,
    execute,
    TARGETS,
    ANSI,
    Indent,
    justify
):

    import deps.stpy.pxd.sexp
    import deps.stpy.mcus

    require('kicad-cli')



    # We must run the meta-preprocessor first to
    # verify every target's GPIO parameterization.

    try: # TODO Bum hack with the UI module...

        exit_code = build(types.SimpleNamespace(
            target              = None,
            metapreprocess_only = True,
        ))

        if exit_code:
            raise ExitCode(exit_code)

    except ExitCode as exit_code:
        if exit_code.args[0]:
            raise

    log()



    connectors = {
        connector_name : []
        for connector_name in COUPLED_CONNECTORS
    }




    kicad_projects = [
        pathlib.Path(file_name).stem
        for root, directories, file_names in root('./pcb').walk()
        for file_name in file_names
        if file_name.endswith('.kicad_pro')
    ]

    for kicad_project in kicad_projects:

        netlist_file_path = pathlib.Path(f'{root('./build', kicad_project).as_posix()}.net')

        execute(f'''
            kicad-cli
                sch export netlist "{root('./pcb', f'{kicad_project}.kicad_sch').as_posix()}"
                --output "{netlist_file_path.as_posix()}"
                --format orcadpcb2
        ''') # `kicad-cli` can't be invoked in parallel because of some lock file for some reason.

        log()

        sexp = netlist_file_path.read_text()
        sexp = sexp.removesuffix('*\n') # Not sure why there's a trailing asterisk.
        sexp = deps.stpy.pxd.sexp.parse_sexp(sexp)

        for entry in sexp:
            match entry:
                case uid, footprint_name, reference, value, *netlist:
                    if footprint_name in (f'pcb:{connector}' for connector in connectors):
                        connectors[footprint_name.removeprefix('pcb:')] += [(kicad_project, netlist)]

        for target in TARGETS:

            pinouts = deps.stpy.mcus.MCUS[target.mcu].pinouts

            if target.schematic_file_path != root('./pcb', f'{kicad_project}.kicad_sch'):
                continue


            # Find the MCU's netlist.

            matches = []

            for entry in sexp:
                match entry:
                    case uid, footprint_name, reference, value, *nets:
                        if value == target.mcu:
                            matches += [nets]

            if not matches:
                log(ANSI(
                    f'[ERROR] No symbol with value of {repr(target.mcu)} '
                    f'was found in {repr(target.schematic_file_path.as_posix())}!',
                    'fg_red'
                ))
                raise ExitCode(1)

            if len(matches) >= 2:
                log(ANSI(
                    f'Multiple symbols with value of {repr(target.mcu)} '
                    f'were found in {repr(target.schematic_file_path.as_posix())}!',
                    'fg_red'
                ))
                raise ExitCode(1)


            # The pin position is sometimes just a number,
            # but for some packages like BGA, it might be a 2D
            # coordinate (letter-number pair like 'J7').
            # The s-exp parser will parse the 1D coordinate as
            # an actual integer but the 2D coordinate as a string.
            # Thus, to keep things consistent, we always convert
            # the position back into a string.

            netlist, = matches
            netlist  = {
                str(position) : net
                for position, net in netlist
            }



            # We look for discrepancies between the
            # netlist and the target's GPIO list.

            issues = []

            for pin_position, pin_net in netlist.items():



                # Skip unused pins.

                if pin_net.startswith('unconnected-('):
                    continue



                # Skip things like power pins.

                if pinouts[pin_position].type != 'I/O':
                    continue



                # Try to find the corresponding GPIO used by the target.

                gpio_name = [
                    gpio_name
                    for gpio_name, gpio_pin, gpio_type, gpio_settings in target.gpios
                    if f'P{gpio_pin}' == pinouts[pin_position].name
                ]

                if gpio_name:

                    gpio_name, = gpio_name



                    # The schematic and target disagree on GPIO.
                    # Note that KiCad prepends a '/' if the net
                    # is defined by a local net label.

                    if gpio_name != pin_net.removeprefix('/'):
                        issues += [
                            f'Pin {repr(pinouts[pin_position].name)} ({repr(gpio_name)}) '
                            f'has net {repr(pin_net)}.'
                        ]



                # Extraneous GPIO in the schematic.

                else:

                    issues += [
                        f'Pin {repr(pinouts[pin_position].name)} ({repr(pin_net)}) '
                        f'is not defined for the target.'
                    ]



            # We check to see if the target has any GPIOs that
            # are not in the schematic. We don't have to check
            # if the GPIO name and net match up because we did
            # that already.

            for gpio_name, gpio_pin, gpio_type, gpio_settings in target.gpios:

                if gpio_pin is None:
                    continue

                pin_position, = [
                    pin_position
                    for pin_position, pin in pinouts.items()
                    if pin.name == f'P{gpio_pin}'
                ]

                pin_net = netlist[pin_position]

                if pin_net.startswith('unconnected-('):

                    issues += [
                        f'Pin {repr(pinouts[pin_position].name)} ({repr(gpio_name)}) '
                        f'is unconnected in the schematic.'
                    ]



            # Report all the issues we found.

            if issues:

                with ANSI('fg_yellow'), Indent('[WARNING] ', hanging = True):

                    log(
                        f'For target {repr(target.name)} and '
                        f'schematic {repr(target.schematic_file_path.as_posix())}:'
                    )

                    for issue in issues:
                        log(f'    - {issue}')

                log()



    for connector_name, kicad_projects_netlist in connectors.items():

        pins = collections.defaultdict(lambda: {})

        for kicad_project, netlist in kicad_projects_netlist:

            for position, net_name in netlist:

                pins[position][kicad_project] = net_name


        for position, usages in pins.items():

            assert set(kicad_project for kicad_project, netlist in kicad_projects_netlist) == set(usages.keys())

            connected_net_names = [net_name for net_name in usages.values() if not net_name.startswith('unconnected-')]

            if (len(set(connected_net_names)) <= 1 and len(connected_net_names) != 1):
                continue

            with ANSI('fg_yellow'), Indent('[WARNING] ', hanging = True):

                log(f'Pin {position} of coupled connector {repr(connector_name)} has issues:')

                for just_kicad_project, just_net_name in justify(
                    (
                        ('<', repr(kicad_project)),
                        ('<', repr(net_name     )),
                    )
                    for kicad_project, net_name in usages.items()
                ):
                    log(f'- {just_kicad_project} : {just_net_name}')

                log()
