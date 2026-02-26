#define stlink_tx(...)        UXART_tx_format(UXARTHandle_stlink, __VA_ARGS__)
#define stlink_rx(...)        UXART_rx       (UXARTHandle_stlink, __VA_ARGS__)
#define UXART_rx(HANDLE, DST) RingBuffer_pop(&_UXART_drivers[HANDLE].reception, (DST))

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
        expansions  = (('driver', '&_UXART_drivers[handle]'),),
        terms       = lambda target, type, peripheral, handle, mode: (
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
    RingBuffer(u8, 256) transmission;
    RingBuffer(u8, 256) reception;
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

    NVIC_DISABLE(UXARTx);

    memzero(driver);



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



static void
_UXART_push_byte_for_transmission(char byte, void* void_handle)
{

    enum UXARTHandle handle = (enum UXARTHandle) void_handle;

    _EXPAND_HANDLE



    // Most of the time, spin-locking will not be done,
    // but it will be done if the buffer is at its limit.

    for (i32 attempts = 0; attempts < 1'000'000; attempts += 1) // Arbitrary upper-limit. @/`UXART Error Handling`.
    {
        if (RingBuffer_push(&driver->transmission, (u8*) &byte))
        {
            break; // Successfully pushed into the transmission ring-buffer.
        }
        else
        {
            NVIC_SET_PENDING(UXARTx); // Make sure that the UXART knows it should be transmitting.
            FREERTOS_delay_ms(1);
        }
    }

}



static void
UXART_tx_bytes(enum UXARTHandle handle, u8* bytes, i32 length)
{

    _EXPAND_HANDLE

    if (bytes && length >= 1)
    {

        // Queue up all of the data to be transmitted.

        for (i32 i = 0; i < length; i += 1)
        {
            _UXART_push_byte_for_transmission(bytes[i], (void*) handle);
        }



        // Let UXART know it should be transmitting now,
        // if it isn't already aware of that.

        if (RingBuffer_reading_pointer(&driver->transmission))
        {
            NVIC_SET_PENDING(UXARTx);
        }

    }
    else
    {
        // No data to transmit. @/`UXART Error Handling`.
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

    if (RingBuffer_reading_pointer(&driver->transmission))
    {
        NVIC_SET_PENDING(UXARTx);
    }



    va_end(arguments);

}



static void
_UXART_driver_interrupt(enum UXARTHandle handle)
{

    _EXPAND_HANDLE



    // In half-duplex mode, we need to disable the receiver
    // side of the UXART peripheral or else the data the
    // transmitter sends will be interpreted as incoming data.

    b32 enable_receiver =
        implies
        (
            UXARTx_MODE == UXARTMode_half_duplex,
            CMSIS_GET(UXARTx, ISR, TC) && !RingBuffer_reading_pointer(&driver->transmission)
        );

    CMSIS_SET
    (
        UXARTx, CR1             ,
        RE    , enable_receiver , // Selectively disable the receiver during half-duplex transmissions.
        TCIE  , !enable_receiver, // Ignore transmission-complete when we're receiving.
    );



    // We fill the UXART's TX-FIFO until it's full;
    // once it becomes empty, the interrupt routine
    // will retrigger and from there we can replenish it.

    while (CMSIS_GET(UXARTx, ISR, TXE_TXFNF))
    {

        u8 data = {0};

        if (RingBuffer_pop(&driver->transmission, &data))
        {
            CMSIS_SET(UXARTx, TDR, TDR, data);
        }
        else
        {
            break; // No more data to send!
        }

    }

    b32 theres_still_stuff_to_transmit = !!RingBuffer_reading_pointer(&driver->transmission);



    // If there's still stuff left to transmit,
    // set the interrupt to trigger for whenever
    // the TX-FIFO is empty again.

    CMSIS_SET(UXARTx, CR1, TXFEIE, !!theres_still_stuff_to_transmit);



    // Handle any received data.

    while (true)
    {

        // We currently ignore any reception errors.

        CMSIS_SET
        (
            UXARTx, ICR ,
            FECF  , true, // Frame error.
            NECF  , true, // Noise error.
            PECF  , true, // Parity error (here for completeness).
            ORECF , true, // Overrun error.
        );



        // Handle any received data.

        if (CMSIS_GET(UXARTx, ISR, RXNE))
        {

            // Pop from the hardware RX-buffer even if we don't have a place to
            // save the data in our software buffer right now.

            u8 data = (u8) UXARTx->RDR;



            // Try pushing the data into the reception ring-buffer.

            if (!RingBuffer_push(&driver->reception, &data))
            {
                // The reception ring-buffer overflowed;
                // we'll just silently drop the data for now.
                // @/`UXART Error Handling`.

            }

        }
        else
        {
            break; // No more new data.
        }

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`UXART Error Handling`:
//
// This UXART driver is specifically designed to not report any particular
// error condition to the user. Things such as UART frame errors and FIFO
// overrun will not be flagged to the user at all. This is to make the
// driver easier to use, as errors concerning UART transmission is pretty
// unlikely, and any data transfer should be backed by a checksum anyways.
//
// The main concern that the user should be aware of when it comes to a bug
// error condition is when the UXART driver is being used it has been initialized;
// this user bug is pretty easy to identify, however, so it's not a big concern.
//
// Nonetheless, if need be, the UXART driver can be reinitialized at any time.
