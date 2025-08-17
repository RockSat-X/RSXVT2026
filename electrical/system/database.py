#meta SYSTEM_DATABASE, verify_and_get_placeholders_in_tag_order

import re
from deps.pxd.sexp import parse_sexp



# Routine for converting S-expressions of database entries into a more usable Python value.

def parse_entry(entry):



    # Get the entry tag and parameters.

    tag, *parameters = entry
    record           = AllocatingNamespace()



    # Look for the structured entry value.

    for parameter_i, parameter in enumerate(parameters):

        match parameter:



            # The entry value is directly given.

            case ('.value', value):
                record.value = value



            # The entry value is an inclusive range.

            case ('.minmax', minimum, maximum):
                record.min = minimum
                record.max = maximum



            # This entry parameter is something else which we'll process later.

            case _: continue



        # We found and processed the structured entry value; no need to process it again.

        del parameters[parameter_i]
        break



    # No structured entry value found, so we assume it's just something like a True/False 1-bit register.

    else:
        record.value = (False, True)



    # We then look for entry properties to extend the entry record.

    parameter_i = 0

    while parameter_i < len(parameters):

        match parameter := parameters[parameter_i]:



            # Entry properties are typically used for filling in placeholders in the entry tag.
            # e.g. (pll{UNIT}_ready (RCC CR PLL3RDY) (UNIT : 3))

            case (property_name, ':', property_value):
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
            record.SECTION  = section
            record.REGISTER = register
            record.FIELD    = field



        # Leftover entry parameters.

        case _:
            raise ValueError(f'Leftover parameters: {repr(parameters)}.')



    # Finished parsing for the tag and record from the S-expression of the entry.

    return tag, types.SimpleNamespace(**record.__dict__)



# Helper routine to make sure all placeholders in a tag are supplied.
# e.g:
# >
# >    verify_and_get_placeholders('pll{UNIT}_input_range', UNIT : 3)
# >                                     ^^^^----------------^^^^^^^^
# >
#
# The returned placeholder names will be given in the order they appear in the tag.
# e.g:
# >
# >    verify_and_get_placeholders('pll{UNIT}{CHANNEL}_enable', CHANNEL : 'q', UNIT : 3)   ->   { 'UNIT', 'CHANNEL' }
# >                                    ^^^^^^^^^^^^^^^--------------------------------------------^^^^^^^^^^^^^^^^^
# >

def verify_and_get_placeholders_in_tag_order(tag, **placeholders):

    names = OrderedSet(re.findall('{(.*?)}', tag))

    if differences := names - placeholders.keys():
        raise ValueError(f'Tag "{tag}" is missing the value for the placeholder "{differences[0]}".')

    if differences := OrderedSet(placeholders.keys()) - names:
        raise ValueError(f'Tag "{tag}" has no placeholder "{differences[0]}".')

    return names



# The database for a given target will just be a dictionary with a specialized query method.

class SystemDatabaseTarget(dict):

    def query(self, tag, **placeholder_values):

        placeholder_names = verify_and_get_placeholders_in_tag_order(tag, **placeholder_values)


        # Find the database entries and determine which entry we should return
        # based on the placeholder values.
        # Note that the order of the placeholder values is important.
        # e.g:
        # >
        # >    query('pll{UNIT}{CHANNEL}_enable', UNIT : 2, CHANNEL : 'q')
        # >                                       ~~~~~~~~~~~~~~~~~~~~~~~
        # >                                                          |
        # >                                                          v
        # >                                                      ~~~~~~~~
        # >    SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(2, 'q')]    <- Expected.
        # >
        # >
        # >    query('pll{UNIT}{CHANNEL}_enable', CHANNEL : 'q', UNIT : 2)
        # >                                       ~~~~~~~~~~~~~~~~~~~~~~~
        # >                                                          |
        # >                                                          v
        # >                                                      ~~~~~~~~
        # >    SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][('q', 2)]    <- Uh oh!
        # >

        if tag not in self:
            raise ValueError(f'No entry has tag: {repr(tag)}.')

        key = tuple(placeholder_values[name] for name in placeholder_names)



        # No placeholders, so we just do a direct look-up.
        # e.g:
        # >
        # >    query('cpu_divider')   ->   SYSTEM_DATABASE[mcu]['cpu_divider']
        # >

        if len(key) == 0:
            return self[tag]



        # Single placeholder, so we get the entry based on the solely provided keyword-argument.
        # e.g:
        # >
        # >    query('pll{UNIT}_predivider', UNIT : 2)   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
        # >

        if len(key) == 1:
            key, = key



        # Multiple placeholders, so we get the entry based on the provided keyword-arguments in tag-order.
        # e.g:
        # >
        # >    query('pll{UNIT}{CHANNEL}_enable', UNIT : 2, CHANNEL : 'q')   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(2, 'q')]
        # >

        if key not in self[tag]:
            raise ValueError(f'Placeholders {', '.join(
                f'({name} = {repr(value)})'
                for name, value in placeholder_values.items()
            )} is not an option for database entry {repr(tag)}.')

        return self[tag][key]



