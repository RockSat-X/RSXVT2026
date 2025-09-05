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



    // Set the kernel clock source for the I2C peripheral.

    // TODO.



    // Clock the peripheral.

    CMSIS_PUT(UXARTx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET(UXARTx, BRR, BRR, UXARTx_BRR_BRR_init); // Set baud rate.
    CMSIS_SET(UXARTx, CR3, EIE, true               ); // Trigger interrupt upon error.
    CMSIS_SET
    (
        UXARTx, CR1 ,
        RXNEIE, true, // Trigger interrupt when receiving a byte.
        TE    , true, // Enable transmitter.
        RE    , true, // Enable receiver.
        UE    , true, // Enable the peripheral.
    );

    #if TARGET_USES_FREERTOS
        driver->transmission_mutex = xSemaphoreCreateMutexStatic(&driver->transmission_mutex_data);
        driver->reception_mutex    = xSemaphoreCreateMutexStatic(&driver->reception_mutex_data   );
    #endif

}



////////////////////////////////////////////////////////////////////////////////



/* #include "uxart_aliases.meta"
/* #meta

    IMPLEMENT_DRIVER_ALIASES('UXART', (
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
            'moniker'     :                         f'UXARTx_BRR_BRR_init',
            'identifier'  : lambda peripheral_unit: f'{peripheral_unit[0]}{peripheral_unit[1]}_BRR_BRR_init',
        },
        {
            'moniker'     :                         f'UXARTx_RESET',
            'cmsis_tuple' : lambda peripheral_unit: f'{peripheral_unit[0]}{peripheral_unit[1]}_RESET',
        },
        {
            'moniker'     :                         f'UXARTx_ENABLE',
            'cmsis_tuple' : lambda peripheral_unit: f'UXART_{peripheral_unit[1]}_ENABLE',
        },
    ))

*/
