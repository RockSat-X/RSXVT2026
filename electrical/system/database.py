#meta SYSTEM_DATABASE, verify_and_get_fields_in_tag_order

import re
from deps.pxd.sexp import parse_sexp



# Routine for converting S-expressions of database entries into a more usable Python value.
# e.g:
# >
# >    (pll{UNIT}_predivider (RCC PLL2CFGR PLL2M) (UNIT : 2) (.minmax 1 63))
# >     ^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# >     Tag.                 types.SimpleNamespace(
# >                              section  = 'RCC',
# >                              register = 'PLL2CFGR',
# >                              field    = 'PLL2M',
# >                              UNIT     = 2,
# >                              min      = 1,
# >                              max      = 63,
# >                          )
# >

def parse_entry(entry):



    # Get the tag and attributes.
    # e.g:
    # >
    # >    (pll{UNIT}_predivider (RCC PLL2CFGR PLL2M) (UNIT : 2) (.minmax 1 63))
    # >     ^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    # >     Tag.                 Attributes.
    # >

    tag, *attributes = entry
    record           = AllocatingNamespace()



    # Look for the dotted attribute which will represent the value of the entry.
    # e.g:
    # >
    # >    (pll{UNIT}_predivider (RCC PLL2CFGR PLL2M) (UNIT : 2) (.minmax 1 63))
    # >                                                          ^^^^^^^^^^^^^^
    # >

    for attribute_i, attribute in enumerate(attributes):

        match attribute:



            # The entry value is directly given.
            # e.g:
            # >
            # >    (APB_UNITS (.value (1 2 4 5)))
            # >               ^^^^^^^^^^^^^^^^^^
            # >

            case ('.value', value):
                record.value = value



            # The entry value is an inclusive range.
            # e.g:
            # >
            # >    (flash_latency (FLASH ACR LATENCY) (.minmax 0b0000 0b1111))
            # >                                       ^^^^^^^^^^^^^^^^^^^^^^^
            # >

            case ('.minmax', minimum, maximum):
                record.minimum = minimum
                record.maximum = maximum



            # This entry attribute is something else which we'll process later.

            case _: continue



        # We found and processed the entry value; no need to process it again.

        del attributes[attribute_i]
        break



    # No dotted entry value found, so we assume it's just something like a True/False 1-bit register.
    # e.g:
    # >
    # >    (current_active_vos_ready (PWR SR1 ACTVOSRDY))
    # >

    else:
        record.value = (False, True)



    # We then look for field-value attributes.
    # e.g:
    # >
    # >    (pll{UNIT}_ready (RCC CR PLL3RDY) (UNIT : 3))
    # >         ^^^^-------------------------^^^^^^^^^^
    # >

    attribute_i = 0

    while attribute_i < len(attributes):

        match attribute := attributes[attribute_i]:

            case (attribute_name, ':', attribute_value):
                record[attribute_name] = attribute_value

            case _:
                attribute_i += 1
                continue

        del attributes[attribute_i]



    # Process the remaining attributes.

    match attributes:



        # Nothing else to do.

        case []:
            pass



        # The entry is also provided with a location.
        # e.g:
        # >
        # >    (pll{UNIT}_predivider (RCC PLL2CFGR PLL2M) (UNIT : 2) (.minmax 1 63))
        # >                          ^^^^^^^^^^^^^^^^^^^^
        # >

        case [(section, register, field)]:
            record.section  = section
            record.register = register
            record.field    = field



        # Leftover attributes.

        case _:
            raise ValueError(f'Leftover attributes: {repr(attributes)}.')



    # Finished parsing the database entry!

    return tag, types.SimpleNamespace(**record.__dict__)



