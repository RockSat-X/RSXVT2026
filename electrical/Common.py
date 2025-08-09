#meta types, root, justify, coalesce, mk_dict, find_dupe, OrderedSet, ContainedNamespace, AllocatingNamespace, log, ANSI, CMSIS_SET, CMSIS_WRITE, CMSIS_SPINLOCK, STLINK_BAUD, TARGETS :
# TODO Provide explaination on how this file works?

import types
from deps.pxd.utils import root, justify, coalesce, mk_dict, c_repr, find_dupe, ContainedNamespace, AllocatingNamespace, OrderedSet
from deps.pxd.log   import log, ANSI



################################################################################################################################



class CMSIS_MODIFY:

    # This class is just a support to define CMSIS_SET and CMSIS_WRITE in the meta-preprocessor;
    # the only difference in the output is which CMSIS_SET/WRITE macro is being used.

    def __init__(self, macro):
        self.macro = macro



    # The most common usage of CMSIS_SET/WRITE in the meta-preprocessor is to mimic
    # how the CMSIS_SET/WRITE macros would be called in C code.

    def __call__(self, *modifies):



        # e.g.
        # CMSIS_SET(
        #     ('SDMMC', 'DCTRL', 'DTEN', True),   ->   CMSIS_SET('SDMMC', 'DCTRL', 'DTEN', True)
        # )

        if len(modifies) == 1:
            modifies, = modifies



        # e.g. CMSIS_SET(x for x in xs)

        modifies = tuple(modifies)



        # e.g. CMSIS_SET(())

        if len(modifies) == 0:
            return



        # If multiple fields of the same register are to be modified, the caller can format it to look like a table.
        # e.g.
        # CMSIS_SET(                    CMSIS_SET(
        #     'SDMMC' , 'DCTRL',            ('SDMMC', 'DCTRL', 'DTEN'  , True ),
        #     'DTEN'  , True   ,   ->       ('SDMMC', 'DCTRL', 'DTDIR' , False),
        #     'DTDIR' , False  ,            ('SDMMC', 'DCTRL', 'DTMODE', 0b10 ),
        #     'DTMODE', 0b10   ,        )
        # )

        if not isinstance(modifies[0], tuple):
            modifies = tuple(
                (modifies[0], modifies[1], modifies[i], modifies[i + 1])
                for i in range(2, len(modifies), 2)
            )



        # Multiple RMWs to different registers can be done within a single CMSIS_SET/WRITE macro.
        # This is mostly for convenience and only should be done when the effects of the registers are independent of each other.
        # e.g.
        # CMSIS_SET(                                          CMSIS_SET(
        #     ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),            ('SDMMC', 'DCTRL', 'DTEN' , True ),
        #     ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),   ->       ('SDMMC', 'DCTRL', 'DTDIR', False),
        #     ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),        )
        #     ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),        CMSIS_SET(
        # )                                                       ('SDMMC', 'DTIMER', 'DATATIME', 1234),
        #                                                     )
        #                                                     CMSIS_SET(
        #                                                         ('I2C', 'CR1', 'DNF', 0b1001),
        #                                                     )

        modifies = mk_dict(coalesce(
            ((section, register), (field, c_repr(value)))
            for section, register, field, value in modifies
        ))



        # Find any field that'd be modified more than once.
        # e.g.
        # CMSIS_SET(
        #     ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),
        #     ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),
        #     ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),
        #     ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),
        #     ('I2C'  , 'CR1'   , 'DNF'     , 0b0110), <- Duplicate field!
        # )

        for (section, register), field_values in modifies.items():
            if (field := find_dupe(field for field, value in field_values)) is not ...:
                raise ValueError(f'Modifying field "{field}" more than once.')



        # Output the proper call to the CMSIS_SET/WRITE macro.

        for (section, register), field_values in modifies.items():
            match field_values:

                # Single-lined output.
                # e.g. CMSIS_SET(a, b, c, d)

                case [(field, value)]:
                    Meta.line(f'''
                        {self.macro}({section}, {register}, {field}, {value});
                    ''')

                # Multi-lined output.
                # e.g.
                # CMSIS_SET(
                #     a, b,
                #     c, d,
                #     e, f,
                #     g, h,
                # )

                case _:
                    with Meta.enter(self.macro, '(', ');'):
                        for just_lhs, just_rhs in justify(
                            (
                                ('<', lhs),
                                ('<', rhs),
                            )
                            for lhs, rhs in ((section, register), *field_values)
                        ):
                            Meta.line(f'{just_lhs}, {just_rhs},')



    # The CMSIS_SET/WRITE functions in the meta-preprocessor can also be used as a context manager.
    #
    # For example:
    #
    #     ...
    #
    #     with CMSIS_SET as sets:
    #         sets += [('SDMMC', 'DCTRL' , 'DTEN'    , True  )]
    #         sets += [('SDMMC', 'DTIMER', 'DATATIME', 1234  )]
    #         sets += [('I2C'  , 'CR1'   , 'DNF'     , 0b1001)]
    #         sets += [('SDMMC', 'DCTRL' , 'DTDIR'   , False )]
    #
    #     ...
    #
    # The above is equivalent to:
    #
    #     ...
    #
    #     CMSIS_SET(
    #         ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),
    #         ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),
    #         ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),
    #         ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),
    #    )
    #
    #     ...
    #
    # The benefit of this syntax is that the logic can be arbitrarily
    # complicated in determining what fields of what registers need to be modified.
    # TODO Somehow enforce no code can be generated until the context manager is exited.

    def __enter__(self):
        self.modifies = []
        return self.modifies

    def __exit__(self, *exception_info):
        self(self.modifies)

