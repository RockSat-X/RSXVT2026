#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include <printf/printf.c>
#pragma GCC diagnostic pop



static struct Jig _JIG = {0};


#if TARGET_NAME_IS_SandboxNucleoH7S3L8
    #define UxART_STLINK                                USART3
    #define UxART_STLINK_                               USART3_
    #define UxART_STLINK_IRQn                           USART3_IRQn
    #define UxART_STLINK_BRR_BRR_init                   USART3_BRR_BRR_init
    #define INTERRUPT_UxART_STLINK                      INTERRUPT_USART3
    #define __MACRO_OVERLOAD__NVIC_ENABLE__UxART_STLINK __MACRO_OVERLOAD__NVIC_ENABLE__USART3
    static const struct CMSISPutTuple UxART_STLINK_EN = { .dst = &RCC->APB1ENR1, .pos = RCC_APB1ENR1_USART3EN_Pos, .msk = RCC_APB1ENR1_USART3EN_Msk };
#endif

#if TARGET_NAME_IS_SandboxNucleoH533RE
    #define UxART_STLINK                                USART2
    #define UxART_STLINK_                               USART2_
    #define UxART_STLINK_IRQn                           USART2_IRQn
    #define UxART_STLINK_BRR_BRR_init                   USART2_BRR_BRR_init
    #define INTERRUPT_UxART_STLINK                      INTERRUPT_USART2
    #define __MACRO_OVERLOAD__NVIC_ENABLE__UxART_STLINK __MACRO_OVERLOAD__NVIC_ENABLE__USART2
    static const struct CMSISPutTuple UxART_STLINK_EN = { .dst = &RCC->APB1LENR, .pos = RCC_APB1LENR_USART2EN_Pos, .msk = RCC_APB1LENR_USART2EN_Msk };
#endif



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static void
JIG_tx_raw(u8* data, i32 length)
{
    for (i32 i = 0; i < length; i += 1)
    {
        while (!CMSIS_GET(UxART_STLINK, ISR, TXE)); // Wait if the TX-FIFO is full.
        CMSIS_SET(UxART_STLINK, TDR, TDR, data[i]);
    }
}



#define JIG_tx(...) fctprintf(&_JIG_fctprintf_callback, nullptr, __VA_ARGS__)
static void
_JIG_fctprintf_callback(char character, void*)
{
    JIG_tx_raw(&(u8) { character }, 1);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static void
JIG_init(void)
{
    NVIC_ENABLE(UxART_STLINK);
    CMSIS_PUT(UxART_STLINK_EN                    , true                     ); // Clock the UxART peripheral.
    CMSIS_SET(UxART_STLINK   , BRR     , BRR     , UxART_STLINK_BRR_BRR_init); // Set baud rate.
    CMSIS_SET(UxART_STLINK   , CR3     , EIE     , true                     ); // Trigger interrupt upon error.
    CMSIS_SET
    (
        UxART_STLINK, CR1 ,
        RXNEIE      , true, // Trigger interrupt when receiving a byte.
        TE          , true, // Enable transmitter.
        RE          , true, // Enable receiver.
        UE          , true, // Enable UxART.
    );

    JIG_tx("\x1B[2J\x1B[H"); // Clears the whole terminal display.
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



INTERRUPT_UxART_STLINK
{

    //
    // Handle reception errors.
    //

    b32 reception_errors =
        UxART_STLINK->ISR &
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
                UxART_STLINK, ICR ,
                FECF        , true, // Clear frame error.
                NECF        , true, // Clear noise error.
                PECF        , true, // Clear parity error (for completeness, even though a parity bit is not used).
                ORECF       , true, // Clear overrun error.
            );
        #else
            sorry
        #endif
    }



    //
    // Handle received data.
    //

    if (CMSIS_GET(UxART_STLINK, ISR, RXNE)) // We got a byte of data?
    {
        u8 data = UxART_STLINK->RDR; // Pop from the RX-buffer.

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
