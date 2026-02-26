#ifndef SPI_BLOCK_SIZE
#error "Please define `SPI_BLOCK_SIZE` to a word-multiple value!"
#endif

typedef u8 SPIBlock[SPI_BLOCK_SIZE] __attribute__((aligned(4)));

#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        expansions  = (('driver', '&_SPI_drivers[handle]'),),
        terms       = lambda target, type, peripheral, handle: (
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

struct SPIDriver // @/`SPI Driver Design`.
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

    NVIC_DISABLE(SPIx);

    memzero(driver);



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
        EOTIE  , true , //     - When all of the expected amount of bytes have been transferred.
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



    // Enable the interrupts.

    NVIC_ENABLE(SPIx);

}



////////////////////////////////////////////////////////////////////////////////



static enum SPIUpdateOnceResult : u32
{
    SPIUpdateOnceResult_again,
    SPIUpdateOnceResult_yield,
    SPIUpdateOnceResult_bug = BUG_CODE,
}
_SPI_update_once(enum SPIHandle handle)
{

    _EXPAND_HANDLE



    u32 interrupt_status = SPIx->SR;
    u32 interrupt_enable = SPIx->IER;



    // Mode fault.

    if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, MODF))
        bug; // This error should only happen in SPI master mode.



    // TI mode frame format error.

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, TIFRE))
        bug; // This error should only happen when using TI mode.



    // CRC mismatch.

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, CRCE))
    {

        CMSIS_SET(SPIx, IFCR, CRCEC, true); // Acknowledge the CRC mismatch condition.

        CMSIS_SET(SPIx, CR1, SPE, false); // @/`SPI Activation Cycling`.
        CMSIS_SET(SPIx, CR1, SPE, true ); // "

        driver->byte_index = 0;

        return SPIUpdateOnceResult_again;

    }



    // Data over-run.

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, OVR))
    {

        CMSIS_SET(SPIx, IFCR, OVRC, true); // Acknowledge the overrun condition.

        // Uh oh, the RX-FIFO got too full!
        // For now, we'll just silently ignore this error condition.
        // The user should have a checksum anyways to ensure integrity
        // of the received data.

        return SPIUpdateOnceResult_again;

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
            // For now, we'll silently drop the data.
        }

        return SPIUpdateOnceResult_again;

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
                // For now, we'll just drop the data without indicating that this has happened.
                // The user should have a checksum anyways to ensure integrity of the received data.
            }
        }
        else
        {
            // We dropped data at some point.
        }

        driver->byte_index = 0;

        return SPIUpdateOnceResult_again;

    }



    // Nothing left to handle for now.

    else
    {

        if (interrupt_status & interrupt_enable)
            bug; // We overlooked handling an interrupt event...

        return SPIUpdateOnceResult_yield;

    }

}

static void
_SPI_driver_interrupt(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        enum SPIUpdateOnceResult result = _SPI_update_once(handle);

        switch (result)
        {

            case SPIUpdateOnceResult_again:
            {
                // See if there's still more stuff to do.
            } break;

            case SPIUpdateOnceResult_yield:
            {
                yield = true;
            } break;

            case SPIUpdateOnceResult_bug:
            default:
            {

                // Shut down the driver. @/`SPI Driver Design`.

                CMSIS_PUT(SPIx_RESET, true);
                NVIC_DISABLE(SPIx);

                yield = true;

            } break;

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`SPI Driver Design`:
//
// This SPI driver is currently very simple; it only handles receiving data
// from a master in fixed block size with a CRC. Our project does not need
// anything more than just this, so it's why the driver is the way it is.
// Nonetheless, the SPI driver is opened to being extended in the future.
//
// It should be noted that the way error handling is currently being done
// for this implementation is that the SPI peripheral is completely turned
// off without any indication to the user. The reasoning here is that hard
// errors (like unexpected interrupt events) are incredibly unlikely, so much
// that it doesn't warrant the user to actually handle it every time the API
// is used.
//
// Another reason is that the user, at least in the context of the current
// project as of writing, should have an additional layer of error conditions
// to detect bad SPI communication. For example, if the user has not received
// data in a while, either because the SPI driver encountered a hard error or
// because the master device is locked up, the user should reinitialize the
// SPI driver and reset the master device if possible.



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