CMSIS_SET   = CMSIS_MODIFY('CMSIS_SET'  )
CMSIS_WRITE = CMSIS_MODIFY('CMSIS_WRITE')



################################################################################################################################



def CMSIS_SPINLOCK(*spinlocks):



    # Spinlocking on multiple fields.
    # e.g.
    # CMSIS_SPINLOCK(
    #     ('IWDG' , 'SR' , 'WVU'    , False ),
    #     ('IWDG' , 'SR' , 'RVU'    , True  ),
    #     ('FLASH', 'ACR', 'LATENCY', 0b1111),
    # )
    if all(isinstance(spinlock, tuple) for spinlock in spinlocks):
        pass



    # Spinlocking on just a single field.
    # e.g. CMSIS_SPINLOCK('IWDG', 'SR', 'WVU', False)
    else:
        spinlocks = [spinlocks]



    #
    # Output the code to spinlock on the fields until they match the expected value.
    # e.g.
    # CMSIS_SPINLOCK('FLASH', 'ACR', 'LATENCY', 0b1111)   ->   while (CMSIS_GET(FLASH, ACR, LATENCY) != 0b1111);
    #
    for section, register, field, value in spinlocks:
        match value:
            case True  : Meta.line(f'while (!CMSIS_GET({section}, {register}, {field}));')
            case False : Meta.line(f'while (CMSIS_GET({section}, {register}, {field}));')
            case _     : Meta.line(f'while (CMSIS_GET({section}, {register}, {field}) != {value});')



################################################################################################################################


#
# Communication speed with the ST-Link.
#

STLINK_BAUD = 1_000_000



#
# Build directory where all build artifacts will be dumped to.
#

BUILD = root('./build')



#
# Specialized tuple so that we can have a tuple of different firmware targets,
# but also some additional properties to make it easy to work with.
#

class TargetTuple(tuple):



    def __new__(cls, elements):
        return super(TargetTuple, cls).__new__(cls, elements)



    def __init__(self, elements):
        self.mcus = OrderedSet(target.mcu for target in self) # Set of all MCUs in use.



    # Get a specific target by name.

    def get(self, target_name):

        if not (matches := [target for target in self if target.name == target_name]):
            raise ValueError(f'Undefined target: {repr(target_name)}.')

        target, = matches

        return target



