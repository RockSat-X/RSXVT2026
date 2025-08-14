#! /usr/bin/env python3



################################################################################################################################



# Built-in modules.

import types, sys, shlex, pathlib, shutil, subprocess, time



# The PXD module.

try:

    import deps.pxd.metapreprocessor
    import deps.pxd.cite
    from   deps.pxd.ui    import ExitCode
    from   deps.pxd.log   import log, ANSI, Indent
    from   deps.pxd.utils import root, justify

except ModuleNotFoundError as error:

    if 'pxd' not in error.name:
        raise # Likely a bug in the PXD module.

    print(f'[ERROR] Could not import "{error.name}"; maybe the Git submodules need to be initialized/updated? Try doing:')
    print(f'        > git submodule update --init --recursive')
    print(f'        If this still doesn\'t work, please raise an issue.')

    sys.exit(1)



# Common definitions with the meta-preprocessor.

from electrical.Shared import *



# Import handler for PySerial.

def import_pyserial():

    try:

        import serial
        import serial.tools.list_ports

    except ModuleNotFoundError as error:

        with ANSI('fg_red'), Indent('[ERROR] ', hanging = True):
            log(f'''
                Python got {type(error).__name__} ({error}); try doing:
                {ANSI('> pip install pyserial', 'bold')}
            ''')

        raise ExitCode(1)

    return serial, serial.tools.list_ports



################################################################################################################################
#
# Routine for ensuring the user has the required programs on their machine (and provide good error messages if not).
#

def require(*needed_programs):

    missing_program = next((program for program in needed_programs if shutil.which(program) is None), None)

    if not missing_program:
        return # The required programs were found on the machine.



    with ANSI('fg_red'), Indent('[ERROR] ', hanging = True):



        # PowerShell.

        if missing_program in (roster := [
            'pwsh',
        ]):

            log(f'''
                Python couldn\'t find "{missing_program}" in your PATH; have you installed PowerShell yet?
                {ANSI(f'> https://apps.microsoft.com/detail/9MZ1SNWT0N5D', 'bold')}
                Installing PowerShell via Windows Store is the most convenient way.
            ''')



        # STM32CubeCLT.

        elif missing_program in (roster := [
            'STM32_Programmer_CLI',
            'ST-LINK_gdbserver'
            'arm-none-eabi-gcc',
            'arm-none-eabi-cpp',
            'arm-none-eabi-objcopy',
            'arm-none-eabi-gdb',
        ]):

            log(f'''
                Python couldn't find "{missing_program}" in your PATH; have you installed STM32CubeCLT yet?
                {ANSI(f'> https://www.st.com/en/development-tools/stm32cubeclt.html', 'bold')}
                Install and then make sure all of these commands are available in your PATH:
            ''')

            for program in roster:
                if shutil.which(program) is not None:
                    log(ANSI(f'    - [located] {program}', 'fg_green'))
                else:
                    log(f'    - [missing] {program}')



        # Picocom.

        elif missing_program in (roster := [
            'picocom'
        ]):

            log(f'''
                Python couldn't find "{missing_program}" in your PATH; have you installed it yet?
                If you're on a Debian-based distro, this is just simply:
                {ANSI(f'> sudo apt install picocom', 'bold')}
                Otherwise, you should only be getting this message on some other Linux distribution
                to which I have faith in you to figure this out on your own.
            ''')



        # Make.

        elif missing_program in (roster := [
            'make'
        ]):

            log(f'''
                Python couldn't find "{missing_program}" in your PATH; have you installed it yet?
                If you're on a Windows system, run the following command and restart your shell:
                {ANSI(f'> winget install ezwinports.make', 'bold')}
            ''')



        # Not implemented.

        else:
            log(f'Python couldn\'t find "{missing_program}" in your PATH; have you installed it yet?')



    raise ExitCode(1)



################################################################################################################################
#
# Routine for logging out ST-Links as a table.
#

def log_stlinks(stlinks):

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
    ):
        log(f'| {' | '.join(justs)} |')



################################################################################################################################
#
# Routine for finding ST-Links.
#

