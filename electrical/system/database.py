#meta SYSTEM_DATABASE, verify_and_get_field_names_in_tag_order

import re
from deps.pxd.sexp import parse_sexp



################################################################################



# Convert a database entry in S-expression form into a more usable Python value.
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

def parse_entry(sexp):



    # Get the tag and attributes.
    # e.g:
    # >
    # >    (pll{UNIT}_predivider (RCC PLL2CFGR PLL2M) (UNIT : 2) (.minmax 1 63))
    # >     ^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    # >     Tag.                 Attributes.
    # >

    tag, *attributes = sexp
    entry            = AllocatingNamespace()



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
                entry.value = value



            # The entry value is an inclusive range.
            # e.g:
            # >
            # >    (flash_latency (FLASH ACR LATENCY) (.minmax 0b0000 0b1111))
            # >                                       ^^^^^^^^^^^^^^^^^^^^^^^
            # >

            case ('.minmax', minimum, maximum):
                entry.minimum = minimum
                entry.maximum = maximum



            # This entry attribute is something else which we'll process later.

            case _: continue



        # We found and processed the entry value; no need to process it again.

        del attributes[attribute_i]
        break



    # No dotted entry value found, so we assume it's just
    # something like a True/False 1-bit register.
    # e.g:
    # >
    # >    (current_active_vos_ready (PWR SR1 ACTVOSRDY))
    # >

    else:
        entry.value = (False, True)



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
                entry[attribute_name] = attribute_value

            case _:
                attribute_i += 1
                continue

        del attributes[attribute_i]



    # Process the remaining attributes.

    match attributes:



        # Nothing else to do.

        case []:
            pass



        # The entry is also provided with a section-register-field location.
        # e.g:
        # >
        # >    (current_active_vos_ready (PWR SR1 ACTVOSRDY))
        # >                              ^^^^^^^^^^^^^^^^^^^
        # >

        case [(section, register, field)]:
            entry.section  = section
            entry.register = register
            entry.field    = field



        # Leftover attributes.

        case _:
            raise ValueError(f'Leftover attributes: {repr(attributes)}.')



    # Finished parsing the database entry!

    return tag, types.SimpleNamespace(**entry.__dict__)



################################################################################



# Helper routine to make sure all fields in a tag are supplied.
# e.g:
# >
# >    ('pll{UNIT}{CHANNEL}_enable', { 'CHANNEL' : 'q', 'UNIT' : 3 })
# >         ^^^^^^^^^^^^^^^----------^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# >

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



# TODO.

def query(self, tag, fields = None):



    # Use the given tag and field values to index into the database.

    if fields is None:
        fields = {}

    field_names = verify_and_get_field_names_in_tag_order(tag, fields)

    if tag not in self:
        raise ValueError(f'No entry has tag {repr(tag)}.')

    key = tuple(fields[name] for name in field_names)



    # We just do a direct look-up for tags with no fields.
    # e.g:
    # >
    # >                   query('cpu_divider')
    # >                         ^^^^^^^^^^^^^
    # >                               |
    # >                         vvvvvvvvvvvvv
    # >    SYSTEM_DATABASE[mcu]['cpu_divider']
    # >

    if len(key) == 0:
        return self[tag]



    # For tags with a single field, we use the only field value there is.
    # e.g:
    # >
    # >        query('pll{UNIT}_predivider', { 'UNIT' : 2 })
    # >              ^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^
    # >                             |                   |
    # >                         vvvvvvvvvvvvvvvvvvvvvv  |
    # >    SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][2]
    # >

    if len(key) == 1:
        key, = key



    # For tags with multiple fields, we use a tuple of the field
    # values in the order the field names appear in the tag.
    # e.g:
    # >
    # >    query('pll{UNIT}{CHANNEL}_enable', { 'CHANNEL' : 'q', 'UNIT' : 2 })
    # >          ^^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    # >                               |                          |
    # >                         vvvvvvvvvvvvvvvvvvvvvvvvvvv  vvvvvvvv
    # >    SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(2, 'q')]
    # >                                                      ^^^^^^^^
    # >                                                          |
    # >              We will sort the field values to be in tag-order.
    # >

    if key not in self[tag]:
        raise ValueError(f'Fields {', '.join(
            f'({name} = {repr(value)})'
            for name, value in fields.items()
        )} is not an option for database entry {repr(tag)}.')



    return self[tag][key]



