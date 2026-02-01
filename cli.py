#! /usr/bin/env python3



# The PXD module.

import sys

try:

    import deps.stpy.pxd.pxd as pxd

except ModuleNotFoundError as error:

    if not any(module in error.name for module in ('deps', 'stpy', 'pxd')):
        raise # Likely a bug in the PXD module.

    import traceback

    print()
    traceback.print_exc()
    print()

    print(
        f'Could not import "{error.name}"; maybe the Git '
        f'submodules need to be initialized/updated? Try doing:' '\n'
        f'> git submodule update --init --recursive'             '\n'
        f'If this still doesn\'t work, please raise an issue.'   '\n'
    )

    sys.exit(1)



# Built-in modules.

import types, pathlib, shutil, subprocess, time, io, struct



# Import declarations that's
# shared with the meta-preprocessor.

from electrical.Shared import *



# Import handlers for non-builtin modules.

def import_pyserial():

    try:

        import serial
        import serial.tools.list_ports

    except ModuleNotFoundError as error:

        pxd.pxd_logger.error(
            f'Python got {type(error).__name__} ({error}); try doing:' '\n'
            f'> pip install pyserial'
        )

        sys.exit(1)

    return serial, serial.tools.list_ports

def import_pygame():

    try:

        import pygame

    except ModuleNotFoundError as error:

        pxd.pxd_logger.error(
            f'Python got {type(error).__name__} ({error}); try doing:' '\n'
            f'> pip install pygame'
        )

        sys.exit(1)

    try:

        import pygame_gui

    except ModuleNotFoundError as error:

        pxd.pxd_logger.error(
            f'Python got {type(error).__name__} ({error}); try doing:' '\n'
            f'> pip install pygame_gui'
        )

        sys.exit(1)

    return pygame, pygame_gui



# Routine for ensuring the user has the required programs
# on their machine (and provide good error messages if not).

def make_sure_shell_command_exists(*needed_programs):



    # Search for a missing required program.

    for missing_program in needed_programs:
        if shutil.which(missing_program) is None:
            break
    else:
        return



    # PowerShell.

    if missing_program in (roster := [
        'pwsh',
    ]):

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed PowerShell yet?' '\n'
            f'> https://apps.microsoft.com/detail/9MZ1SNWT0N5D'                                           '\n'
            f'Installing PowerShell via Windows Store is the most convenient way.'                        '\n'
        )



    # STM32CubeCLT.

    elif missing_program in (roster := [
        'STM32_Programmer_CLI',
        'ST-LINK_gdbserver',
        'arm-none-eabi-gcc',
        'arm-none-eabi-cpp',
        'arm-none-eabi-objcopy',
        'arm-none-eabi-gdb',
    ]):

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed STM32CubeCLT 1.19.0 yet?' '\n'
            f'> https://www.st.com/en/development-tools/stm32cubeclt.html'                                         '\n'
            f'Note that the installation is behind a login-wall.'                                                  '\n'
            f'Install and then make sure all of these commands are available in your PATH:'                        '\n',
            extra = {
                'table' : [
                    (f'> {program}', ('missing' if shutil.which(program) is None else 'located'))
                    for program in roster
                ]
            }
        )



    # Picocom.

    elif missing_program in (roster := [
        'picocom',
    ]):

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?'  '\n'
            f'If you\'re on a Debian-based distro, this is just simply:'                           '\n'
            f'> sudo apt install picocom'                                                          '\n'
            f'Otherwise, you should only be getting this message on some other Linux distribution' '\n'
            f'to which I have faith in you to figure this out on your own.'                        '\n'
        )



    # Make.

    elif missing_program in (roster := [
        'make',
    ]):

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?' '\n'
            f'If you\'re on a Windows system, run the following command and restart your shell:'  '\n'
            f'> winget install ezwinports.make'                                                   '\n'
        )



    # KiCad-CLI.

    elif missing_program in (roster := [
        'kicad-cli',
    ]):

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH.'                           '\n'
            f'This program comes along with KiCad, so on Windows,'                               '\n'
            f'you might find it at "C:\\Program Files\\KiCad\\9.0\\bin\\{missing_program}.exe".' '\n'
            f'Thus, add something like "C:\\Program Files\\KiCad\\9.0\\bin" to your PATH.'       '\n'
        )



    # Explanation not implemented.

    else:

        pxd.pxd_logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?'
        )



    sys.exit(1)



# Routine for logging out ST-Links as a table.

def logger_stlinks(stlinks):

    pxd.pxd_logger.info(
        '\n'.join(
            f'| {' | '.join(justs)} |'
            for justs in pxd.justify(
                [
                    (
                        ('<', 'Probe Index'  ),
                        ('<', 'Board Name'   ),
                        ('<', 'Device'       ),
                        ('<', 'Description'  ),
                        ('<', 'Serial Number'),
                    ),
                ] + [
                    (
                        ('<', stlink.probe_index        ),
                        ('<', stlink.board_name         ),
                        ('<', stlink.comport.device     ),
                        ('<', stlink.comport.description),
                        ('<', stlink.serial_number      ),
                    )
                    for stlink in stlinks
                ]
            )
        )
    )



# Routine for finding ST-Links.

