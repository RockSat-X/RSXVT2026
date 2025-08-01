#meta SYSTEM_DATABASE

import collections, re
from deps.pxd.sexp import parse, deatomize, Atom



# Routine for converting S-expressions of database entries into a more usable Python value.

def parse_entry(entry):



    # Get the entry tag and parameters.

    if not entry:
        assert False


    tag, *parameters = entry
    tag              = tag.string
    record           = Record()



    # Look for the structured entry value.

    for parameter_i, parameter in enumerate(parameters):

        match parameter:



            # The entry value is directly given.

            case (Atom('value:'), value):
                record.VALUE = deatomize(value)



            # The entry value is an inclusive range.

            case (Atom('minmax:'), minimum, maximum):
                record.MIN = deatomize(minimum)
                record.MAX = deatomize(maximum)



            # This entry parameter is something else which we'll process later.

            case _: continue



        # We found and processed the structured entry value; no need to process it again.

        del parameters[parameter_i]
        break



    # No structured entry value found, so we assume it's just something like a True/False 1-bit register.

    else:
        record.VALUE = (False, True)



    # We then look for entry properties to extend the entry record.

    parameter_i = 0

    while parameter_i < len(parameters):

        match parameter := parameters[parameter_i]:



            # Entry properties are typically used for filling in placeholders in the entry tag.
            # e.g. (pll{UNIT}_ready (RCC CR PLL3RDY) (UNIT = 3))

            case (property_name, Atom('='), property_value):

                property_name  = deatomize(property_name)
                property_value = deatomize(property_value)

                record[property_name] = property_value



            # This entry parameter is not a property; just skip it.

            case _:
                parameter_i += 1
                continue



        # We found and processed the entry property; no need to process it again.

        del parameters[parameter_i]



    # Process the remaining entry parameters.

    match parameters:



        # Nothing else to do.

        case []:
            pass



        # The entry is also provided with a location.
        # e.g. (iwdg_stopped_during_debug (DBGMCU APB4FZR DBG_IWDG))

        case [(section, register, field)]:
            record.SECTION  = deatomize(section)
            record.REGISTER = deatomize(register)
            record.FIELD    = deatomize(field)



        # Leftover entry parameters.

        case _:
            assert False



    # Finished parsing for the tag and record from the S-expression of the entry.

    return tag, record



# Create a database for each MCU in use.

SYSTEM_DATABASE = {}

for mcu in TARGETS.mcus:

    SYSTEM_DATABASE[mcu] = {}



    # The database for the MCU is expressed as an S-expression in a separate text file that we then parse.

    database_file_path = root(f'./deps/mcu/{mcu}_database.txt')

    if not database_file_path.is_file():
        assert False

    for tag, records in coalesce(map(parse_entry, parse(database_file_path.read_text()))).items():



        # Determine the placeholders in the entry's tag.
        # e.g.
        # 'pll{UNIT}{CHANNEL}_enable'   ->   { 'UNIT', 'CHANNEL' }

        placeholders = OrdSet(re.findall('{(.*?)}', tag))

        if not placeholders and len(records) >= 2:
            raise ValueError(
                f'For {mcu}, multiple database entries were found with the tag "{tag}"; '
                f'there should be a {{...}} in the string.'
            )
            assert False



        # Based on the placeholder count, we grouped together all of the database entries that have the same tag.

        match len(placeholders):



            # No placeholder in the tag, so the user should expect a single entry when querying.
            # e.g.
            # (iwdg_stopped_during_debug (DBGMCU APB4FZR DBG_IWDG))   ->   SYSTEM_DATABASE[mcu]['iwdg_stopped_during_debug']

            case 0:
                SYSTEM_DATABASE[mcu][tag], = records



            # Single placeholder in the tag; organize entries by their value for the placeholder.
            # e.g.
            # (pll{UNIT}_predivider (RCC PLLCKSELR DIVM1) (minmax: 1 63) (UNIT = 1))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][1]
            # (pll{UNIT}_predivider (RCC PLLCKSELR DIVM2) (minmax: 1 63) (UNIT = 2))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
            # (pll{UNIT}_predivider (RCC PLLCKSELR DIVM3) (minmax: 1 63) (UNIT = 3))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][3]

            case 1:
                SYSTEM_DATABASE[mcu][tag] = mk_dict((record[placeholders[0]], record) for record in records)



            # Multiple placeholders in the tag; organize entries by their values for the placeholders using a namedtuple.
            #
            # e.g.
            # (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1PEN) (UNIT = 1) (CHANNEL = p))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'p')]
            # (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1QEN) (UNIT = 1) (CHANNEL = q))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'q')]
            # (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1SEN) (UNIT = 1) (CHANNEL = s))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 's')]
            #

            case _:

                Placeholders = collections.namedtuple('Placeholders', placeholders)

                SYSTEM_DATABASE[mcu][tag] = {
                    Placeholders(**{
                        key : value
                        for key, value in record
                        if key in placeholders
                    }) : record
                    for record in records
                }
