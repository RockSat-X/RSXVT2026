////////////////////////////////////////////////////////////////////////////////



// Useful shorthand marcos.
#define stlink_tx(...) UXART_tx(UXARTHandle_stlink, __VA_ARGS__)
#define stlink_rx(...) UXART_rx(UXARTHandle_stlink, __VA_ARGS__)



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include <printf/printf.c>
#pragma GCC diagnostic pop



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
_UXART_tx_raw_nonreentrant(enum UXARTHandle handle, u8* data, i32 length)
{

    _EXPAND_HANDLE

    for (i32 i = 0; i < length; i += 1)
    {

        // Wait if the TX-FIFO is full.
        while (!CMSIS_GET(UXARTx, ISR, TXE));

        // This procedure is non-reentrant because a task
        // switch could occur right here; if that task
        // pushes data to TDR, then our current task might
        // resume and then also immediately push into TDR,
        // but TXE might be not set, so data will dropped.

        CMSIS_SET(UXARTx, TDR, TDR, data[i]);

    }

}



static void
_UXART_fctprintf_callback(char character, void* void_handle)
{
    _UXART_tx_raw_nonreentrant
    (
        (enum UXARTHandle) void_handle,
        &(u8) { character },
        1
    );
}



static void __attribute__((format(printf, 2, 3)))
UXART_tx(enum UXARTHandle handle, char* format, ...)
{

    _EXPAND_HANDLE

    va_list arguments = {0};
    va_start(arguments);

    MUTEX_TAKE(driver->transmission_mutex);
    {
        vfctprintf
        (
            &_UXART_fctprintf_callback,
            (void*) handle,
            format,
            arguments
        );
    }
    MUTEX_GIVE(driver->transmission_mutex);

    va_end(arguments);

}



// Note that the terminal on the other end could send '\r'
// to indicate that the user pressed <ENTER>.
// Furthermore, certain keys, such as arrow keys, will be
// sent as a multi-byte sequence.

static useret b32 // Received character?
UXART_rx(enum UXARTHandle handle, char* destination)
{

    _EXPAND_HANDLE

    b32 data_available = {0};

    MUTEX_TAKE(driver->reception_mutex);
    {
        data_available = driver->reception_reader != driver->reception_writer;

        if (data_available && destination)
        {
            static_assert(IS_POWER_OF_TWO(countof(driver->reception_buffer)));

            u32 reader_index =
                driver->reception_reader
                    & (countof(driver->reception_buffer) - 1);

            *destination = driver->reception_buffer[reader_index];

            driver->reception_reader += 1;
        }
    }
    MUTEX_GIVE(driver->reception_mutex);

    return data_available;

}



////////////////////////////////////////////////////////////////////////////////



static void
UXART_init(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(UXARTx_RESET, true );
    CMSIS_PUT(UXARTx_RESET, false);

    *driver = (struct UXARTDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(UXARTx);



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(UXARTx_KERNEL_SOURCE, UXARTx_KERNEL_SOURCE_init);



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



    // Initialize the driver.

    #if TARGET_USES_FREERTOS
        // TODO A quick Google search didn't determine whether or
        //      not it's okay to recreate a static semaphore in FreeRTOS
        //      without manually deleting it first. Until this is figured
        //      out, we will assume that the UXART driver cannot be
        //      reinitialized.
        driver->transmission_mutex = xSemaphoreCreateMutexStatic(&driver->transmission_mutex_data);
        driver->reception_mutex    = xSemaphoreCreateMutexStatic(&driver->reception_mutex_data   );
    #endif

}


////////////////////////////////////////////////////////////////////////////////


static void
_UXART_update(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // Handle reception errors.

    b32 reception_errors =
        UXARTx->ISR &
        (
            USART_ISR_FE  | // Frame error.
            USART_ISR_NE  | // Noise error.
            USART_ISR_PE  | // Parity error (put here for completeness).
            USART_ISR_ORE   // Overrun error.
        );

    if (reception_errors)
    {
        #if 0
            sorry // Reception error!
        #else
            CMSIS_SET
            (
                UXARTx, ICR , // We ignore the reception error.
                FECF  , true, // Frame error.
                NECF  , true, // Noise error.
                PECF  , true, // Parity error (put here for completeness).
                ORECF , true, // Overrun error.
            );
        #endif
    }



    // Handle received data.

    if (CMSIS_GET(UXARTx, ISR, RXNE)) // We got a byte of data?
    {

        // Pop from the hardware RX-buffer even if we don't have
        // a place to save the data in our software buffer right now.
        u8 data = UXARTx->RDR;

        u32 reader_index =
            driver->reception_reader
                + countof(driver->reception_buffer);

        b32 reception_buffer_full =
            (driver->reception_writer == reader_index);

        if (reception_buffer_full)
        {
            #if 0
                sorry // RX-buffer overflow!
            #else
                // We got an overflow,
                // but this isn't a critical thing,
                // so we'll just silently ignore it.
            #endif
        }
        else // Push received byte into the ring-buffer.
        {
            static_assert(IS_POWER_OF_TWO(countof(driver->reception_buffer)));

            u32 writer_index =
                driver->reception_writer
                    & (countof(driver->reception_buffer) - 1);

            driver->reception_buffer[writer_index]  = data;
            driver->reception_writer               += 1;
        }

    }

}



////////////////////////////////////////////////////////////////////////////////



#include "uxart_interrupts.meta"
/* #meta

    for target in PER_TARGET():

        for handle, instance in target.drivers.get('UXART', ()):

            Meta.line(f'''
                INTERRUPT_{instance}
                {{
                    _UXART_update(UXARTHandle_{handle});
                }}
            ''')

*/



////////////////////////////////////////////////////////////////////////////////



/* #include "uxart_aliases.meta"
/* #meta

    IMPLEMENT_DRIVER_ALIASES('UXART', (
        {
            'moniker'     :                  'UXARTx',
            'identifier'  : lambda instance: instance,
            'peripheral'  :                  'USART',
        },
        {
            'moniker'     :                  f'NVICInterrupt_UXARTx',
            'identifier'  : lambda instance: f'NVICInterrupt_{instance}',
        },
        {
            'moniker'     :                  f'UXARTx_KERNEL_SOURCE_init',
            'identifier'  : lambda instance: f'{instance}_KERNEL_SOURCE_init',
        },
        {
            'moniker'     :                  f'UXARTx_BRR_BRR_init',
            'identifier'  : lambda instance: f'{instance}_BRR_BRR_init',
        },
        {
            'moniker'     :                  f'UXARTx_RESET',
            'cmsis_tuple' : lambda instance: f'{instance}_RESET',
        },
        {
            'moniker'     :                  f'UXARTx_ENABLE',
            'cmsis_tuple' : lambda instance: f'{instance}_ENABLE',
        },
        {
            'moniker'     :                  f'UXARTx_KERNEL_SOURCE',
            'cmsis_tuple' : lambda instance: f'{instance}_KERNEL_SOURCE',
        },
    ))

*/
