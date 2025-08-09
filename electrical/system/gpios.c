////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* #meta MK_GPIOS

    # The routine to create a single GPIO instance.

    def mk_gpio(entry):

        name, pin, mode, properties = entry



        # The layout of a GPIO instance.

        gpio = types.SimpleNamespace(
            name       = name,
            pin        = pin,
            mode       = mode,
            port       = None,
            number     = None,
            speed      = None,
            pull       = None,
            open_drain = None,
            initlvl    = None,
            altfunc    = None,
        )



        # If the pin of the GPIO is given, split it into its port and number parts (e.g. 'A10' -> ('A', 10)).
        # The pin can be left unspecified as `None`, which is useful for when we don't know where
        # the pin will be end up being at but we want to at least have it still be in the table.

        if pin is not None:
            gpio.port   = pin[0]
            gpio.number = int(pin[1:])



        # Handle the GPIO mode.

        match mode:



            # A simple input GPIO to read digital voltage levels.

            case 'input':

                # The pull-direction must be specified in order to prevent accidentally defining a floating GPIO pin.
                gpio.pull = properties.pop('pull')



            # A simple output GPIO that can be driven low or high.

            case 'output':

                # The initial voltage level must be specified so the user remembers to take it into consideration.
                gpio.initlvl = properties.pop('initlvl')

                # Other optional properties.
                gpio.speed      = properties.pop('speed'     , None)
                gpio.open_drain = properties.pop('open_drain', None)




            # This GPIO would typically be used for some peripheral functionality (e.g. SPI clock output).

            case 'alternate':

                # Alternate GPIOs are denoted by a string like "UART8_TX" to indicate its alternate function.
                gpio.altfunc = properties.pop('altfunc')

                # Other optional properties.
                gpio.speed      = properties.pop('speed'     , None)
                gpio.pull       = properties.pop('pull'      , None)
                gpio.open_drain = properties.pop('open_drain', None)



            # An analog GPIO would have its Schmit trigger function disabled;
            # this obviously allows for ADC/DAC usage, but it can also serve as a power-saving measure.

            case 'analog':
                assert False # TODO



            # A GPIO that's marked as "reserved" is often useful for marking a particular pin as something
            # that shouldn't be used because it has an important functionality (e.g. JTAG debug).

            case 'reserved':
                properties = {} # Don't care what properties this reserved GPIO has.



            # Unknown GPIO mode.

            case _:
                assert False



        # Done processing this GPIO entry!

        if properties:
            assert False, f'GPIO {name} has leftover properties: {properties}.'

        return gpio



    # The routine to define the table of GPIOs for every target.

    def MK_GPIOS(target_entries):

        table = {
            target_name : tuple(map(mk_gpio, entries))
            for target_name, entries in target_entries.items()
        }

        for gpios in table.values():

            if (name := find_dupe(gpio.name for gpio in gpios)) is not ...:
                assert False, f'GPIO name `{name}` used more than once.'

            if (pin := find_dupe(gpio.pin for gpio in gpios if gpio.pin is not None)) is not ...:
                assert False, f'Pin `{gpio.pin}` used more than once.'

        return table

