#meta types, log, ANSI,    root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, put_title, CMSIS_SET, CMSIS_WRITE, CMSIS_SPINLOCK, CMSIS_TUPLE :
from deps.pxd.utils import root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, c_repr
from deps.pxd.log   import log, ANSI
import types



################################################################################################################################



# Helper routine to make the output look nice and well divided.

def put_title(title = None):

    if title is None:

        Meta.line(f'''

            {"/" * 128}

        ''')

    else:

        Meta.line(f'''

            {"/" * 64} {title} {"/" * 64}

        ''')



################################################################################################################################



# @/url:`https://github.com/PhucXDoan/phucxdoan.github.io/wiki/Macros-for-Reading-and-Writing-to-Memory%E2%80%90Mapped-Registers`.

class CMSIS_MODIFY:



    # This class is just a support to define CMSIS_SET
    # and CMSIS_WRITE in the meta-preprocessor;
    # the only difference in the output is which
    # CMSIS_SET/WRITE macro is being used.

    def __init__(self, macro):
        self.macro = macro



    # The most common usage of CMSIS_SET/WRITE in
    # the meta-preprocessor is to mimic how the
    # CMSIS_SET/WRITE macros would be called in C code.

    def __call__(self, *modifies):



        # e.g:
        # >
        # >    CMSIS_SET(
        # >        ('SDMMC', 'DCTRL', 'DTEN', True),   ->   CMSIS_SET('SDMMC', 'DCTRL', 'DTEN', True)
        # >    )
        # >

        if len(modifies) == 1:
            modifies, = modifies



        # e.g:
        # >
        # >    CMSIS_SET(x for x in xs)
        # >

        modifies = tuple(modifies)



        # e.g:
        # >
        # >    CMSIS_SET(())
        # >

        if len(modifies) == 0:
            return



        # If multiple fields of the same register are to be modified,
        # the caller can format it to look like a table.
        # e.g:
        # >
        # >    CMSIS_SET(                    CMSIS_SET(
        # >        'SDMMC' , 'DCTRL',            ('SDMMC', 'DCTRL', 'DTEN'  , True ),
        # >        'DTEN'  , True   ,   ->       ('SDMMC', 'DCTRL', 'DTDIR' , False),
        # >        'DTDIR' , False  ,            ('SDMMC', 'DCTRL', 'DTMODE', 0b10 ),
        # >        'DTMODE', 0b10   ,        )
        # >    )
        # >

        if not isinstance(modifies[0], tuple):
            modifies = tuple(
                (modifies[0], modifies[1], modifies[i], modifies[i + 1])
                for i in range(2, len(modifies), 2)
            )



        # Multiple RMWs to different registers can be done
        # within a single CMSIS_SET/WRITE macro. This is mostly
        # for convenience and only should be done when the
        # effects of the registers are independent of each other.
        # e.g:
        # >
        # >    CMSIS_SET(                                          CMSIS_SET(
        # >        ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),            ('SDMMC', 'DCTRL', 'DTEN' , True ),
        # >        ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),   ->       ('SDMMC', 'DCTRL', 'DTDIR', False),
        # >        ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),        )
        # >        ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),        CMSIS_SET(
        # >    )                                                       ('SDMMC', 'DTIMER', 'DATATIME', 1234),
        # >                                                        )
        # >                                                        CMSIS_SET(
        # >                                                            ('I2C', 'CR1', 'DNF', 0b1001),
        # >                                                        )
        # >

        modifies = dict(coalesce(
            ((peripheral, register), (field, c_repr(value)))
            for peripheral, register, field, value in modifies
        ))



        # Find any field that'd be modified more than once.
        # e.g:
        # >
        # >    CMSIS_SET(
        # >        ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),
        # >        ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),
        # >        ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),
        # >        ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),
        # >        ('I2C'  , 'CR1'   , 'DNF'     , 0b0110), <- Duplicate field!
        # >    )
        # >

        for (peripheral, register), field_values in modifies.items():
            if (field := find_dupe(field for field, value in field_values)) is not ...:
                raise ValueError(f'Modifying field "{field}" more than once.')



        # Output the proper call to the CMSIS_SET/WRITE macro.

        for (peripheral, register), field_values in modifies.items():
            match field_values:

                # Single-lined output.
                # e.g:
                # >
                # >    CMSIS_SET(a, b, c, d)
                # >

                case [(field, value)]:
                    Meta.line(f'''
                        {self.macro}({peripheral}, {register}, {field}, {value});
                    ''')

                # Multi-lined output.
                # e.g:
                # >
                # >    CMSIS_SET(
                # >        a, b,
                # >        c, d,
                # >        e, f,
                # >        g, h,
                # >    )
                # >

                case _:
                    with Meta.enter(self.macro, '(', ');'):
                        for just_lhs, just_rhs in justify(
                            (
                                ('<', lhs),
                                ('<', rhs),
                            )
                            for lhs, rhs in ((peripheral, register), *field_values)
                        ):
                            Meta.line(f'{just_lhs}, {just_rhs},')



    # The CMSIS_SET/WRITE functions in the meta-preprocessor
    # can also be used as a context manager.
    #
    # For example:
    # >
    # >    with CMSIS_SET as sets:
    # >        sets += [('SDMMC', 'DCTRL' , 'DTEN'    , True  )]
    # >        sets += [('SDMMC', 'DTIMER', 'DATATIME', 1234  )]
    # >        sets += [('I2C'  , 'CR1'   , 'DNF'     , 0b1001)]
    # >        sets += [('SDMMC', 'DCTRL' , 'DTDIR'   , False )]
    # >
    #
    # The above is equivalent to:
    # >
    # >    CMSIS_SET(
    # >        ('SDMMC', 'DCTRL' , 'DTEN'    , True  ),
    # >        ('SDMMC', 'DTIMER', 'DATATIME', 1234  ),
    # >        ('I2C'  , 'CR1'   , 'DNF'     , 0b1001),
    # >        ('SDMMC', 'DCTRL' , 'DTDIR'   , False ),
    # >   )
    # >
    #
    # The benefit of this syntax is that the logic
    # can be arbitrarily complicated in determining
    # what fields of what registers need to be modified.
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
    # e.g:
    # >
    # >    CMSIS_SPINLOCK(
    # >        ('IWDG' , 'SR' , 'WVU'    , False ),
    # >        ('IWDG' , 'SR' , 'RVU'    , True  ),
    # >        ('FLASH', 'ACR', 'LATENCY', 0b1111),
    # >    )
    # >

    if all(isinstance(spinlock, tuple) for spinlock in spinlocks):
        pass



    # Spinlocking on just a single field.
    # e.g:
    # >
    # >    CMSIS_SPINLOCK('IWDG', 'SR', 'WVU', False)
    # >

    else:
        spinlocks = [spinlocks]



    #
    # Output the code to spinlock on the fields until they match the expected value.
    # e.g:
    # >
    # >    CMSIS_SPINLOCK('FLASH', 'ACR', 'LATENCY', 0b1111)   ->   while (CMSIS_GET(FLASH, ACR, LATENCY) != 0b1111);
    # >
    #

    for peripheral, register, field, value in spinlocks:
        match value:
            case True  : Meta.line(f'while (!CMSIS_GET({peripheral}, {register}, {field}));')
            case False : Meta.line(f'while (CMSIS_GET({peripheral}, {register}, {field}));')
            case _     : Meta.line(f'while (CMSIS_GET({peripheral}, {register}, {field}) != {value});')



################################################################################################################################



# Helper routine to make a CMSIS tuple from a database entry easily.

def CMSIS_TUPLE(entry):
    return '(struct CMSISPutTuple) {{ {}, {}, {} }}'.format(
        f'&{entry.peripheral}->{entry.register}',
        f'{entry.peripheral}_{entry.register}_{entry.field}_Pos',
        f'{entry.peripheral}_{entry.register}_{entry.field}_Msk',
    )



################################################################################################################################



# This file is the meta-directive where we
# define any global functions/constants
# for all other meta-directives to have
# without having them to explicitly import it.
