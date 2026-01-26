////////////////////////////////////////////////////////////////////////////////



// Useful shorthand marcos.
#define stlink_tx(...) UXART_tx_format(UXARTHandle_stlink, __VA_ARGS__)
#define stlink_rx(...) UXART_rx       (UXARTHandle_stlink, __VA_ARGS__)



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include <printf/printf.c>
#pragma GCC diagnostic pop



enum UXARTMode : u32
{
    UXARTMode_full_duplex,
    UXARTMode_half_duplex,
};



#include "uxart_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'UXART',
        cmsis_name  = 'USART',
        common_name = 'UXARTx',
        terms       = lambda type, peripheral, handle, mode: (
            ('{}'                   , 'expression' ,                    ),
            ('NVICInterrupt_{}'     , 'expression' ,                    ),
            ('STPY_{}_KERNEL_SOURCE', 'expression' ,                    ),
            ('STPY_{}_BAUD_DIVIDER' , 'expression' ,                    ),
            ('{}_RESET'             , 'cmsis_tuple',                    ),
            ('{}_ENABLE'            , 'cmsis_tuple',                    ),
            ('{}_KERNEL_SOURCE'     , 'cmsis_tuple',                    ),
            ('{}'                   , 'interrupt'  ,                    ),
            ('{}_MODE'              , 'expression' , f'UXARTMode_{mode}'),
        ),
    )

*/



struct UXARTDriver
{
    volatile u32 transmission_reader;
    volatile u32 transmission_writer;
    volatile u8  transmission_buffer[1 << 8]; // TODO Consider being able to be set.
    volatile u32 reception_reader;
    volatile u32 reception_writer;
    volatile u8  reception_buffer[1 << 8]; // TODO Consider being able to be set.
};

static_assert(IS_POWER_OF_TWO(countof(((struct UXARTDriver*) 0)->transmission_buffer)));
static_assert(IS_POWER_OF_TWO(countof(((struct UXARTDriver*) 0)->reception_buffer)));