*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GPIO_init.meta"
/* #meta

    import csv



    # Macros to make using GPIOs in C easy.
    # TODO Use SYTEM_DATABASE.

    Meta.define('GPIO_HIGH'  , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BS, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_LOW'   , ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->BSRR = CONCAT(GPIO_BSRR_BR, _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_TOGGLE', ('NAME'         ), '((void) (CONCAT(GPIO, _PORT_FOR_GPIO_WRITE(NAME))->ODR ^= CONCAT(GPIO_ODR_OD , _NUMBER_FOR_GPIO_WRITE(NAME))))')
    Meta.define('GPIO_SET'   , ('NAME', 'VALUE'), '((void) ((VALUE) ? GPIO_HIGH(NAME) : GPIO_LOW(NAME)))')
    Meta.define('GPIO_READ'  , ('NAME'         ), '(!!(CONCAT(GPIO, _PORT_FOR_GPIO_READ(NAME))->IDR & CONCAT(GPIO_IDR_ID, _NUMBER_FOR_GPIO_READ(NAME))))')



    # We create a table to map GPIO pin and alternate function names to alternate function codes.
    # e.g.
    # ('A', 0, 'UART4_TX') -> 0b1000

    GPIO_AFSEL = {}

    for mcu in TARGETS.mcus:

        GPIO_AFSEL[mcu] = []



        # Find the CSV file that STM32CubeMX can provide to get all of the alternate functions for GPIOs.

        gpio_afsel_file_path = root(f'./deps/mcu/{mcu}_gpios.csv')

        if not gpio_afsel_file_path.is_file():
            assert False, f'File `{gpio_afsel_file_path}` does not exist; use STM32CubeMX to generate the CSV file (clear pinout!).'



        # Process the CSV file.

        for entry in csv.DictReader(gpio_afsel_file_path.read_text().splitlines()):

            match entry['Type']:



                # Most GPIOs we care about are the I/O ones.

                case 'I/O':



                    # Some GPIO names are suffixed with additional things, like for instance:
                    #     PC14-OSC32_IN(OSC32_IN)
                    #     PH1-OSC_OUT(PH1)
                    #     PA14(JTCK/SWCLK)
                    #     PC2_C
                    # So we need to format it slightly so that we just get the port letter and pin number.

                    pin    = entry['Name']
                    pin    = pin.split('-', 1)[0]
                    pin    = pin.split('(', 1)[0]
                    pin    = pin.split('_', 1)[0]
                    port   = pin[1]
                    number = int(pin[2:])

                    if not pin.startswith('P') or not ('A' <= port <= 'Z'):
                        assert False



                    # Gather all the alternate functions of the GPIO, if any.

                    for afsel_code in range(16):

                        if not entry[f'AF{afsel_code}']: # Skip empty cells.
                            continue

                        GPIO_AFSEL[mcu] += [
                            ((port, number, afsel_name), afsel_code)
                            for afsel_name in entry[f'AF{afsel_code}'].split('/') # Some have multiple names for the same AFSEL code (e.g. "I2S3_CK/SPI3_SCK").
                        ]




                # TODO Maybe use this information to ensure the PCB footprint is correct?

                case 'Power' | 'Reset' | 'Boot':
                    pass



                # TODO I have no idea what this is.

                case 'MonoIO':
                    pass



                # Unknown GPIO type in the CSV; doesn't neccessarily mean an error though.

                case _:
                    pass # TODO Warn.



        # Done processing the CSV file; now we have a mapping of GPIO pin
        # and alternate function name to the alternate function code.

        GPIO_AFSEL[mcu] = mk_dict(GPIO_AFSEL[mcu])



    # Generate code to initialize and use the GPIOs in C.
    # TODO Use SYTEM_DATABASE.

    @Meta.ifs(GPIOS, style = '#if')
    def _(target_name):

        target_name = target_name
        gpios       = GPIOS[target_name]
        target      = TARGETS.get(target_name)

        yield f'TARGET_NAME_IS_{target_name}'



        # Macros to make GPIOs easy to use.

        for gpio in gpios:



            # Skip GPIOs with no defined pin yet.

            if gpio.pin is None:
                continue



            # Macros to allow reading from applicable GPIO pins.

            if gpio.mode in ('input', 'alternate'):
                Meta.define('_PORT_FOR_GPIO_READ'  , ('NAME'), gpio.port  , NAME = gpio.name)
                Meta.define('_NUMBER_FOR_GPIO_READ', ('NAME'), gpio.number, NAME = gpio.name)



            # Macros to control output GPIO pins.

            if gpio.mode == 'output':
                Meta.define('_PORT_FOR_GPIO_WRITE'  , ('NAME'), gpio.port  , NAME = gpio.name)
                Meta.define('_NUMBER_FOR_GPIO_WRITE', ('NAME'), gpio.number, NAME = gpio.name)



        # Initialization procedure for GPIOs.

        with Meta.enter('static void\nGPIO_init(void)'):



            # Enable GPIO ports that have defined pins.

            CMSIS_SET(
                ('RCC', SYSTEM_DATABASE[target.mcu]['GPIO_PORT_ENABLE_REGISTER'].VALUE, f'GPIO{port}EN', True)
                for port in sorted(OrderedSet(gpio.port for gpio in gpios if gpio.pin is not None))
            )



            # Set output type (push-pull/open-drain).

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'OTYPER', f'OT{gpio.number}', gpio.open_drain)
                for gpio in gpios
                if gpio.pin is not None and gpio.open_drain is not None
            )



            # Set initial output level.

            CMSIS_SET(
                (f'GPIO{gpio.port}', 'ODR', f'OD{gpio.number}', gpio.initlvl)
                for gpio in gpios
                if gpio.pin is not None and gpio.initlvl is not None
            )



            # Set GPIO drive strength.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'OSPEEDR',
                    f'OSPEED{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_SPEED'].VALUE)[gpio.speed]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.speed is not None
            )



            # Set pull up/pull down/float configuration.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'PUPDR',
                    f'PUPD{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_PULL'].VALUE)[gpio.pull]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.pull is not None
            )



            # Ensure the alternate function is defined in the lookup table.

            for gpio in gpios:

                if gpio.pin is None or gpio.altfunc is None:
                    continue # Not applicable.

                if (gpio.port, gpio.number, gpio.altfunc) not in GPIO_AFSEL[target.mcu]:
                    assert False, f"Pin {gpio.pin} on {target.mcu} ({target_name}) doesn't have alternate function `{gpio.altfunc}`."



            # Set alternative function; must be done before setting pin type
            # so that the alternate function pin will start off properly.

            CMSIS_WRITE(
                (
                    f'GPIO_AFR{('L', 'H')[gpio.number // 8]}',
                    f'GPIO{gpio.port}->AFR[{gpio.number // 8}]',
                    f'AFSEL{gpio.number}',
                    GPIO_AFSEL[target.mcu][(gpio.port, gpio.number, gpio.altfunc)]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.altfunc is not None
            )



            # Set GPIO pin mode.

            CMSIS_SET(
                (
                    f'GPIO{gpio.port}',
                    'MODER',
                    f'MODE{gpio.number}',
                    mk_dict(SYSTEM_DATABASE[target.mcu]['GPIO_MODE'].VALUE)[gpio.mode]
                )
                for gpio in gpios
                if gpio.pin is not None and gpio.mode not in (None, 'reserved')
            )

*/