def request_stlinks(
    *,
    specific_one         = False,
    specific_probe_index = None,
    stlink_error_ok      = False,
):



    # Use STM32_Programmer_CLI to find any connected ST-Links.

    make_sure_shell_command_exists('STM32_Programmer_CLI')

    listing_lines = subprocess.check_output(f'''
        STM32_Programmer_CLI --list st-link
    '''.split()).decode('utf-8').splitlines()



    # Sometimes ST-Links can lock-up.

    if any('ST-LINK error' in line for line in listing_lines):

        pxd.pxd_logger.error(
            f'There seems to be an error with an ST-Link; '
            f'this could be because another program is using the ST-Link. '
            f'See the output of STM32_Programmer_CLI below:' '\n'
            f'\n'
            f'{'\n'.join(listing_lines)}'
        )

        if stlink_error_ok:
            return None
        else:
            sys.exit(1)



    # Parse output of STM32_Programmer_CLI's findings.
    # e.g:
    # >
    # >    ST-Link Probe 0 :
    # >       ST-LINK SN  : 003F00493133510F37363734
    # >       ST-LINK FW  : V3J15M7
    # >       Access Port Number  : 2
    # >       Board Name  : NUCLEO-H7S3L8
    # >

    stlinks = [
        types.SimpleNamespace(
            probe_index        = int(listing_lines[i + 0].removeprefix(prefix).removesuffix(':')),
            serial_number      =     listing_lines[i + 1].split(':')[1].strip(),
            firmware           =     listing_lines[i + 2].split(':')[1].strip(),
            access_port_number = int(listing_lines[i + 3].split(':')[1].strip()),
            board_name         =     listing_lines[i + 4].split(':')[1].strip(),
        )
        for i in range(len(listing_lines))
        if listing_lines[i].startswith(prefix := 'ST-Link Probe ')
    ]



    # Find each ST-Link's corresponding serial port.

    serial, list_ports = import_pyserial()

    comports = list_ports.comports()

    for stlink in stlinks:
        stlink.comport, = [
            comport
            for comport in comports
            if comport.serial_number == stlink.serial_number
        ]



    # The caller expects a specific ST-Link.

    if specific_probe_index is not None:



        # There's no ST-Links!

        if not stlinks:
            pxd.pxd_logger.error(f'No ST-Links found.')
            sys.exit(1)



        # There's no matching ST-Links!

        if not (matches := [stlink for stlink in stlinks if stlink.probe_index == specific_probe_index]):
            logger_stlinks(stlinks)
            pxd.pxd_logger.error(f'No ST-Links found with probe index of "{specific_probe_index}".')
            sys.exit(1)



        # Give back the desired ST-Link.

        stlink, = matches

        return stlink



    # If the caller is assuming there's only one
    # ST-Link, then give back the only one.

    if specific_one:



        # There's no ST-Links!

        if not stlinks:
            pxd.pxd_logger.error(f'No ST-Links found.')
            sys.exit(1)



        # There's too many ST-Links!

        if len(stlinks) >= 2:
            logger_stlinks(stlinks)
            pxd.pxd_logger.error(f'Multiple ST-Links found; I don\'t know which one to use.')
            sys.exit(1)



        # Give back the only ST-Link.

        stlink, = stlinks

        return stlink



    # Otherwise, give back the list of
    # the ST-Links we found (if any).

    return stlinks



################################################################################



