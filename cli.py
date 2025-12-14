#! /usr/bin/env python3



#
# Enforce Python version.
#



import sys

if not (
    sys.version_info.major == 3 and
    sys.version_info.minor >= 13
):
    raise RuntimeError(
        'Unsupported Python version: ' + repr(sys.version) + '; ' +
        'please upgrade to at least ' + str(MINIMUM_MAJOR) + '.' + str(MINIMIM_MINOR) + '; '
        'note that it is possible that you have multiple instances of Python installed; '
        'in this case, please set your PATH accordingly or use a Python virtual environment.'
    )



#
# Built-in modules.
#



import types, shlex, pathlib, shutil, subprocess, time, logging



#
# Logger configuration.
#



class MainFormatter(logging.Formatter):

    def format(self, record):



        message = super().format(record)



        # The `table` property allows for a
        # simple, justified table to be outputted.

        if hasattr(record, 'table'):

            for just_key, just_value in justify([
                (
                    ('<' , str(key  )),
                    (None, str(value)),
                )
                for key, value in record.table
            ]):
                message += f'\n{just_key} : {just_value}'



        # The main interface logger won't have its
        # info logs be prepended with the level.

        if not (
            record.name.split('.')[:2] == [__name__, 'main_interface']
            and record.levelname == 'INFO'
        ):



            # Any newlines will be indented so it'll look nice.

            indent = ' ' * len(f'[{record.levelname}] ')

            message = '\n'.join([
                message.splitlines()[0],
                *[f'{indent}{line}' for line in message.splitlines()[1:]]
            ])



            # Prepend the log level name and color based on severity.

            coloring = {
                'DEBUG'    : '\x1B[0;35m',
                'INFO'     : '\x1B[0;36m',
                'WARNING'  : '\x1B[0;33m',
                'ERROR'    : '\x1B[0;31m',
                'CRITICAL' : '\x1B[1;31m',
            }[record.levelname]

            reset = '\x1B[0m'

            message = f'{coloring}[{record.levelname}]{reset} {message}'



        # Give each log a bit of breathing room.

        message += '\n'



        return message



logger         = logging.getLogger(__name__)
logger_handler = logging.StreamHandler(sys.stdout)
logger_handler.setFormatter(MainFormatter())
logger.addHandler(logger_handler)
logger.setLevel(logging.DEBUG)



#
# The PXD module.
#



try:

    import deps.stpy.pxd.metapreprocessor
    import deps.stpy.pxd.interface
    from   deps.stpy.pxd.utils import make_main_relative_path, justify

except ModuleNotFoundError as error:

    if 'pxd' not in error.name:
        raise # Likely a bug in the PXD module.

    import traceback

    print()
    traceback.print_exc()
    print()

    logger.error(
        f'Could not import "{error.name}"; maybe the Git '
        f'submodules need to be initialized/updated? Try doing:' '\n'
        f'> git submodule update --init --recursive'
        f'If this still doesn\'t work, please raise an issue.'
    )

    sys.exit(1)



#
# Import declarations that's
# shared with the meta-preprocessor.
#



from electrical.Shared import *



#
# Import handlers for non-builtin modules.
#



def import_pyserial():

    try:

        import serial
        import serial.tools.list_ports

    except ModuleNotFoundError as error:

        logger.error(
            f'Python got {type(error).__name__} ({error}); try doing:' '\n'
            f'> pip install pyserial'
        )

        sys.exit(1)

    return serial, serial.tools.list_ports



def import_pygame():

    try:

        import pygame

    except ModuleNotFoundError as error:

        logger.error(
            f'Python got {type(error).__name__} ({error}); try doing:' '\n'
            f'> pip install pygame'
        )

        sys.exit(1)

    return pygame



