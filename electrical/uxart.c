////////////////////////////////////////////////////////////////////////////////



#include "uxart_aliases.meta"



struct UXARTDriver
{
    volatile u32  reception_reader;
    volatile u32  reception_writer;
    volatile char reception_buffer[1 << 5];

    #if TARGET_USES_FREERTOS
        StaticSemaphore_t transmission_mutex_data;
        SemaphoreHandle_t transmission_mutex;
        StaticSemaphore_t reception_mutex_data;
        SemaphoreHandle_t reception_mutex;
    #endif
};



static struct UXARTDriver _UXART_drivers[UXARTHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static void
UXART_reinit(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(USARTx_RESET, true );
    CMSIS_PUT(USARTx_RESET, false);

    *driver = (struct UXARTDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(USARTx);

}



////////////////////////////////////////////////////////////////////////////////



// Stuff to make working with any UXART unit easy.

/* #include "uxart_aliases.meta"
/* #meta



    # Things to be aliased for UXART.

    IDENTIFIERS = (
        'USART{}',
        'NVICInterrupt_USART{}',
    )

    CMSIS_TUPLE_TAGS = (
        'USART{}_RESET',
    )



    # Some target-specific support definitions.

    for target in PER_TARGET():

        if 'uxart_units' not in target.__dict__ or not target.uxart_units:

            Meta.line(
                f'#error Target {target.name} cannot use the UXART driver '
                f'because no UXART unit have been assigned.'
            )

            continue



        # Have the user be able to specify a specific UXART unit.

        Meta.enums('UXARTHandle', 'u32', (f'{peripheral}{unit}' for peripheral, unit in target.uxart_units))



        # A look-up table to allow generic code to be written for any UXART peripheral.

        Meta.lut('UXART_TABLE', (
            (
                f'UXARTHandle_{peripheral}{unit}',
                *[
                    (
                        identifier.format('x'),
                        identifier.format(unit)
                    )
                    for identifier in IDENTIFIERS
                ],
                *[
                    (
                        tag.format('x'),
                        CMSIS_TUPLE(SYSTEM_DATABASE[target.mcu][tag.format(unit)])
                    ) for tag in CMSIS_TUPLE_TAGS
                ]
            )
            for peripheral, unit in target.uxart_units
        ))



    # Macro to mostly bring stuff in the look-up table into the local scope.

    Meta.line(f'#define UXARTx_ UXART_')
    Meta.line('#undef _EXPAND_HANDLE')

    with Meta.enter('#define _EXPAND_HANDLE'):

        Meta.line(f'''

            if (!(0 <= handle && handle < UXARTHandle_COUNT))
            {{
                panic;
            }}

            struct UXARTDriver* const driver = &_UXART_drivers[handle];

        ''')

        for identifier in IDENTIFIERS + CMSIS_TUPLE_TAGS:
            Meta.line(f'''
                auto const {identifier.format('x')} = UXART_TABLE[handle].{identifier.format('x')};
            ''')

*/