#
# Basic description of each of our firmware targets.
#

TARGETS = TargetTuple((

    types.SimpleNamespace(
        name              = 'SandboxNucleoH7S3L8',
        mcu               = 'STM32H7S3',
        cmsis_file_path   = root('./deps/cmsis_device_h7s3l8/Include/stm32h7s3xx.h'),
        source_file_paths = root('''
            ./electrical/SandboxNucleoH7S3L8.c
            ./electrical/system/Startup.S
        '''),
        include_file_paths = (
            root('./deps/cmsis_device_h7s3l8/Include'),
            root('./deps/FreeRTOS_Kernel/include'),
            root('./deps/FreeRTOS_Kernel/portable/GCC/ARM_CM7/r0p1'),
        ),
        stack_size = 8192, # TODO This might be removed depending on how FreeRTOS works.
    ),

))



#
# Figure out the compiler and linker flags for each firmware target.
#

for target in TARGETS:



    # Flags for both the compiler and linker.

    architecture_flags = '''
        -mcpu=cortex-m7
        -mfloat-abi=hard
    '''



    # Warning configurations.

    enabled_warnings = '''
        error
        all
        extra
        switch-enum
        undef
        fatal-errors
        strict-prototypes
        shadow
        switch-default
    '''

    disabled_warnings = '''
        unused-function
        main
        double-promotion
        conversion
        unused-variable
        unused-parameter
        comment
        unused-but-set-variable
        format-zero-length
        unused-label
    '''


    # Additional search paths for the compiler to search through for #includes.

    include_file_paths = target.include_file_paths + (
        root(BUILD, 'meta'),
        root('./deps/CMSIS_6/CMSIS/Core/Include'),
        root('./deps'),
        root('./electrical'),
        root('./deps/printf/src'),
    )



    # Additional macro defines.

    defines = [
        ('TARGET_NAME'    , target.name      ),
        ('LINK_stack_size', target.stack_size),
    ]

    for other in TARGETS:
        defines += [
            (f'TARGET_NAME_IS_{other.name}', int(other.name == target.name)),
            (f'TARGET_MCU_IS_{other.mcu}'  , int(other.mcu  == target.mcu )),
        ]



    # The target's final set of compiler flags. @/`Linker Garbage Collection`.

    target.compiler_flags = (
        f'''
            {architecture_flags}
            -O0
            -ggdb3
            -std=gnu23
            -fmax-errors=1
            -fno-strict-aliasing
            -fno-eliminate-unused-debug-types
            {'\n'.join(f'-D {name}={value}'    for name, value in defines                  )}
            {'\n'.join(f'-W{name}'             for name        in enabled_warnings .split())}
            {'\n'.join(f'-Wno-{name}'          for name        in disabled_warnings.split())}
            {'\n'.join(f'-I {repr(str(path))}' for path        in include_file_paths       )}
            -ffunction-sections
        '''
    )



    # The target's final set of linker flags. @/`Linker Garbage Collection`.

    target.linker_flags = f'''
        {architecture_flags}
        -nostdlib
        -lgcc
        -lc
        -Xlinker --fatal-warnings
        -Xlinker --gc-sections
    '''



################################################################################################################################

# @/`Linker Garbage Collection`:
#
# The `-ffunction-sections` makes the compiler generate a section for every function.
# This will allow the linker to later on garbage-collect any unused functions via `--gc-sections`.
# This isn't necessarily for space-saving reasons, but for letting us compile with libraries without
# necessarily defining all user-side functions until we use a library function that'd depend upon it.
# An example would be `putchar_` for eyalroz's `printf` library.
#
# There's a similar thing for data with the flag `-fdata-sections`, but if we do this, then we won't be
# able to reference any variables that end up being garbage-collected when we debug. This isn't a big
# deal, but it is annoying when it happens, so we'll skip out on GC'ing data and only do functions.
