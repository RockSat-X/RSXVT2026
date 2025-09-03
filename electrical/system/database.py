#meta SYSTEM_DATABASE



# The system database for a particular MCU is just a dictionary
# but with an extended method to create a CMSIS tuple easily.
# e.g:
# >
# >    'I2C1_RESET'
# >
# >         |
# >         v
# >
# >    '{ &RCC->APB1LRSTR, RCC_APB1LRSTR_I2C1RST_Pos, RCC_APB1LRSTR_I2C1RST_Msk }'
# >

class SystemDatabaseDict(dict): # TODO Better error message for unknown tag?

    def tuple(self, tag):

        entry = self[tag]

        return '{{ {}, {}, {} }}'.format(
            f'&{entry.peripheral}->{entry.register}',
            f'{entry.peripheral}_{entry.register}_{entry.field}_Pos',
            f'{entry.peripheral}_{entry.register}_{entry.field}_Msk',
        )



# Parse each MCU's database expression.

SYSTEM_DATABASE = {}

for mcu in MCUS:



    # Load and evaluate the Python expression.

    database_file_path = root(f'./deps/mcu/{mcu}_database.py')

    if not database_file_path.is_file():
        raise RuntimeError(
            f'File "{database_file_path}" does not exist; '
            f'it is needed to describe the properties of the MCU.'
        )

    constants, location_tree = eval(database_file_path.read_text(), {}, {})
    entries                  = []



    # Parse the constants.

    for constant in constants:

        match constant:



            # The constant's value is directly given.
            # e.g:
            # >
            # >    ('APB_UNITS', (1, 2, 3))
            # >

            case (tag, value):

                entries += [(tag, types.SimpleNamespace(
                    value = value,
                ))]



            # The constant's value is an inclusive min-max range.
            # e.g:
            # >
            # >    ('PLL_CHANNEL_FREQ', 1_000_000, 250_000_000)
            # >

            case (tag, minimum, maximum):

                entries += [(tag, types.SimpleNamespace(
                    minimum = minimum,
                    maximum = maximum,
                ))]



            # Unsupported constant.

            case unknown:

                raise ValueError(
                    f'Database constant entry is '
                    f'in an unknown format: {repr(unknown)}.'
                )



    # Parse the locations.

    for peripheral, *register_tree in location_tree:

        for register, *field_tree in register_tree:

            for location in field_tree:

                match location:



                    # The location entry's value is assume to be 1 bit.
                    # e.g:
                    # >
                    # >    ('PWR',
                    # >        ('SR1',
                    # >            ('ACTVOS', 'CURRENT_ACTIVE_VOS'),
                    # >        ),
                    # >    )
                    # >

                    case (field, tag):

                        entries += [(tag, types.SimpleNamespace(
                            peripheral = peripheral,
                            register   = register,
                            field      = field,
                            value      = (False, True),
                        ))]



                    # The location entry's value is directly given.
                    # e.g:
                    # >
                    # >    ('IWDG',
                    # >        ('KR',
                    # >            ('KEY', 'IWDG_KEY', (
                    # >                '0xAAAA',
                    # >                '0x5555',
                    # >                '0xCCCC',
                    # >            )),
                    # >        ),
                    # >    )
                    # >

                    case (field, tag, value):

                        entries += [(tag, types.SimpleNamespace(
                            peripheral = peripheral,
                            register   = register,
                            field      = field,
                            value      = value,
                        ))]



                    # The location entry's value is an inclusive min-max range.
                    # e.g:
                    # >
                    # >    ('SysTick',
                    # >        ('LOAD',
                    # >            ('RELOAD', 'SYSTICK_RELOAD', 1, (1 << 24) - 1),
                    # >        ),
                    # >    )
                    # >

                    case (field, tag, minimum, maximum):

                        entries += [(tag, types.SimpleNamespace(
                            peripheral = peripheral,
                            register   = register,
                            field      = field,
                            minimum    = minimum,
                            maximum    = maximum,
                        ))]



                    # Unsupported location format.

                    case unknown:

                        raise ValueError(
                            f'Database location entry is '
                            f'in an unknown format: {repr(unknown)}.'
                        )



    # Check for consistency.

    if (dupe := find_dupe(tag for tag, entry in entries)) is not ...:
        raise ValueError(
            f'For {mcu}, '
            f'there is already a database entry with the tag {repr(dupe)}.'
        )

    if (dupe := find_dupe(
        (entry.peripheral, entry.register, entry.field)
        for tag, entry in entries
        if 'peripheral'  in entry.__dict__
    )) is not ...:
        raise ValueError(
            f'For {mcu}, '
            f'there is already a database entry with the location {repr(dupe)}.'
        )

    SYSTEM_DATABASE[mcu] = SystemDatabaseDict(entries)



################################################################################



# The microcontroller database contains information found in reference manuals
# and datasheets. The point of the database is to make it easy to port common
# code between multiple microcontrollers without having to worry about the exact
# naming conventions and specified values of each register / hardware properties.
#
# For instance, STM32 microcontrollers typically have a PLL unit to generate bus
# frequencies for other parts of the MCU to use. How many PLL units and channels,
# what the exact range of multipliers and dividers are, and so on would all be
# described in the database file (as found in <./deps/mcu/{MCU}_database.py>).
#
# This in turns makes the logic that needs to brute-force the PLL units for the
# clock-tree be overlapped with other microcontrollers. There will always be
# some slight differences that the database can't account for, like whether or
# not each PLL unit has its own clock source or they all share the same one,
# but this layer of abstraction helps a lot with reducing the code duplication.
#
# The database is just defined as a Python expression in a file that we then
# evaluate; we do a small amount of post-processing so it's more usable, but
# overall it's really not that complicated.
#
# The database is not at all comprehensive. When working with low-level system
# stuff and trying to automate things with meta-directives, you might find
# yourself seeing that there's missing entries in the database. This is expected;
# just add more registers and stuff to the database whenever needed. The syntax
# of the database expression should be pretty straight-forward to figure out.
