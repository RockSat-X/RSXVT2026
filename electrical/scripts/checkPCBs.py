#! /usr/bin/env python3

import types, pathlib, collections, logging

logger = logging.getLogger(__name__)

def checkPCBs(
    *,
    make_sure_shell_command_exists,
    make_main_relative_path,
    execute_shell_command,
    run_metapreprocessor,
    mcu_pinouts,
    TARGETS,
):

    DIRECTORY_PATH_OF_KICAD_PROJECTS = make_main_relative_path('./pcb')
    DIRECTORY_PATH_OF_OUTPUTS        = make_main_relative_path('./build')



    # We'll be relying on KiCad's CLI program to generate the netlist of schematics.

    make_sure_shell_command_exists('kicad-cli')



    # Some of the targets' GPIOs configuration will be verified
    # when we run the meta-preprocessor; things like invalid GPIO
    # pin port-number settings will be caught first.

    run_metapreprocessor()



    # TODO Automatically find coupled connectors.

    coupled_connectors = {
        name : collections.defaultdict(lambda: [])
        for name in (
            'MainStackConnector',
            'VehicleStackConnector',
        )
    }



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
                str(coordinate) : net_name
                for coordinate, net_name in mcu_nets
            }



            # We look for discrepancies between the
            # netlist and the target's GPIO list.

            for mcu_pin_coordinate, mcu_pin_net_name in mcu_nets.items():



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
                        if not mcu_pin_net_name.startswith('unconnected-('):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should be '
                                f'unconnected in the schematic, '
                                f'but instead has net {repr(mcu_pin_net_name)}.',
                            )



                    # GPIO found, so this MCU pin's net name should match
                    # the GPIO name. The net name sometimes starts with a
                    # `/` to indicate it's a local net.

                    case [gpio_name]:

                        if mcu_pin_net_name.startswith('unconnected-('):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should have '
                                f'net {repr(gpio_name)} in the schematic, '
                                f'but instead is unconnected.'
                            )

                        elif gpio_name != mcu_pin_net_name.removeprefix('/'):
                            logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should have '
                                f'net {repr(gpio_name)} in the schematic, '
                                f'but instead has net {repr(mcu_pin_net_name)}.'
                            )



                    case _: raise RuntimeError



        # Find any coupled connectors in the schematic.

        for uid, footprint, reference, value, *nets in netlist_sexp:

            if (key := footprint.removeprefix('pcb:')) in coupled_connectors:

                for coordinate, net_name in nets:

                    coupled_connectors[key][coordinate] += [(schematic_file_path, net_name)]



    # Check for any discrepancies between coupled connectors.

    for coupled_connector_name, coupled_pins in coupled_connectors.items():

        for coordinate, schematic_net_name_pairs in coupled_pins.items():



            # This pin only has one net tied to it.

            if len([
                net_name
                for schematic_file_path, net_name in schematic_net_name_pairs
                if not net_name.startswith('unconnected-')
            ]) == 1:

                logger.warning(
                    f'Coupled connector {repr(coupled_connector_name)} '
                    f'on pin {repr(coordinate)} only has one net associated with it.',
                    extra = {
                        'table' : [
                            (repr(schematic_file_path.name), repr(net_name))
                            for schematic_file_path, net_name in schematic_net_name_pairs
                        ]
                    }
                )



            # This pin has conflicting nets tied to it.

            if len(set(
                net_name
                for schematic_file_path, net_name in schematic_net_name_pairs
                if not net_name.startswith('unconnected-')
            )) >= 2:

                logger.warning(
                    f'Coupled connector {repr(coupled_connector_name)} '
                    f'on pin {repr(coordinate)} has conflicting nets associated with it.',
                    extra = {
                        'table' : [
                            (repr(schematic_file_path.name), repr(net_name))
                            for schematic_file_path, net_name in schematic_net_name_pairs
                        ]
                    }
                )
