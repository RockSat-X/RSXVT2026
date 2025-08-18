#meta SYSTEM_DATABASE, verify_and_get_field_names_in_tag_order

import re
from deps.pxd.sexp import parse_sexp



################################################################################



# TODO.

def verify_and_get_field_names_in_tag_order(tag, given_fields):

    tag_field_names   = OrderedSet(re.findall('{(.*?)}', tag))
    given_field_names = OrderedSet(given_fields.keys())

    if differences := tag_field_names - given_field_names:
        raise ValueError(
            f'Tag {repr(tag)} is missing the value '
            f'for the field {repr(differences[0])}.'
        )

    if differences := given_field_names - tag_field_names:
        raise ValueError(
            f'Tag {repr(tag)} has no field {repr(differences[0])}.'
        )

    return tag_field_names



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

    for name, value in database_py['value']:
        SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
            value = value,
        )

    for name, minimum, maximum in database_py['minmax']:
        SYSTEM_DATABASE[mcu][name] = types.SimpleNamespace(
            minimum = minimum,
            maximum = maximum,
        )

    for section, *registers in database_py['registers']:
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
