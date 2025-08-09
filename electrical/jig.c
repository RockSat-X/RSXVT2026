static void
JIG_tx_raw(u8* data, i32 length)
{
    for (i32 i = 0; i < length; i += 1)
    {
        while (!CMSIS_GET(USART3, ISR, TXE)); // Wait if the TX-FIFO is full.
        CMSIS_SET(USART3, TDR, TDR, data[i]);
    }
}

static void
JIG_tx(char* format)
{
    i32 length = strlen(format);
    JIG_tx_raw((u8*) format, length);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static void
JIG_init(void)
{
    NVIC_ENABLE(USART3);
    CMSIS_SET(RCC   , APB1ENR1, USART3EN, true               ); // Clock the USART peripheral.
    CMSIS_SET(USART3, BRR     , BRR     , USART3_BRR_BRR_init); // Set baud rate.
    CMSIS_SET(USART3, CR3     , EIE     , true               ); // Trigger interrupt upon error.
    CMSIS_SET
    (
        USART3, CR1 ,
        RXNEIE, true, // Trigger interrupt when receiving a byte.
        TE    , true, // Enable transmitter.
        RE    , true, // Enable receiver.
        UE    , true, // Enable USART.
    );

    JIG_tx("\x1B[2J\x1B[H"); // Clears the whole terminal display.
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT(USART3)
{



    //
    // Handle reception errors.
    //

    b32 reception_errors =
        USART3->ISR &
        (
            USART_ISR_FE  | // Frame error.
            USART_ISR_NE  | // Noise error.
            USART_ISR_PE  | // Parity error (for completion, even though a parity bit is not used).
            USART_ISR_ORE   // Overrun error.
        );

    if (reception_errors)
    {
        #if 1 // We got a reception error, but this isn't a critical thing, so we can just silently ignore it...
            CMSIS_SET
            (
                USART3, ICR ,
                FECF  , true, // Clear frame error.
                NECF  , true, // Clear noise error.
                PECF  , true, // Clear parity error (for completeness, even though a parity bit is not used).
                ORECF , true, // Clear overrun error.
            );
        #else
            sorry
        #endif
    }



    //
    // Handle received data.
    //

    if (CMSIS_GET(USART3, ISR, RXNE)) // We got a byte of data?
    {
        u8 data = USART3->RDR; // Pop from the RX-buffer.

        if (_JIG.reception_writer == (u32) (_JIG.reception_reader + countof(_JIG.reception_buffer))) // RX-FIFO is full?
        {
            #if 0 // We got an overflow, but this isn't a critical thing, so we can just silently ignore it...
                sorry
            #endif
        }
        else // Push received byte into the ring-buffer.
        {
            static_assert(IS_POW_2(countof(_JIG.reception_buffer)));
            i32 index = _JIG.reception_writer & (countof(_JIG.reception_buffer) - 1);

            _JIG.reception_buffer[index]  = data;
            _JIG.reception_writer        += 1;
        }
    }



}