# Create a database for each MCU in use.

SYSTEM_DATABASE = {}

for mcu in MCUS:

    SYSTEM_DATABASE[mcu] = SystemDatabaseTarget()



    # The database for the MCU is expressed as an S-expression in a separate text file that we then parse.

    database_file_path = root(f'./deps/mcu/{mcu}_database.txt')

    if not database_file_path.is_file():
        raise RuntimeError(
            f'File "{database_file_path}" does not exist; '
            f'a file of S-expressions is needed to describe the properties of the MCU.'
        )

    for tag, records in coalesce(map(parse_entry, parse_sexp(database_file_path.read_text()))):



        # Determine the placeholders in the entry's tag.
        # e.g:
        # >
        # >    'pll{UNIT}{CHANNEL}_enable'   ->   { 'UNIT', 'CHANNEL' }
        # >

        ordered_placeholders = OrderedSet(re.findall('{(.*?)}', tag))

        if not ordered_placeholders and len(records) >= 2:
            raise ValueError(
                f'For {mcu}, multiple database entries were found with the tag "{tag}"; '
                f'there should be a {{...}} in the string.'
            )



        # Based on the placeholder count, we grouped together all of the database entries that have the same tag.

        match list(ordered_placeholders):



            # No placeholder in the tag, so the user should expect a single entry when querying.
            # e.g:
            # >
            # >    (iwdg_stopped_during_debug (DBGMCU APB4FZR DBG_IWDG))   ->   SYSTEM_DATABASE[mcu]['iwdg_stopped_during_debug']
            # >

            case []:
                SYSTEM_DATABASE[mcu][tag], = records



            # Single placeholder in the tag.
            # e.g:
            # >
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM1) (minmax: 1 63) (UNIT : 1))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][1]
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM2) (minmax: 1 63) (UNIT : 2))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM3) (minmax: 1 63) (UNIT : 3))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][3]
            # >

            case [placeholder]:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (record.__dict__[placeholder], record)
                    for record in records
                )



            # Multiple placeholders in the tag.
            # e.g:
            # >
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1PEN) (UNIT : 1) (CHANNEL : p))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'p')]
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1QEN) (UNIT : 1) (CHANNEL : q))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'q')]
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1SEN) (UNIT : 1) (CHANNEL : s))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 's')]
            # >

            case _:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (tuple(record.__dict__[placeholder] for placeholder in ordered_placeholders), record)
                    for record in records
                )



################################################################################################################################



# This meta-directive creates what I call the "database" for a microcontroller.
# The so-called database simply contains information from the MCU's reference manual and datasheet.
# For instance, things like:
#
#     - the maximum bus frequency,
#     - the available clock sources for a particular PLL unit,
#     - the range of predividers for making the baud of a UART peripheral,
#     - the register to enable an internal oscillator,
#     - etc.
#
# The database is extremely useful for porting across different microcontrollers
# because, first, obviously the specifications values will be different,
# but more importantly, the naming convention is different.
#
# One reference manual might use the field name "PLL3ON" to enable PLL3,
# while a different RM might use the field name "PLL3EN" instead.
# To account for this, the database organizes everything by "tags",
# so rather than use names like "PLL3ON" or "PLL3EN", we instead use the
# more human readable key of "pll_{UNIT}_enable".
# The placeholder "{UNIT}" would be eventually replaced with 3 later on,
# so this makes using the tag "pll_{UNIT}_enable" very natural because we often
# iterate over the other PLL units too.
#
# This layer of abstraction makes it very easy to read/modify registers
# across multiple different microcontroller models without getting bogged
# down by the different values and naming conventions.
#
# The way the database is defined is in "deps/mcu/<MCU>_database.txt" which
# is just a S-expression file. I'm not going to go into detail how the syntax
# exactly works here since I think it's pretty self-explanatory.
# If you're curious just read the Python code above and play with it!