main_interface = pxd.CommandLineInterface()



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Delete all build artifacts.',
    },
)
def clean(parameters):

    DIRECTORIES = (
        pxd.make_main_relative_path('./build'),
        pxd.make_main_relative_path('./electrical/meta'),
    )

    for directory in DIRECTORIES:

        pxd.execute_shell_command(
            bash = f'''
                rm -rf {repr(directory.as_posix())}
            ''',
            cmd = f'''
                if exist "{directory}" rmdir /S /Q "{directory}"
            ''',
        )



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Compile and generate the binary for flashing.',
    },
    {
        'name'        : 'target',
        'description' : 'Name of target program to build; otherwise, build entire project.',
        'type'        : { target.name : target for target in TARGETS },
        'default'     : None,
    },
    {
        'name'        : 'metapreprocess_only',
        'description' : 'Run the meta-preprocessor; no compiling and linking.',
        'type'        : bool,
        'default'     : False,
    },
)
def build(parameters):



    # Determine the targets we're building for.

    if parameters.target is None:
        targets_to_build = TARGETS
    else:
        targets_to_build = [parameters.target]



    # Determine the files for the meta-preprocessor to scan through.

    metapreprocessor_file_paths = [
        pathlib.Path(root, file_name)
        for root, directories, file_names in pxd.make_main_relative_path('./electrical').walk()
        for                    file_name  in file_names
        if file_name.endswith(('.c', '.h', '.py', '.ld', '.S'))
    ]



    # Begin meta-preprocessing!

    try:
        pxd.metapreprocess(
            output_directory_path = pxd.make_main_relative_path('./electrical/meta'),
            source_file_paths     = metapreprocessor_file_paths,
        )
    except pxd.MetaPreprocessorError:
        sys.exit(1)



    # Sometimes we only just need to run the meta-preprocessor without compiling.

    if parameters.metapreprocess_only:
        return



    # We now move onto actually building the firmware.

    make_sure_shell_command_exists(
        'arm-none-eabi-gcc',
        'arm-none-eabi-cpp',
        'arm-none-eabi-objcopy',
        'arm-none-eabi-gdb',
    )



    # Creating build artifact folders.

    pxd.execute_shell_command(
        bash = [
            f'mkdir -p {pxd.make_main_relative_path('./build', target.name)}'
            for target in targets_to_build
            if target.source_file_paths
        ],
        cmd = [
            f'''
                if not exist "{pxd.make_main_relative_path('./build', target.name)}" (
                    mkdir {pxd.make_main_relative_path('./build', target.name)}
                )
            '''
            for target in targets_to_build
            if target.source_file_paths
        ],
    )



    # Preprocessing the linker files.

    pxd.execute_shell_command([
        f'''
            arm-none-eabi-cpp
                {target.compiler_flags}
                -E
                -x c
                -o "{pxd.make_main_relative_path('./build', target.name, 'link.ld').as_posix()}"
                "{pxd.make_main_relative_path('./electrical/link.ld').as_posix()}"
        '''
        for target in targets_to_build
        if target.source_file_paths
    ])



    # Compiling and linking the source code.

    pxd.execute_shell_command([
        f'''
            arm-none-eabi-gcc
                {' '.join(f'"{source.as_posix()}"' for source in target.source_file_paths)}
                -o "{pxd.make_main_relative_path('./build', target.name, f'{target.name}.elf').as_posix()}"
                -T "{pxd.make_main_relative_path('./build', target.name, 'link.ld'           ).as_posix()}"
                {target.compiler_flags}
                {target.linker_flags}
        '''
        for target in targets_to_build
        if target.source_file_paths
    ])



    # Converting the ELF file to a binary file.

    pxd.execute_shell_command([
        f'''
            arm-none-eabi-objcopy
                -S
                -O binary
                "{pxd.make_main_relative_path('./build', target.name, f'{target.name}.elf').as_posix()}"
                "{pxd.make_main_relative_path('./build', target.name, f'{target.name}.bin').as_posix()}"
        '''
        for target in targets_to_build
        if target.source_file_paths
    ])



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Flash the binary to the MCU.',
    },
    {
        'name'        : 'target',
        'description' : 'Name of the program to flash the target MCU with.',
        'type'        : { target.name : target for target in TARGETS },
        'default'     : TARGETS[0] if len(TARGETS) == 1 else ...,
    },
)
def flash(parameters):



    make_sure_shell_command_exists('STM32_Programmer_CLI')



    # Ensure binary file exists.

    binary_file_path = pxd.make_main_relative_path('./build', parameters.target.name, f'{parameters.target.name}.bin')

    if not binary_file_path.is_file():

        pxd.pxd_logger.error(
            f'Binary file {repr(binary_file_path.as_posix())} '
            f'is needed for flashing; try building first.'
        )

        sys.exit(1)



    # Begin flashing the MCU, which might take multiple tries.

    stlink   = request_stlinks(specific_one = True)
    attempts = 0

    while True:



        # Maxed out attempts?

        if attempts == 3:

            pxd.pxd_logger.error(
                f'Failed to flash; this might be because '
                f'the ST-Link is being used by another '
                f'program or that the ST-Link is disconnected.'
            )

            sys.exit(1)



        # Not the first try?

        elif attempts:

            pxd.pxd_logger.warning(
                f'Failed to flash '
                f'(maybe due to verification error); '
                f'trying again...'
            )



        # Try flashing.

        try:

            pxd.execute_shell_command(f'''
                STM32_Programmer_CLI
                    --connect port=SWD index={stlink.probe_index}
                    --download "{binary_file_path.as_posix()}" 0x08000000
                    --verify
                    --start
            ''')

            break

        except pxd.ExecuteShellCommandNonZeroExitCode:

            attempts += 1



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Set up a debugging session.',
    },
    {
        'name'        : 'target',
        'description' : 'Name of target MCU to debug.',
        'type'        : { target.name : target for target in TARGETS },
        'default'     : TARGETS[0] if len(TARGETS) == 1 else ...,
    },
    {
        'name'        : 'just_gdbserver',
        'description' : 'Just set up the GDB-server and nothing else; mainly used for Visual Studio Code.',
        'type'        : bool,
        'default'     : None,
    },
)
def debug(parameters):



    # Ensure ELF file exists.

    elf_file_path = pxd.make_main_relative_path('./build', parameters.target.name, f'{parameters.target.name}.elf')

    if not elf_file_path.is_file():

        pxd.pxd_logger.error(
            f'ELF file {repr(elf_file_path.as_posix())} '
            f'is needed for debugging; try building first.'
        )

        sys.exit(1)



    # Set up the shell command that'd initialize the GDB-server.

    make_sure_shell_command_exists(
        'ST-LINK_gdbserver',
        'STM32_Programmer_CLI',
    )

    stlink = request_stlinks(specific_one = True)

    gdbserver = f'''
        ST-LINK_gdbserver
            --stm32cubeprogrammer-path {repr(str(pathlib.Path(shutil.which('STM32_Programmer_CLI')).parent))}
            --serial-number {stlink.serial_number}
            --swd
            --apid 1
            --verify
            --attach
    '''



    # Just set up the GDB-server now if that's all that's needed of us.

    if parameters.just_gdbserver:
        pxd.execute_shell_command(gdbserver)
        return



    # Set up the shell command that'd initialize GDB itself.

    make_sure_shell_command_exists('arm-none-eabi-gdb')

    gdb_init_instructions = f'''
        file {repr(elf_file_path.as_posix())}
        target extended-remote localhost:61234
        with pagination off -- focus cmd
    '''

    gdb = f'''
        arm-none-eabi-gdb -q {' '.join(
            f'-ex "{instructions.strip()}"'
            for instructions in gdb_init_instructions.strip().splitlines()
        )}
    '''



    # Launch the debugging session.

    try:

        pxd.execute_shell_command(
            bash       = f'set -m; {gdbserver} 1> /dev/null 2> /dev/null & {gdb}',
            powershell = f'{gdbserver} & {gdb}',
        )

    except KeyboardInterrupt:

        # If the user presses CTRL-C during a GDB
        # session, Python will register this as an
        # exception once the command ends.

        pass



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Open the COM serial port of the ST-Link.',
        'more_help'   : True,
    },
    {
        'name'        : 'port',
        'description' : "The serial port to open; otherwise, automatically find the ST-Link's serial port.",
        'type'        : str,
        'default'     : None,
    },
)
def talk(parameters):



    if parameters is None:

        match sys.platform:
            case 'linux' : return '<ctrl-a ctrl-q> Exit.'
            case 'win32' : return '<ctrl-c> Exit.'
            case _       : return '<ctrl-c> Exit (maybe?).'



    if sys.platform == 'linux':
        make_sure_shell_command_exists('picocom')



    if parameters.port is None:

        stlink = request_stlinks(
            specific_one    = True,
            stlink_error_ok = True,
        )

        if stlink is None:

            serial, list_ports = import_pyserial()

            comports = list_ports.comports()

            pxd.pxd_logger.info(
                'The ST-Link might be in use (e.g. due to an attached debugger); '
                'if so, explicitly name the serial port with the flag `--port`.',
                extra = {
                    'table' : (
                        (f'> {comport.name}', comport.description)
                        for comport in comports
                    )
                }
            )

            sys.exit(1)

        device = stlink.comport.device

    else:

        device = parameters.port



    try:

        pxd.execute_shell_command(

            # Picocom's pretty good!

            bash = f'''
                picocom --baud={STLINK_BAUD} --quiet --imap=lfcrlf {device}
            ''',



            # The only reason why PowerShell is used here is
            # because there's no convenient way in Python to
            # read a single character from STDIN with no blocking
            # and buffering. Furthermore, the serial port on
            # Windows seem to be buffered up, so data before the
            # port is opened for reading is available to fetch;
            # PySerial only returns data sent after the port is opened.

            powershell = f'''
                $port = new-Object System.IO.Ports.SerialPort {device},{STLINK_BAUD},None,8,one;
                $port.Open();
                try {{
                    while ($true) {{
                        Write-Host -NoNewline $port.ReadExisting();
                        if ([System.Console]::KeyAvailable) {{
                            $port.Write([System.Console]::ReadKey($true).KeyChar);
                        }}
                        Start-Sleep -Milliseconds 1;
                    }}
                }} finally {{
                    $port.Close();
                }}
            '''

        )

    except KeyboardInterrupt:
        pass



