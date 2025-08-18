#meta SYSTEM_DATABASE




################################################################################



SYSTEM_DATABASE = {}

for mcu in MCUS:

    SYSTEM_DATABASE[mcu] = {}

    database_file_path = root(f'./deps/mcu/{mcu}_database.py')

    if not database_file_path.is_file():
        raise RuntimeError(
            f'File "{database_file_path}" does not exist; '
            f'it is needed to describe the properties of the MCU.'
        )

    database_py = eval(database_file_path.read_text(), {}, {})


    value_part, registers_part = database_py

    for name, *entry in value_part:

        match entry:

            case (value,):
                SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
                    value = value,
                )

            case (minimum, maximum):
                SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
                    minimum = minimum,
                    maximum = maximum,
                )

            case unknown:
                raise ValueError(f'Unknown value: {repr(unknown)}.')

    for section, *registers in registers_part:
        for register, *fields in registers:
            for entry in fields:

                match entry:

                    case (field, name, minimum, maximum):
                        SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            minimum  = minimum,
                            maximum  = maximum,
                        )

                    case (field, name, value):
                        SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            value    = value,
                        )

                    case (field, name):
                        SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
                            section  = section,
                            register = register,
                            field    = field,
                            value    = (False, True),
                        )

                    case unknown:
                        raise ValueError(f'Unknown value: {repr(unknown)}.')




################################################################################



# TODO.