static struct UXARTDriver _UXART_drivers[UXARTHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static void
UXART_init(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(UXARTx_RESET, true );
    CMSIS_PUT(UXARTx_RESET, false);

    *driver = (struct UXARTDriver) {0};



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(UXARTx_KERNEL_SOURCE, STPY_UXARTx_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(UXARTx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET(UXARTx, BRR, BRR, STPY_UXARTx_BAUD_DIVIDER); // Set baud rate.
    CMSIS_SET
    (
        UXARTx , CR3                                 ,
        EIE    , true                                , // Trigger interrupt upon error.
        HDSEL  , UXARTx_MODE == UXARTMode_half_duplex, // Whether or not to use half-duplex.
    );
    CMSIS_SET
    (
        UXARTx, CR1 ,
        RXNEIE, true, // Trigger interrupt when receiving a byte.
        FIFOEN, true, // Enable usage of RX-FIFO and TX-FIFO.
        TE    , true, // Enable transmitter.
        RE    , true, // Enable receiver.
        UE    , true, // Enable the peripheral.
    );



    // Enable the interrupts.

    NVIC_ENABLE(UXARTx);

}



static void // This weird type-signature is for the callback of `vfctprintf`.
_UXART_push_byte_for_transmission(char byte, void* void_handle)
{

    enum UXARTHandle handle = (enum UXARTHandle) void_handle;

    _EXPAND_HANDLE

    if (!CMSIS_GET(UXARTx, CR1, UE))
        panic;



    // Ensure there's enough space in the transmission ring-buffer.

    while (driver->transmission_writer - driver->transmission_reader == countof(driver->transmission_buffer))
    {
        NVIC_SET_PENDING(UXARTx); // Make sure that the UXART knows it should be transmitting.
    }



    // Push the data to be transmitted eventually.

    u32 writer_index = driver->transmission_writer % countof(driver->transmission_buffer);
    driver->transmission_buffer[writer_index]  = byte;
    driver->transmission_writer               += 1;

}



static void
UXART_tx_bytes(enum UXARTHandle handle, u8* bytes, i32 length)
{

    _EXPAND_HANDLE

    if (!bytes)
        panic;

    if (length < 0)
        panic;



    // Queue up all of the data to be transmitted.

    for (i32 i = 0; i < length; i += 1)
    {
        _UXART_push_byte_for_transmission(bytes[i], (void*) handle);
    }



    // Let UXART know it should be transmitting now,
    // if it isn't already aware of that.

    if (driver->transmission_reader != driver->transmission_writer)
    {
        NVIC_SET_PENDING(UXARTx);
    }

}



static void __attribute__((format(printf, 2, 3)))
UXART_tx_format(enum UXARTHandle handle, char* format, ...)
{

    _EXPAND_HANDLE

    va_list arguments = {0};
    va_start(arguments);



    // Format the string and queue up the characters for transmission.

    vfctprintf
    (
        &_UXART_push_byte_for_transmission,
        (void*) handle,
        format,
        arguments
    );



    // Let UXART know it should be transmitting now,
    // if it isn't already aware of that.

    if (driver->transmission_reader != driver->transmission_writer)
    {
        NVIC_SET_PENDING(UXARTx);
    }



    va_end(arguments);

}



// Note that the terminal on the other end could send '\r'
// to indicate that the user pressed <ENTER>.
// Furthermore, certain keys, such as arrow keys, will be
// sent as a multi-byte sequence.

static useret b32 // Received character?
UXART_rx(enum UXARTHandle handle, u8* destination)
{

    _EXPAND_HANDLE

    if (!CMSIS_GET(UXARTx, CR1, UE))
        panic;

    b32 data_available = driver->reception_reader != driver->reception_writer;

    if (data_available && destination)
    {
        i32 reader_index = driver->reception_reader % countof(driver->reception_buffer);
        *destination              = driver->reception_buffer[reader_index];
        driver->reception_reader += 1;
    }

    return data_available;

}



static void
_UXART_driver_interrupt(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // In half-duplex mode, we need to disable the receiver
    // side of the UXART peripheral or else the data the
    // transmitter sends will be interpreted as incoming data.

    if (UXARTx_MODE == UXARTMode_half_duplex && !CMSIS_GET(UXARTx, ISR, TC))
    {
        CMSIS_SET
        (
            UXARTx, CR1  ,
            RE    , false, // Receiver will ignore the transmitter's output.
            TCIE  , true , // We'll reenable the receiver once the transmitter is done.
        );
    }



    // We fill the UXART's TX-FIFO until it's full;
    // once it becomes empty, the interrupt routine
    // will retrigger and from there we can replenish it.

    while (driver->transmission_reader != driver->transmission_writer)
    {
        if (CMSIS_GET(UXARTx, ISR, TXE_TXFNF))
        {

            i32 reader_index = driver->transmission_reader % countof(driver->transmission_buffer);

            CMSIS_SET(UXARTx, TDR, TDR, driver->transmission_buffer[reader_index]);

            driver->transmission_reader += 1;

        }
        else
        {
            break; // TX-FIFO's full now; come back later when it's empty again.
        }
    }



    // If there's still stuff left to transmit,
    // set the interrupt to trigger for whenever
    // the TX-FIFO is empty again.

    CMSIS_SET(UXARTx, CR1, TXFEIE, driver->transmission_reader != driver->transmission_writer);



    // In half-duplex mode, the receiver will be enabled whenever the
    // transmitter is not doing stuff; otherwise, data we send out is
    // interpreted as incoming data.

    if (UXARTx_MODE == UXARTMode_half_duplex && CMSIS_GET(UXARTx, ISR, TC))
    {
        CMSIS_SET
        (
            UXARTx, CR1  ,
            RE    , true , // Receiver is now allowed to read for data.
            TCIE  , false, // No need for transmission-complete interrupts for now.
        );
    }



    // Handle any received data.

    while (true)
    {

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
                    PECF  , true, // Parity error (here for completeness).
                    ORECF , true, // Overrun error.
                );
            #endif
        }



        // Handle any received data.

        if (CMSIS_GET(UXARTx, ISR, RXNE))
        {

            // Pop from the hardware RX-buffer
            // even if we don't have a place to
            // save the data in our software
            // buffer right now.

            u8 data = (u8) UXARTx->RDR;



            // See if we can push the data into
            // the reception ring-buffer.

            if (driver->reception_writer - driver->reception_reader < countof(driver->reception_buffer))
            {
                u32 writer_index = driver->reception_writer % countof(driver->reception_buffer);
                driver->reception_buffer[writer_index]  = data;
                driver->reception_writer               += 1;
            }
            else
            {
                #if 0
                    sorry // RX-buffer overflow!
                #else
                    // We got an overflow,
                    // but this isn't a critical thing,
                    // so we'll just silently ignore it.
                #endif
            }

        }
        else
        {
            break;
        }

    }

}
