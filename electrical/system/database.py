#meta SYSTEM_DATABASE

SYSTEM_DATABASE = {}

for mcu in MCUS:

    database_file_path = root(f'./deps/mcu/{mcu}_database.py')

    if not database_file_path.is_file():
        raise RuntimeError(
            f'File "{database_file_path}" does not exist; '
            f'it is needed to describe the properties of the MCU.'
        )

    database_py = eval(database_file_path.read_text(), {}, {})

    value_part, registers_part = database_py

    entries = []

    for name, *entry in value_part:

        match entry:

            case (value,):
                entries += [(name, types.SimpleNamespace(
                    value = value,
                ))]

            case (minimum, maximum):
                entries += [(name, types.SimpleNamespace(
                    minimum = minimum,
                    maximum = maximum,
                ))]

            case unknown:
                raise ValueError(f'Unknown value: {repr(unknown)}.')

    for section, *registers in registers_part:
        for register, *fields in registers:
            for entry in fields:

                match entry:

                    case (field, name, minimum, maximum):
                        entries += [(name, types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            minimum  = minimum,
                            maximum  = maximum,
                        ))]

                    case (field, name, value):
                        entries += [(name, types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            value    = value,
                        ))]

                    case (field, name):
                        entries += [(name, types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            value    = (False, True),
                        ))]

                    case unknown:
                        raise ValueError(f'Unknown value: {repr(unknown)}.')

    if (dupe := find_dupe(name for name, entry in entries)) is not ...:
        raise ValueError(f'For {mcu}, there is already a database entry with the tag {repr(dupe)}.')

    if (dupe := find_dupe(
        (entry.section, entry.register, entry.field)
        for name, entry in entries if 'section' in entry.__dict__
    )) is not ...:
        raise ValueError(f'For {mcu}, there is already a database entry with the location {repr(dupe)}.')

    SYSTEM_DATABASE[mcu] = dict(entries)


################################################################################



# TODO.
