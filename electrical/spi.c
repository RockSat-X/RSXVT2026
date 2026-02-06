#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        expansions  = (('driver', '&_SPI_drivers[handle]'),),
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

#define SPI_BLOCK_SIZE 64 // @/`OpenMV SPI Block Size`.

typedef u8 SPIBlock[SPI_BLOCK_SIZE];

struct SPIDriver
{
    i32                     byte_index;
    RingBuffer(SPIBlock, 8) ring_buffer;
};

static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};

#define SPI_ring_buffer(HANDLE) &_SPI_drivers[(HANDLE)].ring_buffer



////////////////////////////////////////////////////////////////////////////////



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
        SSIOP  , 0b0  , // When 0b0, the NSS pin is active-low.
        CPOL   , 0b0  , // When 0b0, the SCK pin is low.
        CPHA   , 0b0  , // When 0b0, data is captured on the first clock edge.
        LSBFRST, false, // Whether or not LSb is transmitted first.
    );

    CMSIS_SET
    (
        SPIx   , CFG1           ,
        DSIZE  , bitsof(u32) - 1, // Bits per word.
        FTHLV  , 1 - 1          , // Amount of words buffered to trigger an interrupt.
        CRCEN  , true           , // Enable CRC checking.
        CRCSIZE, 8 - 1          , // Amount of bits in CRC.
    );

    CMSIS_SET
    (
        SPIx   , CRCPOLY, // Set CRC polynomial with explicit MSb.
        CRCPOLY, 0x107  , // @/`OpenMV CRC Polynomial`.
    );

    CMSIS_SET
    (
        SPIx   , IER  , // Enable interrupts for:
        MODFIE , true , //     - Mode fault.
        TIFREIE, true , //     - TI frame error.
        CRCEIE , true , //     - CRC mismatch.
        OVRIE  , true , //     - RX-FIFO got too full.
        UDRIE  , false, //     - (Don't care) TX-FIFO was not filled with data in time.
        TXTFIE , false, //     - (Don't care) All data is buffered to be transmitted.
        EOTIE  , true , //     - When all of the expected amount of bytes have been transferred.
        DXPIE  , false, //     - (Don't care) Space and data available in TX/RX-FIFO.
        TXPIE  , false, //     - (Don't care) Space available in TX-FIFO.
        RXPIE  , true , //     - Data available in RX-FIFO.
    );

    CMSIS_SET(SPIx, CR2, TSIZE, SPI_BLOCK_SIZE / sizeof(u32)); // Amount of words followed by the CRC.
    static_assert(SPI_BLOCK_SIZE % sizeof(u32) == 0);

    CMSIS_SET
    (
        SPIx    , CR1  ,
        RCRCINI , false, // Whether or not to initialize the CRC digest with all 1s for the receiver.
        CRC33_17, false, // Whether or not to use the full 33-bit/17-bit CRC polynomial.
        SPE     , true , // Activate the peripheral.
    );

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

            CMSIS_SET(SPIx, IFCR, CRCEC, true); // Acknowledge the CRC mismatch condition.

            CMSIS_SET(SPIx, CR1, SPE, false); // @/`SPI Activation Cycling`.
            CMSIS_SET(SPIx, CR1, SPE, true ); // "

            driver->byte_index = 0;

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

            u32 data = *(u32*) &SPIx->RXDR; // Pop 32-bit word from the RX-FIFO.

            SPIBlock* block = RingBuffer_writing_pointer(&driver->ring_buffer);

            if (block)
            {
                *(u32*) (&(*block)[driver->byte_index])  = __builtin_bswap32(data);
                driver->byte_index                      += sizeof(u32);
            }
            else
            {
                // There's no next spot in the ring-buffer to put the data in.
                // For now, we'll silently drop the byte.
            }

        }



        // All expected data has been transferred successfully.

        else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, EOT))
        {

            CMSIS_SET(SPIx, CR1, SPE, false); // @/`SPI Activation Cycling`.
            CMSIS_SET(SPIx, CR1, SPE, true ); // "

            if (driver->byte_index == SPI_BLOCK_SIZE)
            {
                if (!RingBuffer_push(&driver->ring_buffer, nullptr))
                {
                    // Uh oh, ring-buffer over-run!
                    // For now, we'll just drop the data
                    // without indicating that this has happened.
                    // The user should have a checksum anyways
                    // to ensure integrity of the received data.
                }
            }
            else
            {
                // We missed some data at some point.
            }

            driver->byte_index = 0;

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



////////////////////////////////////////////////////////////////////////////////



// @/`SPI Activation Cycling`:
//
// Whenever a transfer is done, either with a CRC error or not,
// preparing for the next transfer is done by deactivating and
// reactivating the SPI peripheral. This is just how the RM states
// it should be done.
//
// It should be noted that this is also how the SPI transfer is
// resynchronized. If the peripheral expected the transfer to end
// but more data keeps being sent (i.e. NSS doesn't get released),
// then very likely a CRC error will happen, to which the SPI peripheral
// goes through the process of activiation cycling. When this happens
// the SPI peripheral does not start picking up on the data until
// the next NSS cycle. This overall prevents the SPI slave and master
// from being perpetually out-of-sync with each other.
