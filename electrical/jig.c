INTERRUPT(USART3)
{
    sorry
}

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