def request_stlinks(
    *,
    specific_one         = False,
    specific_probe_index = None,
):



    # Parse output of STM32_Programmer_CLI's findings.
    # e.g:
    # >
    # >    ST-Link Probe 0 :
    # >       ST-LINK SN  : 003F00493133510F37363734
    # >       ST-LINK FW  : V3J15M7
    # >       Access Port Number  : 2
    # >       Board Name  : NUCLEO-H7S3L8
    # >

    require('STM32_Programmer_CLI')

    listing_lines = subprocess.check_output(['STM32_Programmer_CLI', '--list', 'st-link']).decode('utf-8').splitlines()
    stlinks       = [
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
        stlink.comport, = [comport for comport in comports if comport.serial_number == stlink.serial_number]



    # If a specific probe index was given, give back that specific ST-Link.

    if specific_probe_index is not None:

        if not stlinks:
            log(ANSI(f'[ERROR] No ST-Links found.', 'fg_red'))
            raise ExitCode(1)

        if not (matches := [stlink for stlink in stlinks if stlink.probe_index == specific_probe_index]):
            log_stlinks(stlinks)
            log()
            log(ANSI(f'[ERROR] No ST-Links found with probe index of "{specific_probe_index}".', 'fg_red'))
            raise ExitCode(1)

        stlink, = matches

        return stlink



    # If the caller is assuming there's only one ST-Link, then give back the only one.

    if specific_one:

        if not stlinks:
            log(ANSI(f'[ERROR] No ST-Links found.', 'fg_red'))
            raise ExitCode(1)

        if len(stlinks) >= 2:
            log_stlinks(stlinks)
            log()
            log(ANSI(f'[ERROR] Multiple ST-Links found; I don\'t know which one to use.', 'fg_red'))
            raise ExitCode(1)

        stlink, = stlinks

        return stlink



    # Otherwise, give back the list of the ST-Links we found (if any).

    return stlinks



################################################################################################################################
#
# Routine for executing shell commands.
#

def execute(
    default               = None,  # Typically for when the command for cmd.exe and bash are the same (e.g. "echo hello!").
    *,
    bash                  = None,
    cmd                   = None,  # PowerShell is slow to invoke, so cmd.exe would be used if its good enough.
    powershell            = None,  # "
    keyboard_interrupt_ok = False,
    nonzero_exit_code_ok  = False
):

    # Determine the shell command we'll be executing based on the operating system.

    if cmd is not None and powershell is not None:
        raise RuntimeError(
            f'CMD and PowerShell commands cannot be both provided; '
            f'please raise an issue or patch the script yourself.'
        )

    match sys.platform:

        case 'win32':
            use_powershell = cmd is None
            command        = powershell if use_powershell else cmd

        case _:
            command        = bash
            use_powershell = False

    if command is None:
        command = default

    if command is None:
        raise RuntimeError(
            f'Missing shell command for platform "{sys.platform}"; '
            f'please raise an issue or patch the script yourself.'
        )

    if use_powershell:
        require('pwsh')



    # Carry out the shell command.

    lexer                  = shlex.shlex(command)
    lexer.quotes           = '"'
    lexer.whitespace_split = True
    lexer.commenters       = ''
    lexer_parts            = list(lexer)
    command                = ' '.join(lexer_parts)

    log(
        '{} {}',
        ANSI(f'$ {lexer_parts[0]}', 'fg_bright_green'),
        ' '.join(lexer_parts[1:]),
    )

    if use_powershell:
        command = ['pwsh', '-Command', command]

    try:
        subprocess_exit_code = subprocess.call(command, shell = True)
    except KeyboardInterrupt:
        if keyboard_interrupt_ok:
            subprocess_exit_code = None
        else:
            raise

    if subprocess_exit_code and not nonzero_exit_code_ok:
        log()
        log(ANSI(f'[ERROR] Shell command exited with a non-zero code of {subprocess_exit_code}.', 'fg_red'))
        raise ExitCode(subprocess_exit_code)

    return subprocess_exit_code



################################################################################################################################



def ui_verb_hook(verb, parameters):

    start = time.time()
    yield
    end = time.time()

    if (elapsed := end - start) >= 0.5:
        log()
        log(f'> "{verb.name}" took: {elapsed :.3f}s')



ui = deps.pxd.ui.UI(
    f'{root(pathlib.Path(__file__).name)}',
    f'The command line program (pronounced "clippy").',
    ui_verb_hook,
)



################################################################################################################################



@ui(
    {
        'description' : 'Delete all build artifacts.',
    },
)
def clean(parameters):

    directories = [
        BUILD,
    ]

    for directory in directories:
        execute(
            bash = f'''
                rm -rf {repr(str(directory))}
            ''',
            cmd = f'''
                if exist {repr(str(directory))} rmdir /S /Q {repr(str(directory))}
            ''',
        )



################################################################################################################################



@ui(
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
        targets = TARGETS
    else:
        targets = [parameters.target]



    # Logging routine for outputting nice dividers in stdout.

    def log_header(message):

        log(f'''

            {ANSI(f'{'>' * 32} {message} {'<' * 32}', 'bold', 'fg_bright_black')}

        ''')



    # Determine the files for the meta-preprocessor to scan through.

    metapreprocessor_file_paths = [
        pathlib.Path(root, file_name)
        for root, dirs, file_names in root('./electrical').walk()
        for file_name in file_names
        if file_name.endswith(('.c', '.h', '.py', '.ld', '.S'))
    ]



    # Callback of things to do before and after the execution of a meta-directive.

    elapsed = 0

    def metadirective_callback(index, meta_directives):

        nonlocal elapsed



        # Show some of the symbols that this meta-directive will export.

        export_preview = ', '.join(meta_directives[index].exports)

        if export_preview:

            if len(export_preview) > (cutoff := 64): # This meta-directive is exporting a lot of stuff, so we trim the list.
                export_preview = ', '.join(export_preview[:cutoff].rsplit(',')[:-1] + ['...'])

            export_preview = ' ' + export_preview



        # Log the evaluation of the meta-directive.

        just_nth, just_src, just_line = justify(
            (
                ('<', meta_directive_i + 1                  ),
                ('<', meta_directive.source_file_path       ),
                ('<', meta_directive.meta_header_line_number),
            )
            for meta_directive_i, meta_directive in enumerate(meta_directives)
        )[index]

        log(f'| {just_nth}/{len(meta_directives)} | {just_src} : {just_line} |{export_preview}')



        # Record how long it takes to run this meta-directive.

        start  = time.time()
        output = yield
        end    = time.time()
        delta  = end - start

        if delta > 0.050:
            log(ANSI(f'^ {delta :.3}s', 'fg_yellow')) # Warn that this meta-directive took quite a while to execute.

        elapsed += delta



    # Begin meta-preprocessing!

    log_header('Meta-preprocessing')

    try:
        deps.pxd.metapreprocessor.do(
            output_directory_path = root(BUILD, 'meta'),
            source_file_paths     = metapreprocessor_file_paths,
            callback              = metadirective_callback,
        )
    except deps.pxd.metapreprocessor.MetaError as error:
        error.dump()
        raise ExitCode(1)

    log()
    log(ANSI(f'# Meta-preprocessor : {elapsed :.3f}s.', 'fg_magenta'))

    if parameters.metapreprocess_only:
        raise ExitCode(0)



    # Compile each source.

    require(
        'arm-none-eabi-gcc',
    )

    for target in targets:

        log_header(f'Compiling "{target.name}"')

        for source_i, source in enumerate(target.source_file_paths):

            object = root(BUILD, target.name, source.stem + '.o')

            object.parent.mkdir(parents = True, exist_ok = True)

            if source_i:
                log()

            execute(f'''
                arm-none-eabi-gcc
                    -o {repr(str(object))}
                    -c {repr(str(source))}
                    {target.compiler_flags}
            ''')



    # Link the firmware.

    require(
        'arm-none-eabi-cpp',
        'arm-none-eabi-objcopy',
        'arm-none-eabi-gdb',
    )

    for target in targets:

        log_header(f'Linking "{target.name}"')

        # Preprocess the linker file.
        execute(f'''
            arm-none-eabi-cpp
                {target.compiler_flags}
                -E
                -x c
                -o {repr(str(root(BUILD, target.name, 'link.ld')))}
                {repr(str(root('./electrical/system/link.ld')))}
        ''')

        log()

        # Link object files.
        execute(f'''
            arm-none-eabi-gcc
                -o {repr(str(root(BUILD, target.name, target.name + '.elf')))}
                -T {repr(str(root(BUILD, target.name, 'link.ld')))}
                {' '.join(
                    repr(str(root(BUILD, target.name, source.stem + '.o')))
                    for source in target.source_file_paths
                )}
                {target.linker_flags}
        ''')

        log()

        # Turn ELF into raw binary.
        execute(f'''
            arm-none-eabi-objcopy
                -S
                -O binary
                {repr(str(root(BUILD, target.name, target.name + '.elf')))}
                {repr(str(root(BUILD, target.name, target.name + '.bin')))}
        ''')



    # Done!

    log_header(f'Hip-hip hooray! Built {', '.join(f'"{target.name}"' for target in targets)}!')



################################################################################################################################



@ui(
    {
        'description' : 'Flash the binary to the MCU.',
    },
    {
        'name'        : 'target',
        'description' : 'Name of the target MCU to program.',
        'type'        : { target.name : target for target in TARGETS },
        'default'     : TARGETS[0] if len(TARGETS) == 1 else ...,
    },
)
def flash(parameters):



    # Begin flashing the MCU, which might take multiple tries.

    require('STM32_Programmer_CLI')

    stlink   = request_stlinks(specific_one = True)
    attempts = 0

    while True:



        # Maxed out attempts?

        if attempts == 3:
            with ANSI('fg_red'):
                log('''

                    [ERROR] Failed to flash; this might be because...
                            - the binary file haven\'t been built yet.
                            - the ST-Link is being used by a another program.
                            - the ST-Link has disconnected.
                            - ... or something else entirely!
                ''')
            raise ExitCode(1)



        # Not the first try?

        elif attempts:
            log('''

                {ANSI('[WARNING] Failed to flash (maybe due to verification error); trying again...', 'fg_yellow')}

            ''')



        # Try flashing.

        exit_code = execute(f'''
            STM32_Programmer_CLI
                --connect port=SWD index={stlink.probe_index}
                --download {repr(str(root(BUILD, parameters.target.name, parameters.target.name + '.bin')))} 0x08000000
                --verify
                --start
        ''', nonzero_exit_code_ok = True)



        # Try again if needed.

        if exit_code:
            attempts += 1
        else:
            break



################################################################################################################################



@ui(
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



    # Set up the GDB-server.

    stlink = request_stlinks(specific_one = True)

    require(
        'ST-LINK_gdbserver',
        'STM32_Programmer_CLI',
    )

    apid      = 1 # TODO This is the core ID, which varies board to board... Maybe we just hardcode it?
    gdbserver = f'''
        ST-LINK_gdbserver
            --stm32cubeprogrammer-path {repr(str(pathlib.Path(shutil.which('STM32_Programmer_CLI')).parent))}
            --serial-number {stlink.serial_number}
            --swd
            --apid {apid}
            --verify
            --attach
    '''

    if parameters.just_gdbserver:
        execute(gdbserver, keyboard_interrupt_ok = True)
        return



    # Set up GDB.

    require('arm-none-eabi-gdb')

    gdb_init = f'''
        file {repr(str(root(BUILD, parameters.target.name, parameters.target.name + '.elf').as_posix()))}
        target extended-remote localhost:61234
        with pagination off -- focus cmd
    '''

    gdb = f'''
        arm-none-eabi-gdb -q {' '.join(f'-ex "{inst.strip()}"' for inst in gdb_init.strip().splitlines())}
    '''



    # Launch the debugging session.

    execute(
        bash                  = f'set -m; {gdbserver} 1> /dev/null 2> /dev/null & {gdb}',
        powershell            = f'{gdbserver} & {gdb}',
        keyboard_interrupt_ok = True, # Happens whenever we halt execution in GDB using CTRL-C.
    )



################################################################################################################################



@ui(
    {
        'description' : 'Open the COM serial port of the ST-Link.',
        'help'        : ...,
    },
)
def talk(parameters):



    if parameters.help:

        match sys.platform:
            case 'linux' : log('<ctrl-a ctrl-q> Exit.')
            case 'win32' : log('<ctrl-c> Exit.')
            case 'win32' : log('<ctrl-c> Exit (maybe?).')

        return



    if sys.platform == 'linux':
        require('picocom')

    stlink = request_stlinks(specific_one = True)

    execute(



        # Picocom's pretty good!

        bash = f'''
            picocom --baud={STLINK_BAUD} --quiet --imap=lfcrlf {stlink.comport.device}
        ''',



        # The only reason why PowerShell is used here is because there's no convenient way
        # in Python to read a single character from STDIN with no blocking and buffering.
        # Furthermore, the serial port on Windows seem to be buffered up, so data before the
        # port is opened for reading is available to fetch; PySerial only returns data sent after
        # the port is opened.

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
        ''',



        # Whenever we close the port using CTRL-C, a KeyboardInterrupt exception is raised as a false-positive.

        keyboard_interrupt_ok = True,
    )



################################################################################################################################



@ui(
    {
        'name'        : 'stlinks',
        'description' : 'Search and list for any ST-Links connected to the computer.',
    },
)
def _(parameters):
    if stlinks := request_stlinks():
        log_stlinks(stlinks)
    else:
        log('No ST-Link detected by STM32_Programmer_CLI.')



################################################################################################################################



ui(deps.pxd.cite.ui)



################################################################################################################################



exit(ui.invoke(sys.argv[1:]))