#
# Routine for ensuring the user has the required programs
# on their machine (and provide good error messages if not).
#



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

        logger.error(
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

        logger.error(
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

        logger.error(
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

        logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?' '\n'
            f'If you\'re on a Windows system, run the following command and restart your shell:'  '\n'
            f'> winget install ezwinports.make'                                                   '\n'
        )



    # KiCad-CLI.

    elif missing_program in (roster := [
        'kicad-cli',
    ]):

        logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH.'                           '\n'
            f'This program comes along with KiCad, so on Windows,'                               '\n'
            f'you might find it at "C:\\Program Files\\KiCad\\9.0\\bin\\{missing_program}.exe".' '\n'
            f'Thus, add something like "C:\\Program Files\\KiCad\\9.0\\bin" to your PATH.'       '\n'
        )



    # Explanation not implemented.

    else:

        logger.error(
            f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?'
        )



    sys.exit(1)



#
# Routine for logging out ST-Links as a table.
#



def logger_stlinks(stlinks):

    logger.info(
        '\n'.join(
            f'| {' | '.join(justs)} |'
            for justs in justify(
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



#
# Routine for finding ST-Links.
#



def request_stlinks(
    *,
    specific_one         = False,
    specific_probe_index = None,
):



    # Use STM32_Programmer_CLI to find any connected ST-Links.

    make_sure_shell_command_exists('STM32_Programmer_CLI')

    listing_lines = subprocess.check_output(f'''
        STM32_Programmer_CLI --list st-link
    '''.split()).decode('utf-8').splitlines()



    # Sometimes ST-Links can lock-up.

    if any('ST-LINK error' in line for line in listing_lines):

        logger.error(
            f'There seems to be an error with an ST-Link; '
            f'see the output of STM32_Programmer_CLI below:' '\n'
            f'\n'
            f'{'\n'.join(listing_lines)}'
        )

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
            logger.error(f'No ST-Links found.')
            sys.exit(1)



        # There's no matching ST-Links!

        if not (matches := [stlink for stlink in stlinks if stlink.probe_index == specific_probe_index]):
            logger_stlinks(stlinks)
            logger.error(f'No ST-Links found with probe index of "{specific_probe_index}".')
            sys.exit(1)



        # Give back the desired ST-Link.

        stlink, = matches

        return stlink



    # If the caller is assuming there's only one
    # ST-Link, then give back the only one.

    if specific_one:



        # There's no ST-Links!

        if not stlinks:
            logger.error(f'No ST-Links found.')
            sys.exit(1)



        # There's too many ST-Links!

        if len(stlinks) >= 2:
            logger_stlinks(stlinks)
            logger.error(f'Multiple ST-Links found; I don\'t know which one to use.')
            sys.exit(1)



        # Give back the only ST-Link.

        stlink, = stlinks

        return stlink



    # Otherwise, give back the list of
    # the ST-Links we found (if any).

    return stlinks



#
# Routine to carry out shell commands.
#



class ExecuteShellCommandNonZeroExitCode(Exception):
    pass



def execute_shell_command(
    default    = None,
    *,
    bash       = None,
    cmd        = None,
    powershell = None,
):



    # PowerShell is slow to invoke, so cmd.exe
    # would be used if its good enough.

    if cmd is not None and powershell is not None:
        raise ValueError('CMD and PowerShell commands cannot be both provided.')

    match sys.platform:

        case 'win32':
            use_powershell = cmd is None and powershell is not None
            commands       = powershell if use_powershell else cmd

        case _:
            commands       = bash
            use_powershell = False

    if commands is None:
        commands = default

    if commands is None:
        raise ValueError(f'Missing shell command for platform {repr(sys.platform)}.')

    if isinstance(commands, str):
        commands = [commands]



    # Process each command to have it be split into shell tokens.
    # The lexing that's done here is to do a lot of the funny
    # business involving escaping quotes and what not. To be honest,
    # it's a little out my depth, mainly because I frankly do not
    # care enough to get it 100% correct; it working most of the time
    # is good enough for me.

    for command_i in range(len(commands)):

        lexer                  = shlex.shlex(commands[command_i])
        lexer.quotes           = '"'
        lexer.whitespace_split = True
        lexer.commenters       = ''
        commands[command_i]    = list(lexer)



    # Execute each shell command.

    processes = []

    for command_i, command in enumerate(commands):

        command = ' '.join(command)

        logger.info(f'$ {command}')

        if use_powershell:

            # On Windows, Python will call CMD.exe
            # to run the shell command, so we'll
            # have to invoke PowerShell to run the
            # command if PowerShell is needed.

            processes += [subprocess.Popen(['pwsh', '-Command', command], shell = False)]

        else:

            processes += [subprocess.Popen(command, shell = True)]



    # Wait on each subprocess to be done.

    for process in processes:
        if process.wait():
            raise ExecuteShellCommandNonZeroExitCode



#
# Set up the command-line-interface of the Python script.
#



def main_interface_hook(verb, parameters):

    start   = time.time()
    yield
    end     = time.time()
    elapsed = end - start

    if elapsed >= 0.5:
        logger.debug(f'"{verb.name}" took {elapsed :.3f}s.')



main_interface = deps.stpy.pxd.interface.Interface(
    name        = f'{make_main_relative_path(pathlib.Path(__file__).name)}',
    description = f'The command line program (pronounced "clippy").',
    logger      = logging.getLogger(f'{logger.name}.main_interface'),
    hook        = main_interface_hook,
)



################################################################################



@main_interface.new_verb(
    {
        'description' : 'Delete all build artifacts.',
    },
)
def clean(parameters):

    DIRECTORIES = (
        make_main_relative_path('./build'),
        make_main_relative_path('./electrical/meta'),
    )

    for directory in DIRECTORIES:

        execute_shell_command(
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
        for root, directories, file_names in make_main_relative_path('./electrical').walk()
        for                    file_name  in file_names
        if file_name.endswith(('.c', '.h', '.py', '.ld', '.S'))
    ]



    # Callback of things to do before and after the execution of a meta-directive.

    elapsed               = 0
    meta_directive_deltas = []

    def metadirective_callback(index, meta_directives):

        nonlocal elapsed, meta_directive_deltas

        # Log the evaluation of the meta-directive.

        location = f'{meta_directives[index].source_file_path.as_posix()}:{meta_directives[index].meta_header_line_number}'

        logger.info(f'Meta-preprocessing {location}')



        # Record how long it takes to run this meta-directive.

        start                  = time.time()
        output                 = yield
        end                    = time.time()
        delta                  = end - start
        meta_directive_deltas += [(location, delta)]
        elapsed               += delta



    # Begin meta-preprocessing!

    try:
        deps.stpy.pxd.metapreprocessor.do(
            output_directory_path = make_main_relative_path('./electrical/meta'),
            source_file_paths     = metapreprocessor_file_paths,
            callback              = metadirective_callback,
        )
    except deps.stpy.pxd.metapreprocessor.MetaError as error:
        error.dump()
        sys.exit(1)



    # Log the performance of the meta-preprocessor.

    logger.debug(
        f'Meta-preprocessing {len(meta_directive_deltas)} meta-directives took {elapsed :.3f}s.',
        extra = {
            'table' : [
                (location, f'{delta :.3f}s | {delta / elapsed * 100 : 5.1f}%')
                for location, delta in sorted(meta_directive_deltas, key = lambda x: -x[1])
            ]
        }
    )



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

    execute_shell_command(
        bash = [
            f'mkdir -p {make_main_relative_path('./build', target.name)}'
            for target in targets_to_build
            if target.source_file_paths
        ],
        cmd = [
            f'''
                if not exist "{make_main_relative_path('./build', target.name)}" (
                    mkdir {make_main_relative_path('./build', target.name)}
                )
            '''
            for target in targets_to_build
            if target.source_file_paths
        ],
    )



    # Preprocessing the linker files.

    execute_shell_command([
        f'''
            arm-none-eabi-cpp
                {target.compiler_flags}
                -E
                -x c
                -o "{make_main_relative_path('./build', target.name, 'link.ld').as_posix()}"
                "{make_main_relative_path('./electrical/link.ld').as_posix()}"
        '''
        for target in targets_to_build
        if target.source_file_paths
    ])



    # Compiling and linking the source code.

    execute_shell_command([
        f'''
            arm-none-eabi-gcc
                {' '.join(f'"{source}"' for source in target.source_file_paths)}
                -o "{make_main_relative_path('./build', target.name, f'{target.name}.elf').as_posix()}"
                -T "{make_main_relative_path('./build', target.name, 'link.ld'           ).as_posix()}"
                {target.compiler_flags}
                {target.linker_flags}
        '''
        for target in targets_to_build
        if target.source_file_paths
    ])



    # Converting the ELF file to a binary file.

    execute_shell_command([
        f'''
            arm-none-eabi-objcopy
                -S
                -O binary
                "{make_main_relative_path('./build', target.name, f'{target.name}.elf').as_posix()}"
                "{make_main_relative_path('./build', target.name, f'{target.name}.bin').as_posix()}"
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

    binary_file_path = make_main_relative_path('./build', parameters.target.name, f'{parameters.target.name}.bin')

    if not binary_file_path.is_file():

        logger.error(
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

            logger.error(
                f'Failed to flash; this might be because '
                f'the ST-Link is being used by another '
                f'program or that the ST-Link is disconnected.'
            )

            sys.exit(1)



        # Not the first try?

        elif attempts:

            logger.warning(
                f'Failed to flash '
                f'(maybe due to verification error); '
                f'trying again...'
            )



        # Try flashing.

        try:

            execute_shell_command(f'''
                STM32_Programmer_CLI
                    --connect port=SWD index={stlink.probe_index}
                    --download "{binary_file_path.as_posix()}" 0x08000000
                    --verify
                    --start
            ''')

            break

        except ExecuteShellCommandNonZeroExitCode:

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

    elf_file_path = make_main_relative_path('./build', parameters.target.name, f'{parameters.target.name}.elf')

    if not elf_file_path.is_file():

        logger.error(
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
        execute_shell_command(gdbserver)
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

        execute_shell_command(
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
)
def talk(parameters):



    if parameters is None:

        match sys.platform:
            case 'linux' : return '<ctrl-a ctrl-q> Exit.'
            case 'win32' : return '<ctrl-c> Exit.'
            case _       : return '<ctrl-c> Exit (maybe?).'



    if sys.platform == 'linux':
        make_sure_shell_command_exists('picocom')

    stlink = request_stlinks(specific_one = True)

    try:

        execute_shell_command(

            # Picocom's pretty good!

            bash = f'''
                picocom --baud={STLINK_BAUD} --quiet --imap=lfcrlf {stlink.comport.device}
            ''',



            # The only reason why PowerShell is used here is
            # because there's no convenient way in Python to
            # read a single character from STDIN with no blocking
            # and buffering. Furthermore, the serial port on
            # Windows seem to be buffered up, so data before the
            # port is opened for reading is available to fetch;
            # PySerial only returns data sent after the port is opened.

            powershell = f'''
                $port = new-Object System.IO.Ports.SerialPort {stlink.comport.device},{STLINK_BAUD},None,8,one;
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

        logger.info('No ST-Link detected by STM32_Programmer_CLI.')



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
        for _, _, file_names in make_main_relative_path('./pcb/symbols').walk()
        for       file_name  in file_names
        if file_name.endswith(suffix := '.kicad_sym')
    ]



    # Find any symbols with the 'CoupledConnector' keyword
    # associated with it and get the pin-outs for them.

    import deps.stpy.pxd.sexp

    coupled_connectors = {}

    for symbol_library_file_name in symbol_library_file_names:

        symbol_library_sexp = deps.stpy.pxd.sexp.parse_sexp(
            make_main_relative_path('./pcb/symbols', symbol_library_file_name).read_text()
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
        for _, _, file_names in make_main_relative_path('./pcb').walk()
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

        schematic_file_path = make_main_relative_path('./pcb'  , f'{kicad_project}.kicad_sch')
        netlist_file_path   = make_main_relative_path('./build', f'{kicad_project}.net'      )

        execute_shell_command(f'''
            kicad-cli
                sch export netlist "{schematic_file_path.as_posix()}"
                --output "{netlist_file_path.as_posix()}"
                --format orcadpcb2
        ''')



        # Parse the outputted netlist file.

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

                    coupled_connectors[key][str(coordinate)].nets += [
                        (schematic_file_path, net_name)
                    ]



    # Check for any discrepancies between coupled connectors.

    for coupled_connector_name, coupled_pins in coupled_connectors.items():

        for coupled_pin_coordinate, coupled_pin in coupled_pins.items():



            # This pin only has one net tied to it and
            # it does not match with the coupled pin's name.

            if len(net_names := [
                net_name
                for schematic_file_path, net_name in coupled_pin.nets
                if not net_name.startswith('unconnected-')
            ]) == 1 and coupled_pin.name != net_names[0]:

                logger.warning(
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
                net_name
                for schematic_file_path, net_name in coupled_pin.nets
                if not net_name.startswith('unconnected-')
            )) >= 2:

                logger.warning(
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



    # List the keybindings in the help message.

    if parameters is None:

        more_help = ''

        for just_keystroke, just_description in justify([
            (
                ('<' , f'<{keybinding.keystroke}>'),
                (None, keybinding.description     ),
            )
            for keybinding in keybindings
        ]):

            more_help += f'{just_keystroke} {just_description}\n'

        return more_help



    # Some state variables.

    pygame             = import_pygame()
    serial, list_ports = import_pyserial()

    pygame.init()

    screen = pygame.display.set_mode((1080, 720), pygame.RESIZABLE)
    clock  = pygame.time.Clock()
    quit   = False

    surface = pygame.Surface(OVCAM_RESOLUTION)
    surface.fill((74, 65, 42))



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

    image_data = None

    def stream_callback(event, new_data):

        nonlocal image_data

        match event:



            # Initializing for the new image.

            case 'starting':

                image_data = b''



            # Adding the new information to the image.

            case 'continuing':

                image_data += new_data



            # Adding the last bit of data to the image and rendering it.

            case 'ending':

                image_data += new_data



                # Check for consistency of the data.

                got      = len(image_data)
                expected = OVCAM_RESOLUTION[0] * OVCAM_RESOLUTION[1] * 3

                if got != expected:

                    logger.error(f'Got {got} bytes; expected {expected} bytes.')

                    return



                # Plot the data.

                for y in range(OVCAM_RESOLUTION[1]):

                    for x in range(OVCAM_RESOLUTION[0]):

                        pixel_i = y * OVCAM_RESOLUTION[0] + x
                        pixel   = (
                            image_data[pixel_i * 3 + 2],
                            image_data[pixel_i * 3 + 0],
                            image_data[pixel_i * 3 + 1],
                        )

                        surface.set_at((x, y), pixel)



            case idk: raise RuntimeError(idk)



    # Window loop.

    while not quit:



        # Handle inputs.

        dt = clock.tick(60) / 1000

        for event in pygame.event.get():

            match event.type:



                case pygame.QUIT:
                    quit = True



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

                match (token_expected, next_token_found):



                    # No start token yet,
                    # so the data is meaningless to us.

                    case (TV_TOKEN.START, None):

                        pass



                    # Start token found! The following
                    # stream data will be image data.

                    case (TV_TOKEN.START, TV_TOKEN.START):

                        stream_callback('starting', b'')

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

                        stream_callback('continuing', data_before_token)



                    # Uh oh, image data stream is starting
                    # again for some reason...

                    case (TV_TOKEN.END, TV_TOKEN.START):

                        logger.error('Start token was found instead of an end token; restarting image stream.')

                        stream_callback('starting', b'')



                    # We've received the last of the image data.

                    case (TV_TOKEN.END, TV_TOKEN.END):

                        stream_callback('ending', data_before_token)

                        token_expected = TV_TOKEN.START



                    case idk: raise RuntimeError(idk)



                # We've processed the stream
                # as much as we have possibly can.

                if len(stream_data) < max(len(TV_TOKEN.START), len(TV_TOKEN.END)):
                    break



        # Render.

        pygame.display.update(
            screen.blit(
                pygame.transform.scale(
                    surface,
                    (screen.get_width(), screen.get_height())
                ),
                (0, 0)
            )
        )



################################################################################



try:

    main_interface.invoke(sys.argv[1:])

except KeyboardInterrupt:

    logger.error('Interrupted by keyboard.')

except ExecuteShellCommandNonZeroExitCode:

    logger.error('Shell command exited with non-zero exit code.')