################################################################################



SYSTEM_DATABASE = {}

for mcu in MCUS:

    SYSTEM_DATABASE[mcu] = {}



    # The MCU database is defined as an S-expression
    # in a separate text file that we parse.

    database_file_path = root(f'./deps/mcu/{mcu}_database.txt')

    if not database_file_path.is_file():
        raise RuntimeError(
            f'File "{database_file_path}" does not exist; '
            f'a file of S-expressions is needed to describe '
            f'the properties of the MCU.'
        )

    for tag, entries in coalesce(
        map(parse_entry, parse_sexp(database_file_path.read_text()))
    ):



        # Determine the fields in the entry's tag.
        # e.g:
        # >
        # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1PEN) (UNIT : 1) (CHANNEL : p))
        # >         ^^^^  ^^^^^^^
        # >

        field_names = OrderedSet(re.findall('{(.*?)}', tag))

        if not field_names and len(entries) >= 2:
            raise ValueError(
                f'For {repr(mcu)}, '
                f'multiple database entries were found '
                f'with the tag {repr(tag)}; '
                f'there should be a {{...}} in the tag.'
            )



        # We figure out how to group all of the
        # database entries with the same tag together.

        match list(field_names):



            # A field-less tag should be enough to
            # uniquely determine the database entry.
            # e.g:
            # >
            # >    (iwdg_stopped_during_debug (DBGMCU APB4FZR DBG_IWDG))
            # >     ~~~~~~~~~~~~~~~~~~~~~~~~~
            # >                            |
            # >                         ~~~~~~~~~~~~~~~~~~~~~~~~~~~
            # >    SYSTEM_DATABASE[mcu]['iwdg_stopped_during_debug']
            # >

            case []:
                SYSTEM_DATABASE[mcu][tag], = entries



            # A tag with a single field means we'll need a single field-value.
            # e.g:
            # >
            # >    (pll{UNIT}_predivider (RCC PLLCKSELR DIVM1) (minmax: 1 63) (UNIT : 1))
            # >     ~~~~~~~~~~~~~~~~~~~~                                      ~~~~~~~~~~
            # >              |                                                     |
            # >              |--------------------|             |------------------|
            # >                                   |             |
            # >                         ~~~~~~~~~~~~~~~~~~~~~~  ~
            # >    SYSTEM_DATABASE[mcu]['pll{UNIT}_predivider'][1]
            # >

            case [field_name]:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (entry.__dict__[field_name], entry)
                    for entry in entries
                )



            # A tag with multiple fields means we'll need a tuple of field-values.
            # e.g:
            # >
            # >    (pll{UNIT}{CHANNEL}_enable (RCC PLLCFGR PLL1PEN) (UNIT : 1) (CHANNEL : p))
            # >     ~~~~~~~~~~~~~~~~~~~~~~~~~                       ~~~~~~~~~~~~~~~~~~~~~~~~
            # >                           |                             |
            # >                         ~~~~~~~~~~~~~~~~~~~~~~~~~~~  ~~~~~~~~
            # >    SYSTEM_DATABASE[mcu]['pll{UNIT}{CHANNEL}_enable'][(1, 'p')]
            # >

            case _:
                SYSTEM_DATABASE[mcu][tag] = mk_dict(
                    (
                        tuple(entry.__dict__[name] for name in field_names),
                        entry
                    )
                    for entry in entries
                )



        # TODO.

        for entry in entries:
            fields                             = { name : entry.__dict__[name] for name in field_names }
            expanded_tag                       = tag.format(**fields)
            SYSTEM_DATABASE[mcu][expanded_tag] = query(SYSTEM_DATABASE[mcu], tag, fields)



################################################################################



# This meta-directive creates what I call the "database" for a microcontroller.
# The so-called database simply contains information from the MCU's reference
# manual and datasheet.
#
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
