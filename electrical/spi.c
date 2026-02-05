#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        terms       = lambda type, peripheral, handle: (
            ('{}'                    , 'expression' ),
            ('NVICInterrupt_{}'      , 'expression' ),
            ('STPY_{}_KERNEL_SOURCE' , 'expression' ),
            ('{}_RESET'              , 'cmsis_tuple'),
            ('{}_ENABLE'             , 'cmsis_tuple'),
            ('{}_KERNEL_SOURCE'      , 'cmsis_tuple'),
            ('{}'                    , 'interrupt'  ),
        ),
    )

*/



struct SPIDriver
{
    u8  buffer[1 << 8];
    u32 reader;
    u32 writer;
};



static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret b32
SPI_receive_byte(enum SPIHandle handle, u8* dst)
{

    _EXPAND_HANDLE

    b32 data_available = driver->writer != driver->reader;

    if (dst && data_available)
    {

        i32 index = driver->reader % countof(driver->buffer);
        *dst            = driver->buffer[index];
        driver->reader += 1;
    }
    else
    {
        *dst = 0;
    }

    return data_available;

}



static void
SPI_reinit(enum SPIHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(SPIx_RESET, true );
    CMSIS_PUT(SPIx_RESET, false);

    *driver = (struct SPIDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(SPIx);



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(SPIx_KERNEL_SOURCE, STPY_SPIx_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(SPIx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET
    (
        SPIx   , CFG2 ,
        MASTER , false, // SPI in slave mode.
        SSIOP  , false, // The SS  pin level during inactivity.
        CPOL   , false, // The SCK pin level during inactivity.
        CPHA   , false, // Capture data on the second clock edge?
        LSBFRST, false, // LSb transmitted first?
        AFCNTR , true , // Don't leave pins floating when SPI is disabled.
    );

    CMSIS_SET
    (
        SPIx , CFG1 ,
        DSIZE, 8 - 1, // Bits per word.
        FTHLV, 1 - 1, // Amount of words for an interrupt.
    );



    // The slave-receiver can be enabled immediately.

    CMSIS_SET
    (
        SPIx , IER , // Enable interrupts for:
        OVRIE, true, //     - Overrun error.
        RXPIE, true, //     - FIFO received a packet to be read.
    );

    CMSIS_SET(SPIx, CR1, SPE, true);

}



static useret enum SPIUpdateOnce : u32
{
    SPIUpdateOnce_again,
    SPIUpdateOnce_yield,
}
_SPI_update_once(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    enum SPIInterruptEvent : u32
    {
        SPIInterruptEvent_none,
        SPIInterruptEvent_data_overrun,
        SPIInterruptEvent_data_available_to_read,
    };
    enum SPIInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = SPIx->SR;
    u32                    interrupt_enable = SPIx->IER;



    // Uh oh, we missed some data...

    if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, OVR))
    {
        CMSIS_SET(SPIx, IFCR, OVRC, true); // Acknowledge the overrun condition.
        interrupt_event = SPIInterruptEvent_data_overrun;
    }



    // We got data to read!

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, RXP))
    {
        interrupt_event = SPIInterruptEvent_data_available_to_read;
    }



    switch (interrupt_event)
    {

        case SPIInterruptEvent_none:
        {
            return SPIUpdateOnce_yield; // Nothing interesting has happend.
        } break;

        case SPIInterruptEvent_data_overrun:
        {

            // Uh oh, the RX-FIFO got too full!
            // For now, we'll just silently ignore
            // this error condition.
            // The user should have a checksum anyways
            // to ensure integrity of the recieved data.

            return SPIUpdateOnce_yield;

        } break;

        case SPIInterruptEvent_data_available_to_read:
        {

            u8 data = *(u8*) &SPIx->RXDR; // Pop from the RX-FIFO.

            if (driver->writer - driver->reader < countof(driver->buffer))
            {
                i32 index = driver->writer % countof(driver->buffer);
                driver->buffer[index]  = data;
                driver->writer        += 1;
            }
            else
            {
                // Uh oh, ring-buffer overrun!
                // For now, we'll just drop the data
                // without indicating that this has happened.
                // The user should have a checksum anyways
                // to ensure integrity of the recieved data.
            }

            return SPIUpdateOnce_again;

        } break;

        default: panic;

    }

}



static void
_SPI_driver_interrupt(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        enum SPIUpdateOnce result = _SPI_update_once(handle);

        yield = (result == SPIUpdateOnce_yield);

        switch (result)
        {
            case SPIUpdateOnce_again:
            {
                // The state-machine will be updated again.
            } break;

            case SPIUpdateOnce_yield:
            {
                // We can stop updating the state-machine for now.
            } break;

            default: panic;
        }

    }

}
