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

    CMSIS_PUT(UXARTx_RESET, true );
    CMSIS_PUT(UXARTx_RESET, false);

    *driver = (struct UXARTDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(UXARTx);

}



////////////////////////////////////////////////////////////////////////////////



/* #include "uxart_aliases.meta"
/* #meta

    IMPLEMENT_DRIVERS('UXART', (
        {
            'moniker'     :                         f'UXARTx',
            'identifier'  : lambda peripheral_unit: f'{peripheral_unit[0]}{peripheral_unit[1]}',
            'peripheral'  :                         f'USART',
        },
        {
            'moniker'     :                         f'NVICInterrupt_UXARTx',
            'identifier'  : lambda peripheral_unit: f'NVICInterrupt_{peripheral_unit[0]}{peripheral_unit[1]}',
        },
        {
            'moniker'     :                         f'UXARTx_RESET',
            'cmsis_tuple' : lambda peripheral_unit: f'{peripheral_unit[0]}{peripheral_unit[1]}_RESET',
        },
    ))

*/
