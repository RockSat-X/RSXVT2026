#! /usr/bin/env python3

import types, pathlib, collections, logging

logger = logging.getLogger(__name__)



def checkPCBs(
    *,
    make_sure_shell_command_exists,
    make_main_relative_path,
    execute_shell_command,
    preverify_gpios,
    mcu_pinouts,
    TARGETS,
):

    DIRECTORY_PATH_OF_KICAD_PROJECTS = make_main_relative_path('./pcb')
    DIRECTORY_PATH_OF_OUTPUTS        = make_main_relative_path('./build')

    make_sure_shell_command_exists('kicad-cli')

    preverify_gpios()



    # Get the names of all KiCad projects.

    kicad_projects = [
        file_name.removesuffix(suffix)
        for _, _, file_names in DIRECTORY_PATH_OF_KICAD_PROJECTS.walk()
        for       file_name  in file_names
        if file_name.endswith(suffix := '.kicad_pro')
    ]



    # Process all KiCad projects.

    for kicad_project in kicad_projects:



         # Get the netlist of the KiCad project.
         #
         # Note that `kicad-cli` can't be invoked
         # in parallel because of some lock file
         # for some reason.

        schematic_file_path = DIRECTORY_PATH_OF_KICAD_PROJECTS / f'{kicad_project}.kicad_sch'
        netlist_file_path   = DIRECTORY_PATH_OF_OUTPUTS        / f'{kicad_project}.net'

        execute_shell_command(f'''
            kicad-cli
                sch export netlist "{schematic_file_path.as_posix()}"
                --output "{netlist_file_path.as_posix()}"
                --format orcadpcb2
        ''')



        # Parse the outputted netlist file.

        import deps.stpy.pxd.sexp

        netlist_sexp = netlist_file_path.read_text()
        netlist_sexp = netlist_sexp.removesuffix('*\n') # Trailing asterisk for some reason.
        netlist_sexp = deps.stpy.pxd.sexp.parse_sexp(netlist_sexp)

        match netlist_sexp:
            case ('{', 'EESchema', 'Netlist', 'Version', _, 'created', _, '}', *rest):
                netlist_sexp = rest



        # Find targets that are associated with the KiCad project.

        applicable_targets = [
            target
            for target in TARGETS
            if target.kicad_project == kicad_project
        ]



        # Find any discrepancies between the target's GPIOs and the schematic.

        for target in applicable_targets:



            # Find the one MCU in the schematic and get its nets.

            match [
                nets
                for uid, footprint, reference, value, *nets in netlist_sexp
                if value == target.mcu
            ]:



                # The MCU is missing from the schematic.

                case []:
                    logger.warning(
                        f'For target {repr(target.name)} '
                        f'and schematic {repr(schematic_file_path.as_posix())}, '
                        f'symbol {repr(target.mcu)} is missing.'
                    )
                    continue



                # The MCU was found.

                case [mcu_nets]:
                    pass



                # There are multiple MCUs.

                case _:
                    logger.warning(
                        f'For target {repr(target.name)} '
                        f'and schematic {repr(schematic_file_path.as_posix())}, '
                        f'multiple {repr(target.mcu)} are found.'
                    )
                    continue



            # The pin coordinate is sometimes just a number,
            # but for some packages like BGA, it might be a 2D
            # coordinate (letter-number pair like 'J7').
            # The s-exp parser will parse the 1D coordinate as
            # an actual integer but the 2D coordinate as a string.
            # Thus, to keep things consistent, we always convert
            # the coordinate back into a string.

            mcu_nets = {
                str(coordinate) : net
                for coordinate, net in mcu_nets
            }



            # We look for discrepancies between the
            # netlist and the target's GPIO list.

            for mcu_pin_coordinate, mcu_pin_net in mcu_nets.items():



                # Skip things like power pins.

                if mcu_pinouts[target.mcu][mcu_pin_coordinate].type != 'I/O':
                    continue



                # Get the port-number name (e.g. `A3`).

                mcu_pin_port_number = mcu_pinouts[target.mcu][mcu_pin_coordinate].name.removeprefix('P')



                # Find the corresponding GPIO for the target, if any.

                match [
                    gpio_name
                    for gpio_name, gpio_pin, gpio_type, gpio_settings in target.gpios
                    if gpio_pin == mcu_pin_port_number
                ]:



                    # No GPIO found, so this MCU pin should be unconnected.

                    case []:
                        if not mcu_pin_net.startswith('unconnected-('):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should be '
                                f'unconnected in the schematic, '
                                f'but instead has net {repr(mcu_pin_net)}.',
                            )



                    # GPIO found, so this MCU pin's net name should match
                    # the GPIO name. The net name sometimes starts with a
                    # `/` to indicate it's a local net.

                    case [gpio_name]:

                        if mcu_pin_net.startswith('unconnected-('):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should have '
                                f'net {repr(gpio_name)} in the schematic, '
                                f'but instead is unconnected.'
                            )

                        elif gpio_name != mcu_pin_net.removeprefix('/'):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should have '
                                f'net {repr(gpio_name)} in the schematic, '
                                f'but instead has net {repr(mcu_pin_net)}.'
                            )



                    case _: raise RuntimeError



#        connectors = {
#            connector_name : []
#            for connector_name in (
#                'MainStackConnector',
#                'VehicleStackConnector',
#            )
#        }
#
#        for entry in netlist_sexp:
#
#            match entry:
#
#                case (uid, footprint, reference, value, *net_mapping):
#
#                    if any(footprint == f'pcb:{connector}' for connector in connectors):
#
#                        connectors[footprint.removeprefix('pcb:')] += [(kicad_project, net_mapping)]
#
#
#
#
#    for connector_name, kicad_projects_netlist in connectors.items():
#
#        pins = collections.defaultdict(lambda: {})
#
#        for kicad_project, netlist in kicad_projects_netlist:
#
#            for coordinate, net_name in netlist:
#
#                pins[coordinate][kicad_project] = net_name
#
#
#        for coordinate, usages in pins.items():
#
#            assert set(kicad_project for kicad_project, netlist in kicad_projects_netlist) == set(usages.keys())
#
#            connected_net_names = [net_name for net_name in usages.values() if not net_name.startswith('unconnected-')]
#
#            if (len(set(connected_net_names)) <= 1 and len(connected_net_names) != 1):
#                continue
#
#            with ANSI('fg_yellow'), Indent('[WARNING] ', hanging = True):
#
#                log(f'Pin {coordinate} of coupled connector {repr(connector_name)} has issues:')
#
#                for just_kicad_project, just_net_name in justify(
#                    (
#                        ('<', repr(kicad_project)),
#                        ('<', repr(net_name     )),
#                    )
#                    for kicad_project, net_name in usages.items()
#                ):
#                    log(f'- {just_kicad_project} : {just_net_name}')
#
#                log()
