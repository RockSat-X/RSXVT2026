////////////////////////////////////////////////////////////////////////////////



// Useful shorthand marcos.
#define stlink_tx(...)        UXART_tx_format(UXARTHandle_stlink, __VA_ARGS__)
#define stlink_rx(...)        UXART_rx  (UXARTHandle_stlink, __VA_ARGS__)
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
    RingBuffer(u8, 256) transmission;
    RingBuffer(u8, 256) reception;
};



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


    // Push into the transmission ring-buffer.
    // Most of the time, spin-locking will not be done,
    // but it will be done if the buffer is at its limit.

    while (!RingBuffer_push(&driver->transmission, (u8*) &byte))
    {
        NVIC_SET_PENDING(UXARTx); // Make sure that the UXART knows it should be transmitting.
    }

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

    if (RingBuffer_reading_pointer(&driver->transmission))
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
        TCIE  , !enable_receiver, // The receiver once the transmitter is done.
    );

    b32 theres_still_stuff_to_transmit = false;



    // If the UXART handle is used for logging, we see
    // if there's any log entries that should be handled.

    #if TARGET_USES_LOG
    {
        if (handle == LOG_UXART_HANDLE)
        {

            LogEntry* log_entry = RingBuffer_reading_pointer(&_LOG_driver.entries);

            while (true)
            {
                if (!log_entry)
                {
                    break; // No log to transmit right now.
                }
                else if (!CMSIS_GET(UXARTx, ISR, TXE_TXFNF))
                {

                    theres_still_stuff_to_transmit = true;

                    break; // No space in the TX-FIFO.

                }
                else
                {

                    u8 data = (*log_entry)[_LOG_driver.bytes_of_current_entry_transmitted_so_far];

                    if (data == '\0') // Move onto next log?
                    {

                        if (!RingBuffer_pop(&_LOG_driver.entries, nullptr))
                            sorry

                        log_entry = RingBuffer_reading_pointer(&_LOG_driver.entries);

                        _LOG_driver.bytes_of_current_entry_transmitted_so_far = 0;

                    }
                    else // Push next byte of log message into TX-FIFO.
                    {
                        _LOG_driver.bytes_of_current_entry_transmitted_so_far += 1;
                        CMSIS_SET(UXARTx, TDR, TDR, data);
                    }

                }
            }

        }
    }
    #endif



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

    theres_still_stuff_to_transmit |= !!RingBuffer_reading_pointer(&driver->transmission);



    // If there's still stuff left to transmit,
    // set the interrupt to trigger for whenever
    // the TX-FIFO is empty again.

    CMSIS_SET(UXARTx, CR1, TXFEIE, !!theres_still_stuff_to_transmit);



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



            // Try pushing the data into
            // the reception ring-buffer.

            if (!RingBuffer_push(&driver->reception, &data))
            {
                #if 0
                    sorry // Reception ring-buffer overflow!
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
