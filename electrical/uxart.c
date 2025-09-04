////////////////////////////////////////////////////////////////////////////////



enum UXARTHandle : u32
{
    UXARTHandle_STLINK,
    UXARTHandle_COUNT,
};



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



#define _EXPAND_HANDLE \
    struct UXARTDriver* driver      = &_UXART_drivers[handle]; \
    auto const UXARTx_RESET         = (struct CMSISTuple) { &RCC->APB1LRSTR, RCC_APB1LRSTR_USART2RST_Pos, RCC_APB1LRSTR_USART2RST_Msk }; \
    auto const NVICInterrupt_UXARTx = NVICInterrupt_USART2;



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