################################################################################



@main_interface.new_verb(
    {
        'name'        : 'stlinks',
        'description' : 'Search and list for any ST-Links connected to the computer.',
    },
)
def _(parameters):

    if stlinks := request_stlinks():

        logger_stlinks(stlinks)

    else:

        pxd.pxd_logger.info('No ST-Link detected by STM32_Programmer_CLI.')



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Check the correctness of PCBs.',
    },
)
def checkPCBs(parameters):



    # We'll be relying on KiCad's CLI program
    # to generate the netlist of schematics.

    make_sure_shell_command_exists('kicad-cli')



    # Some of the targets' GPIOs configuration will be verified
    # when we run the meta-preprocessor; things like invalid GPIO
    # pin port-number settings will be caught first.

    build(types.SimpleNamespace(
        target              = None,
        metapreprocess_only = True
    ))



    # Get all of the KiCad symbol libraries.

    symbol_library_file_names = [
        file_name
        for _, _, file_names in pxd.make_main_relative_path('./pcb/symbols').walk()
        for       file_name  in file_names
        if file_name.endswith(suffix := '.kicad_sym')
    ]



    # Find any symbols with the 'CoupledConnector' keyword
    # associated with it and get the pin-outs for them.

    coupled_connectors = {}

    for symbol_library_file_name in symbol_library_file_names:

        symbol_library_sexp = pxd.parse_sexp(
            pxd.make_main_relative_path('./pcb/symbols', symbol_library_file_name).read_text()
        )

        for entry in symbol_library_sexp:

            match entry:

                case ('symbol', symbol_name, *symbol_properties):



                    # Make sure the symbol has the 'CoupledConnector' keyword.

                    for symbol_property in symbol_properties:
                        match symbol_property:
                            case ('property', 'ki_keywords', 'CoupledConnector', *_):
                                break
                    else:
                        continue



                    # The details for the symbol's pin-outs is
                    # buried within another symbol S-expression.

                    subsymbol_properties = None

                    for symbol_property in symbol_properties:
                        match symbol_property:
                            case ('symbol', subsymbol_name, *subsymbol_properties): pass



                    # Get all of the symbol's pin names and coordinates.

                    coupled_connectors[symbol_name] = {}

                    for subsymbol_property in subsymbol_properties:

                        match subsymbol_property:

                            case ('pin', pin_type, pin_style, *pin_properties):

                                pin_coordinate = None
                                pin_name       = None

                                for pin_property in pin_properties:
                                    match pin_property:
                                        case ('number', pin_coordinate, *_): pass
                                        case ('name'  , pin_name      , *_): pass

                                if pin_coordinate in coupled_connectors[symbol_name]:
                                    raise NotImplementedError

                                coupled_connectors[symbol_name][pin_coordinate] = types.SimpleNamespace(
                                    name = pin_name,
                                    nets = [],
                                )



    # Get the names of all KiCad projects.

    kicad_projects = [
        file_name.removesuffix(suffix)
        for _, _, file_names in pxd.make_main_relative_path('./pcb').walk()
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

        schematic_file_path = pxd.make_main_relative_path('./pcb'  , f'{kicad_project}.kicad_sch')
        netlist_file_path   = pxd.make_main_relative_path('./build', f'{kicad_project}.net'      )

        pxd.execute_shell_command(f'''
            kicad-cli
                sch export netlist "{schematic_file_path.as_posix()}"
                --output "{netlist_file_path.as_posix()}"
                --format orcadpcb2
        ''')



        # Parse the outputted netlist file.

        netlist_sexp = netlist_file_path.read_text()
        netlist_sexp = netlist_sexp.removesuffix('*\n') # Trailing asterisk for some reason.
        netlist_sexp = pxd.parse_sexp(netlist_sexp)

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
                    pxd.pxd_logger.warning(
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
                    pxd.pxd_logger.warning(
                        f'For target {repr(target.name)} '
                        f'and schematic {repr(schematic_file_path.as_posix())}, '
                        f'multiple {repr(target.mcu)} are found.'
                    )
                    continue



            # We look for discrepancies between the
            # netlist and the target's GPIO list.

            for mcu_pin_coordinate, mcu_pin_net_name in mcu_nets:



                # The pin coordinate is sometimes just a number,
                # but for some packages like BGA, it might be a 2D
                # coordinate (letter-number pair like 'J7').
                # The s-exp parser will parse the 1D coordinate as
                # an actual integer but the 2D coordinate as a string.
                # Thus, to keep things consistent, we always convert
                # the coordinate back into a string.

                mcu_pin_coordinate = str(mcu_pin_coordinate)



                # Skip things like power pins.

                import deps.stpy.mcus

                mcu_pin = deps.stpy.mcus.MCUS[target.mcu].pinouts[mcu_pin_coordinate]

                if mcu_pin.type != 'I/O':
                    continue



                # Get the port-number name (e.g. `A3`).

                mcu_pin_port_number = mcu_pin.name.removeprefix('P')



                # Find the corresponding GPIO for the target, if any.

                match [
                    gpio_name
                    for gpio_name, gpio_pin, gpio_type, gpio_settings in target.gpios
                    if gpio_pin == mcu_pin_port_number
                ]:



                    # No GPIO found, so this MCU pin should be unconnected.

                    case []:
                        if not mcu_pin_net_name.startswith('unconnected-('):
                            pxd.pxd_logger.warning(
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
                            pxd.pxd_logger.warning(
                                f'For target {repr(target.name)} '
                                f'and schematic {repr(schematic_file_path.as_posix())}, '
                                f'pin {repr(mcu_pin_port_number)} should have '
                                f'net {repr(gpio_name)} in the schematic, '
                                f'but instead is unconnected.'
                            )

                        elif gpio_name != mcu_pin_net_name.removeprefix('/'):
                            pxd.pxd_logger.warning(
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

                    coupled_connectors[key][str(coordinate)].nets += [
                        (schematic_file_path, net_name)
                    ]



    # Check for any discrepancies between coupled connectors.

    for coupled_connector_name, coupled_pins in coupled_connectors.items():

        for coupled_pin_coordinate, coupled_pin in coupled_pins.items():



            # This pin only has one net tied to it and
            # it does not match with the coupled pin's name.

            if len(net_names := [
                net_name.removeprefix('/')
                for schematic_file_path, net_name in coupled_pin.nets
                if not net_name.startswith('unconnected-')
            ]) == 1 and coupled_pin.name != net_names[0]:

                pxd.pxd_logger.warning(
                    f'Coupled connector {repr(coupled_connector_name)} '
                    f'on pin {repr(coupled_pin_coordinate)} only has '
                    f'one net associated with it.',
                    extra = {
                        'table' : [
                            (repr(schematic_file_path.name), repr(net_name))
                            for schematic_file_path, net_name in coupled_pin.nets
                        ]
                    }
                )



            # This pin has conflicting nets tied to it.

            if len(set(
                net_name.removeprefix('/')
                for schematic_file_path, net_name in coupled_pin.nets
                if not net_name.startswith('unconnected-')
            )) >= 2:

                pxd.pxd_logger.warning(
                    f'Coupled connector {repr(coupled_connector_name)} '
                    f'on pin {repr(coupled_pin_coordinate)} has '
                    f'conflicting nets associated with it.',
                    extra = {
                        'table' : [
                            (repr(schematic_file_path.name), repr(net_name))
                            for schematic_file_path, net_name in coupled_pin.nets
                        ]
                    }
                )



################################################################################



@main_interface.new_verb(
    {
        'description' : 'View image data from the OVCAM driver.',
        'more_help'   : True
    },
)
def tv(parameters):



    SURFACE_DEFAULT_RGB = (74, 65, 42)



    # Define some useful keybindings.

    keybindings = []

    def Keybinding(keystroke, description):

        def decorator(function):

            nonlocal keybindings

            keybindings += [types.SimpleNamespace(
                keystroke   = keystroke,
                description = description,
                function    = function,
            )]

            return function

        return decorator



    @Keybinding('ctrl-w', 'Turn the TV off.')
    def _():

        nonlocal quit

        quit = True



    orientation = 0

    @Keybinding('f', 'Flip through different orientations.')
    def _():

        nonlocal orientation

        orientation = (orientation + 1) % 4

        pxd.pxd_logger.info(f'Orientation: {orientation}.')



    stream_image_progress = False

    @Keybinding('p', 'Toggle whether or not image data is rendered as it streams in.')
    def _():

        nonlocal stream_image_progress

        stream_image_progress = not stream_image_progress

        pxd.pxd_logger.info(f'Partial frame rendering: {'enabled' if stream_image_progress else 'disabled'}.')



    @Keybinding('g', 'Toggle visibility of debug GUI.')
    def _():

        for widget in widgets:

            if widget.element.visible:
                widget.element.hide()
            else:
                widget.element.show()



    # List the keybindings in the help message.

    if parameters is None:

        more_help = ''

        for just_keystroke, just_description in pxd.justify([
            (
                ('<' , f'<{keybinding.keystroke}>'),
                (None, keybinding.description     ),
            )
            for keybinding in keybindings
        ]):

            more_help += f'{just_keystroke} {just_description}\n'

        return more_help



    # Some state variables.

    pygame, pygame_gui = import_pygame()
    serial, list_ports = import_pyserial()

    pygame.init()

    screen  = pygame.display.set_mode((1080, 720), pygame.RESIZABLE)
    manager = pygame_gui.UIManager(screen.get_size())
    clock   = pygame.time.Clock()
    quit    = False

    surface = pygame.Surface((1, 1))
    surface.fill(SURFACE_DEFAULT_RGB)

    widgets = []

    def calculate_widget_y():
        return 1.25 * sum(widget.element.relative_rect[3] for widget in widgets)



    # Options for configuring JPEG CTRL3 register.

    def jpeg_ctrl3_callback(_):

        jpeg_ctrl3_value = 0

        for bit_index, field in enumerate(OVCAM_JPEG_CTRL3_FIELDS):

            if field.configurable:

                widget, = (
                    widget
                    for widget in widgets
                    if widget.callback == jpeg_ctrl3_callback
                    if widget.field    == field
                )
                bit = widget.element.get_state()

            else:

                bit = field.default

            jpeg_ctrl3_value |= bit << bit_index

        serial_port.write(bytes([TV_WRITE_BYTE, 0x44, 0x03, jpeg_ctrl3_value]))

    for bit_index, field in enumerate(OVCAM_JPEG_CTRL3_FIELDS):

        if not field.configurable:
            continue

        widgets += [types.SimpleNamespace(
            field    = field,
            callback = jpeg_ctrl3_callback,
            element  = pygame_gui.elements.UICheckBox(
                relative_rect = pygame.Rect((0, calculate_widget_y()), (16, 16)),
                text          = field.description,
                initial_state = field.default,
                visible       = False,
                manager       = manager,
            ),
        )]



    # Options for changing the image resolution.

    resolution_options_list = {
        f'{width}x{height}' : (width, height)
        for width, height in OVCAM_RESOLUTIONS
    }

    def resolution_callback(widget):

        width, height = resolution_options_list[widget.element.selected_option[0]]

        serial_port.write(bytes([TV_WRITE_BYTE, 0x38, 0x08, (width  >> 8) & 0xFF]))
        serial_port.write(bytes([TV_WRITE_BYTE, 0x38, 0x09, (width  >> 0) & 0xFF]))
        serial_port.write(bytes([TV_WRITE_BYTE, 0x38, 0x0A, (height >> 8) & 0xFF]))
        serial_port.write(bytes([TV_WRITE_BYTE, 0x38, 0x0B, (height >> 0) & 0xFF]))

    widgets += [types.SimpleNamespace(
        callback = resolution_callback,
        element  = pygame_gui.elements.UIDropDownMenu(
            relative_rect   = pygame.Rect((0, calculate_widget_y()), (128, 32)),
            options_list    = resolution_options_list,
            starting_option = [
                text
                for text, resolution in resolution_options_list.items()
                if resolution == OVCAM_DEFAULT_RESOLUTION
            ][0],
            visible = False,
            manager = manager,
        ),
    )]



    # Options for doing PRE-ISP tests.

    def pre_isp_test_callback(widget):

        pre_isp_test_setting_value = 0
        bit_index                  = 0

        for field in PRE_ISP_TEST_SETTING_FIELDS:

            widget, = (
                widget
                for widget in widgets
                if widget.callback == pre_isp_test_callback
                if widget.field    == field
            )

            if field.options == bool:
                field_value = widget.element.get_state()
                field_width = 1
            else:
                field_value = field.options.index(widget.element.selected_option[0])
                field_width = len(f'{len(field.options) :b}') - 1

            pre_isp_test_setting_value |= field_value << bit_index
            bit_index                  += field_width

        serial_port.write(bytes([TV_WRITE_BYTE, 0x50, 0x3D, pre_isp_test_setting_value]))

    for field in reversed(PRE_ISP_TEST_SETTING_FIELDS):

        if field.options == bool:

            widgets += [types.SimpleNamespace(
                field    = field,
                callback = pre_isp_test_callback,
                element  = pygame_gui.elements.UICheckBox(
                    relative_rect = pygame.Rect((0, calculate_widget_y()), (16, 16)),
                    text          = field.description,
                    initial_state = False,
                    visible       = False,
                    manager       = manager,
                ),
            )]

        else:

            widgets += [types.SimpleNamespace(
                field    = field,
                callback = pre_isp_test_callback,
                element  = pygame_gui.elements.UIDropDownMenu(
                    relative_rect = pygame.Rect(
                        (0, calculate_widget_y()),
                        (max(len(option) for option in field.options) * 12, 32)
                    ),
                    options_list    = field.options,
                    starting_option = field.options[0],
                    visible         = False,
                    manager         = manager,
                ),
            )]



    # Image data sent over the ST-Link serial port
    # will be delimited with starting and ending tokens.

    stlink      = request_stlinks(specific_one = True)
    serial_port = serial.Serial(
        stlink.comport.device,
        baudrate = STLINK_BAUD,
        timeout  = 0
    )

    token_expected = TV_TOKEN.START
    stream_data    = b''



    # Routine to handle the chunks of image
    # data as they come through the serial port.

    image_data      = None
    past_sizes      = []
    past_timestamps = []

    def stream_callback(event, new_data):

        nonlocal surface, image_data, past_sizes, past_timestamps

        match event:



            # Initializing for the new image.

            case 'starting':

                image_data = b''



            # Adding the new information to the image.

            case 'continuing':

                image_data += new_data



                # Seems like PyGame's image loader is sometimes okay
                # when the JPEG frame is incomplete; we might
                # as well render it to show progress so far.

                if stream_image_progress:

                    try:
                        surface = pygame.image.load(io.BytesIO(image_data))
                    except pygame.error as error:
                        pass



                # Ensure we aren't collecting too much data.

                got       = len(image_data)
                excessive = 128 * 1024

                if got > excessive:

                    pxd.pxd_logger.error(f'Got {got} bytes; expected at most {excessive} bytes.')

                    return True



            # Adding the last bit of data to the image and rendering it.

            case 'ending':

                image_data += new_data



                # Record the size of the JPEG frame
                # and average it with the past frames.

                past_sizes   += [len(image_data)]
                past_sizes    = past_sizes[-10:]
                average_size  = sum(past_sizes) / len(past_sizes)



                # Record the delta time between each frame
                # to estimate the frame-rate.

                past_timestamps += [time.time()]
                past_timestamps  = past_timestamps[-10:]

                if len(past_timestamps) >= 2:

                    average_fps = [
                        1 / (past_timestamps[i + 1] - past_timestamps[i])
                        for i in range(len(past_timestamps) - 1)
                    ]

                    average_fps = sum(average_fps) / len(average_fps)

                else:

                    average_fps = 0



                # Display the statistics.

                pygame.display.set_caption(
                    ' | '.join((
                        f'curr. {len(image_data) :_} bytes',
                        f'avg. {round(average_size) :_} bytes',
                        f'avg. {average_fps :.4f} FPS',
                    ))
                )



                # Try parsing the JPEG frame.

                try:

                    surface = pygame.image.load(io.BytesIO(image_data))

                except pygame.error as error:

                    pxd.pxd_logger.error(f'PyGame failed to parse received JPEG image data: {repr(str(error))}.')

                    return True



            case idk: raise RuntimeError(idk)



    # Window loop.

    while not quit:



        # Handle inputs.

        dt = clock.tick(60) / 1000

        for event in pygame.event.get():

            manager.process_events(event)

            match event.type:



                # A quit signal from ALT+F4 for instance.

                case pygame.QUIT:
                    quit = True



                # Handle keybindings.

                case pygame.KEYDOWN:



                    # Determine keystroke.

                    keystroke = pygame.key.name(event.key)

                    if keystroke == 'return':
                        keystroke = 'enter'

                    if event.mod & pygame.KMOD_SHIFT:
                        keystroke = f'shift-{keystroke}'

                    if event.mod & pygame.KMOD_CTRL:
                        keystroke = f'ctrl-{keystroke}'



                    # Execute the appropriate keybinding if one exists.

                    for keybinding in keybindings:
                        if keybinding.keystroke == keystroke:
                                keybinding.function()



                # Handle UI events.

                case (
                    pygame_gui.UI_BUTTON_PRESSED         |
                    pygame_gui.UI_CHECK_BOX_CHECKED      |
                    pygame_gui.UI_CHECK_BOX_UNCHECKED    |
                    pygame_gui.UI_DROP_DOWN_MENU_CHANGED
                ):



                    # Skip buttons belonging to drop-down menus.

                    if (hasattr(event.ui_element, 'parent_element') and (
                        isinstance(event.ui_element.parent_element, pygame_gui.elements.ui_drop_down_menu.UIDropDownMenu ) or
                        isinstance(event.ui_element.parent_element, pygame_gui.elements.ui_selection_list.UISelectionList)
                    )):
                        continue



                    # Find the corresponding widget and run its callback.

                    widget, = (
                        widget
                        for widget in widgets
                        if widget.element == event.ui_element
                    )

                    widget.callback(widget)



        # Grab the new serial data, if any.

        if serial_port.in_waiting:

            stream_data += serial_port.read(serial_port.in_waiting)

            while True:



                # Determine the first token in the stream, if any.

                try:
                    start_token_index = stream_data.index(TV_TOKEN.START)
                except ValueError:
                    start_token_index = None

                try:
                    end_token_index = stream_data.index(TV_TOKEN.END)
                except ValueError:
                    end_token_index = None

                next_token_found = None
                next_token_index = None

                match (start_token_index, end_token_index):

                    case (None, None):
                        next_token_found = None
                        next_token_index = None

                    case (_, None):
                        next_token_found = TV_TOKEN.START
                        next_token_index = start_token_index

                    case (None, _):
                        next_token_found = TV_TOKEN.END
                        next_token_index = end_token_index

                    case (a, b) if a < b:
                        next_token_found = TV_TOKEN.START
                        next_token_index = start_token_index

                    case (a, b) if a > b:
                        next_token_found = TV_TOKEN.END
                        next_token_index = end_token_index

                    case idk: raise RuntimeError(idk)



                # A token was found, so we split the stream into
                # what came before the token and what's after.

                if next_token_found:

                    data_before_token = stream_data[: next_token_index]
                    stream_data       = stream_data[next_token_index + len(next_token_found) :]



                # No token was found, so most the stream will be
                # before whatever the next token will be; we have
                # to be aware of the edge-case that the next token
                # might've been truncated at the end of the stream.

                else:

                    split_index = max(
                        len(stream_data) - max(len(TV_TOKEN.START), len(TV_TOKEN.END)) + 1,
                        0
                    )

                    data_before_token = stream_data[            : split_index]
                    stream_data       = stream_data[split_index :            ]



                # Handle the new stream data.

                stream_error = None

                match (token_expected, next_token_found):



                    # No start token yet,
                    # so the data is meaningless to us.

                    case (TV_TOKEN.START, None):

                        pass



                    # Start token found! The following
                    # stream data will be image data.

                    case (TV_TOKEN.START, TV_TOKEN.START):

                        stream_error   = stream_callback('starting', b'')
                        token_expected = TV_TOKEN.END



                    # Found an end token for some reason;
                    # this is probably okay, because the
                    # TV might've been started during the
                    # streaming of image data.

                    case (TV_TOKEN.START, TV_TOKEN.END):

                        pass



                    # We have some new image data that was
                    # just streamed in!

                    case (TV_TOKEN.END, None):

                        stream_error = stream_callback('continuing', data_before_token)



                    # Uh oh, image data stream is starting
                    # again for some reason...

                    case (TV_TOKEN.END, TV_TOKEN.START):

                        pxd.pxd_logger.error('Two consecutive start tokens found; restarting image stream.')

                        stream_error = True

                        stream_callback('starting', b'')



                    # We've received the last of the image data.

                    case (TV_TOKEN.END, TV_TOKEN.END):

                        stream_error   = stream_callback('ending', data_before_token)
                        token_expected = TV_TOKEN.START



                    case idk: raise RuntimeError(idk)



                # If there was something wrong with the data,
                # we'll pretty much discard everything and start over.

                if stream_error:

                    surface.fill(SURFACE_DEFAULT_RGB)

                    token_expected  = TV_TOKEN.START
                    past_sizes      = []
                    past_timestamps = []



                # We've processed the stream
                # as much as we have possibly can.

                if len(stream_data) < max(len(TV_TOKEN.START), len(TV_TOKEN.END)):
                    break



        # Render.

        screen.blit(
            pygame.transform.scale(
                pygame.transform.flip(
                    surface,
                    orientation & 0b01,
                    orientation & 0b10,
                ),
                (screen.get_width(), screen.get_height())
            ),
            (0, 0)
        )

        manager.update(dt)
        manager.draw_ui(screen)

        pygame.display.update()



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Parse UART data from the main ESP32.',
    },
)
def parseESP32(parameters):



    # TODO Move elsewhere.

    PACKET_START_TOKEN = (
        types.SimpleNamespace(
            name    = 'ESP32',
            value   = bytes([0xBE, 0xBA]),
            members = (
                ('magnetometer_x'            , 'f'),
                ('magnetometer_y'            , 'f'),
                ('magnetometer_z'            , 'f'),
                ('image_chunk'               , 'B' * 190),
                ('quaternion_i'              , 'f'),
                ('quaternion_j'              , 'f'),
                ('quaternion_k'              , 'f'),
                ('quaternion_r'              , 'f'),
                ('accelerometer_x'           , 'f'),
                ('accelerometer_y'           , 'f'),
                ('accelerometer_z'           , 'f'),
                ('gyro_x'                    , 'f'),
                ('gyro_y'                    , 'f'),
                ('gyro_z'                    , 'f'),
                ('computer_vision_confidence', 'f'),
                ('timestamp_ms'              , 'H'),
                ('sequence_number'           , 'B'),
                ('crc'                       , 'B'),
            ),
        ),
        types.SimpleNamespace(
            name    = 'LORA',
            value   = bytes([0xFE, 0xCA]),
            members = (
                ('quaternion_i'              , 'f'),
                ('quaternion_j'              , 'f'),
                ('quaternion_k'              , 'f'),
                ('quaternion_r'              , 'f'),
                ('accelerometer_x'           , 'f'),
                ('accelerometer_y'           , 'f'),
                ('accelerometer_z'           , 'f'),
                ('gyro_x'                    , 'f'),
                ('gyro_y'                    , 'f'),
                ('gyro_z'                    , 'f'),
                ('computer_vision_confidence', 'f'),
                ('timestamp_ms'              , 'H'),
                ('sequence_number'           , 'B'),
                ('crc'                       , 'B'),
            ),
        ),
    )



    # Open serial port of ST-Link.

    serial, list_ports = import_pyserial()
    stlink             = request_stlinks(specific_one = True)
    serial_port        = serial.Serial(
        stlink.comport.device,
        baudrate = STLINK_BAUD,
        timeout  = 0
    )



    # Continously parse UART data.

    stream_data = b''

    while True:



        # Spinlock without burning up the CPU.

        if not serial_port.in_waiting:

            time.sleep(0.010)

            continue



        # Find the first start token in the stream, if any.

        stream_data       += serial_port.read(serial_port.in_waiting)
        first_token_index  = None
        first_token        = None

        for start_token in PACKET_START_TOKEN:

            try:
                start_token_index = stream_data.index(start_token.value)
            except ValueError:
                continue

            if first_token_index is None or first_token_index > start_token_index:
                first_token_index = start_token_index
                first_token       = start_token



        # No start token found, so we
        # discard most of the received data.

        if first_token is None:

            split_index = max(
                len(stream_data) - max(len(token.value) for token in PACKET_START_TOKEN) + 1,
                0
            )

            stream_data = stream_data[split_index:]

            continue



        # Ignore everything before the starting token.

        stream_data = stream_data[first_token_index:]



        # Ensure that we have collected enough
        # of the bytes to parse the payload.

        first_token_format = f'<{''.join(
            member_format
            for member_name, member_format in first_token.members
        )}'

        payload_size = struct.calcsize(first_token_format)

        if len(stream_data) < len(first_token.value) + payload_size:

            continue



        # Parse the payload.

        stream_data  = stream_data[len(first_token.value) :             ]
        payload_data = stream_data[                       : payload_size]
        stream_data  = stream_data[payload_size           :             ]

        payload_struct = {}
        member_offset  = 0

        for member_name, member_format in first_token.members:

            member_size                  = struct.calcsize(member_format)
            payload_struct[member_name]  = struct.unpack(member_format, payload_data[member_offset:][:member_size])
            member_offset               += member_size

            if len(payload_struct[member_name]) == 1:
                payload_struct[member_name], = payload_struct[member_name]



        # Check CRC.

        crc = 0xFF

        for payload_data_i, payload_data in enumerate(payload_data):

            crc ^= payload_data

            for i in range(8):

                crc = (((crc << 1) ^ 0x2F) if (crc & (1 << 7)) else (crc << 1)) & 0xFF



        # Output the payload.

        if crc:
            pxd.pxd_logger.warning(f'{(first_token.name, payload_struct)}')
        else:
            pxd.pxd_logger.info(f'{(first_token.name, payload_struct)}')



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Find citations used throughout the project.',
    },
    {
        'name'        : 'find',
        'description' : 'Name of the reference to search for.',
        'type'        : str,
        'default'     : None,
    },
    {
        'name'        : 'replace',
        'description' : 'After finding citations of a specific reference, replace it with a new one.',
        'type'        : str,
        'default'     : None,
    },
)
def cite(parameters):

    pxd.process_citations(
        file_paths = [
            pathlib.Path(root, file_name)
            for root, directories, file_names in pxd.make_main_relative_path('./').walk()
            if not any(
                root.is_relative_to(excluded_directory_path)
                for excluded_directory_path in (
                    pxd.make_main_relative_path('./build'),
                    pxd.make_main_relative_path('./deps'),
                    pxd.make_main_relative_path('./pcb/models'),
                    pxd.make_main_relative_path('./mechanical'),
                    pxd.make_main_relative_path('./electrical/meta'),
                    pxd.make_main_relative_path('./.git'),
                )
            )
            for file_name in file_names
        ],
        reference_text_to_find     = parameters.find,
        replacement_reference_text = parameters.replace,
    )



################################################################################



main_interface.invoke()
