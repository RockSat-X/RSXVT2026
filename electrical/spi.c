#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        expansions  = (('SPI_ring_buffer', '&SPI_ring_buffers[handle]'),),
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

static RingBuffer(u8, 256) SPI_ring_buffers[SPIHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static void
SPI_reinit(enum SPIHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(SPIx_RESET, true );
    CMSIS_PUT(SPIx_RESET, false);

    SPI_ring_buffer->ring_buffer_raw = (struct RingBufferRaw) {0};



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
        SSIOP  , 0b0  , // When 0b0, the NSS pin is active-low.
        CPOL   , 0b0  , // When 0b0, the SCK pin is low.
        CPHA   , 0b0  , // When 0b0, data is captured on the first clock edge.
        LSBFRST, false, // Whether or not LSb is transmitted first.
    );

    CMSIS_SET
    (
        SPIx , CFG1 ,
        DSIZE, 8 - 1, // Bits per word.
        FTHLV, 1 - 1, // Amount of words buffered to trigger an interrupt.
    );

    CMSIS_SET
    (
        SPIx   , IER  , // Enable interrupts for:
        MODFIE , true , //     - Mode fault.
        TIFREIE, true , //     - (Unlikely) TI frame error.
        CRCEIE , true , //     - CRC mismatch.
        OVRIE  , true , //     - RX-FIFO got too full.
        UDRIE  , false, //     - (Don't care) TX-FIFO was not filled with data in time.
        TXTFIE , false, //     - (Don't care) All data is buffered to be transmitted.
        EOTIE  , false, //     - (Don't care) All data has been transmitted.
        DXPIE  , false, //     - (Don't care) Space and data available in TX/RX-FIFO.
        TXPIE  , false, //     - (Don't care) Space available in TX-FIFO.
        RXPIE  , true , //     - Data available in RX-FIFO.
    );



    // Activate the peripheral.

    CMSIS_SET(SPIx, CR1, SPE, true);

}



static void
_SPI_driver_interrupt(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        u32 interrupt_status = SPIx->SR;
        u32 interrupt_enable = SPIx->IER;



        // Mode fault.

        if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, MODF))
        {
            panic; // This error should only happen in SPI master mode.
        }



        // TI mode frame format error.

        else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, TIFRE))
        {
            panic; // This error should only happen when using TI mode.
        }



        // CRC mismatch.

        else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, CRCE))
        {
            panic; // This error should only happen when CRC is enable. TODO Use?
        }



        // Data over-run.

        else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, OVR))
        {

            CMSIS_SET(SPIx, IFCR, OVRC, true); // Acknowledge the over-run condition.

            // Uh oh, the RX-FIFO got too full!
            // For now, we'll just silently ignore
            // this error condition.
            // The user should have a checksum anyways
            // to ensure integrity of the received data.

        }



        // Data available in RX-FIFO.

        else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, RXP))
        {

            u8 data = *(u8*) &SPIx->RXDR; // Pop from the RX-FIFO.

            if (!RingBuffer_push(SPI_ring_buffer, &data))
            {
                // Uh oh, ring-buffer over-run!
                // For now, we'll just drop the data
                // without indicating that this has happened.
                // The user should have a checksum anyways
                // to ensure integrity of the received data.
            }

        }



        // Nothing left to handle for now.

        else
        {

            if (interrupt_status & interrupt_enable)
                panic; // We overlooked handling an interrupt event...

            yield = true;

        }

    }

}