# Helper routine to make sure all placeholders in a tag are supplied.
# e.g:
# >
# >    verify_and_get_fields_in_tag_order('pll{UNIT}_input_range', { 'UNIT' : 3 })
# >                                            ^^^^----------------^^^^^^^^^^^^^^
# >
#
# The returned field-names will be given in the order they appear in the tag.
# e.g:
# >
# >    verify_and_get_fields_in_tag_order('pll{UNIT}{CHANNEL}_enable', { 'CHANNEL' : 'q', 'UNIT' : 3 })   ->   { 'UNIT', 'CHANNEL' }
# >                                           ^^^^^^^^^^^^^^^----------------------------------------------------^^^^^^^^^^^^^^^^^
# >

def verify_and_get_fields_in_tag_order(tag, fields = {}):

    names = OrderedSet(re.findall('{(.*?)}', tag))

    if differences := names - fields.keys():
        raise ValueError(f'Tag "{tag}" is missing the value for the field "{differences[0]}".')

    if differences := OrderedSet(fields.keys()) - names:
        raise ValueError(f'Tag "{tag}" has no field "{differences[0]}".')

    return names



# The database for a given target will just be a dictionary with a specialized query method.

class SystemDatabaseTarget(dict):

    def query(self, tag, **field_values):

        field_names = verify_and_get_fields_in_tag_order(tag, field_values)


        # Find the database entries and determine which entry we should return based on the field values.
        # Note that the order of the fields is important.
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

        key = tuple(field_values[name] for name in field_names)



        # No fields, so we just do a direct look-up.
        # e.g:
        # >
        # >    query('cpu_divider')   ->   SYSTEM_DATABASE[mcu]['cpu_divider']
        # >

        if len(key) == 0:
            return self[tag]



        # Single field, so we get the entry based on the solely provided keyword-argument.
        # e.g:
        # >
        # >    query('pll{UNIT}_predivider', UNIT : 2)   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
        # >

        if len(key) == 1:
            key, = key



        # Multiple fields, so we get the entry based on the provided keyword-arguments in tag-order.
        # e.g:
        # >
        # >    query('pll{UNIT}{CHANNEL}_enable', UNIT : 2, CHANNEL : 'q')   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(2, 'q')]
        # >

        if key not in self[tag]:
            raise ValueError(f'Fields {', '.join(
                f'({name} = {repr(value)})'
                for name, value in field_values.items()
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



        # Determine the fields in the entry's tag.
        # e.g:
        # >
        # >    'pll{UNIT}{CHANNEL}_enable'   ->   { 'UNIT', 'CHANNEL' }
        # >

        ordered_fields = OrderedSet(re.findall('{(.*?)}', tag))

        if not ordered_fields and len(records) >= 2:
            raise ValueError(
                f'For {mcu}, multiple database entries were found with the tag "{tag}"; '
                f'there should be a {{...}} in the string.'
            )



        # Based on the field count, we grouped together all of the database entries that have the same tag.

        match list(ordered_fields):



            # No field in the tag, so the user should expect a single entry when querying.
            # e.g:
            # >
            # >    (iwdg_stopped_during_debug (DBGMCU APB4FZR DBG_IWDG))   ->   SYSTEM_DATABASE[mcu]['iwdg_stopped_during_debug']
            # >

            case []:
                SYSTEM_DATABASE[mcu][tag], = records



            # Single field in the tag.
            # e.g:
            # >
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM1) (minmax: 1 63) (UNIT : 1))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][1]
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM2) (minmax: 1 63) (UNIT : 2))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM3) (minmax: 1 63) (UNIT : 3))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][3]
            # >

            case [field]:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (record.__dict__[field], record)
                    for record in records
                )



            # Multiple fields in the tag.
            # e.g:
            # >
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1PEN) (UNIT : 1) (CHANNEL : p))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'p')]
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1QEN) (UNIT : 1) (CHANNEL : q))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'q')]
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1SEN) (UNIT : 1) (CHANNEL : s))   ->   SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 's')]
            # >

            case _:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (tuple(record.__dict__[field] for field in ordered_fields), record)
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
# The field "{UNIT}" would be eventually replaced with 3 later on,
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
