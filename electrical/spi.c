#ifndef SPI_BLOCK_SIZE
#error "Please define `SPI_BLOCK_SIZE` to a word-multiple value!"
#endif

#ifndef SPI_RECEPTION_RING_BUFFER_LENGTH
#error "Please define `SPI_RECEPTION_RING_BUFFER_LENGTH`!"
#endif

#include "spi_driver_support.meta"
/* #meta

    # TODO We're currently only supporting SPI2 because we're
    #      making assumptions about the DMA configuration.
    #      This is obviously not ideal for a general purpose SPI
    #      driver, but in the future, this can be fixed.

    for target in PER_TARGET():

        driver = [
            driver
            for driver in target.drivers
            if driver['type'] == 'SPI'
        ]

        if len(driver) >= 2:
            Meta.line(
                f'#error Target {repr(target.name)} defines multiple SPI drivers; '
                f'the current implementation does not support this currently. Sorry!'
            )
            continue

        if driver:

            driver, = driver

            if driver['peripheral'] != 'SPI2':
                Meta.line(
                    f'#error Target {repr(target.name)} defines a SPI driver for {repr(driver['peripheral'])}; '
                    f'the current implementation only supports SPI2. Sorry!'
                )
                continue

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

struct SPIBlock
{
    union
    {
        u8  bytes[SPI_BLOCK_SIZE / sizeof(u8 )] __attribute__((aligned(4)));
        u32 words[SPI_BLOCK_SIZE / sizeof(u32)] __attribute__((aligned(4)));
        static_assert(SPI_BLOCK_SIZE % sizeof(u32) == 0);
    };
};

enum SPIDriverState : u32
{
    SPIDriverState_not_transferring,
    SPIDriverState_transfer_queued,
    SPIDriverState_discarding,
};

struct SPIDriver // @/`SPI Driver Design`.
{
    enum SPIDriverState                                           state;
    RingBuffer(struct SPIBlock, SPI_RECEPTION_RING_BUFFER_LENGTH) reception;
};

static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



// Terse macro for the user to access the reception ring-buffer; note that it's
// like this so the user can either access the data either in-place or by copy.
#define SPI_reception(HANDLE) &_SPI_drivers[(HANDLE)].reception



static void
SPI_reinit(enum SPIHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST, true );
    CMSIS_PUT(SPIx_RESET              , true );
    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST, false);
    CMSIS_PUT(SPIx_RESET              , false);

    NVIC_DISABLE(GPDMA1_Channel7);
    NVIC_DISABLE(SPIx);

    memzero(driver);



    // Clock the GPDMA1 peripheral.

    CMSIS_SET(RCC, AHB1ENR, GPDMA1EN, true);



    // Configure the DMA channel.

    CMSIS_SET
    (
        GPDMA1_Channel7, CSAR,
        SA             , (u32) &SPIx->RXDR // Where to pop the received SPI data from.
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CTR1 ,
        SINC           , false, // Source addresss will stay fixed.
        DDW_LOG2       , 0b10 , // Transfer size of four bytes.
        SDW_LOG2       , 0b10 , // "
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CTR2,
        PFREQ          , true, // The peripheral will dictate when the transfer can happen.
        REQSEL         , 8   , // Get the request from `spi2_rx_dma`.
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CCR , // Enable interrupts for:
        TOIE           , true, //     - Trigger over-run.
        SUSPIE         , true, //     - Completed suspension.
        USEIE          , true, //     - User setting error.
        ULEIE          , true, //     - Update link transfer error.
        DTEIE          , true, //     - Data transfer error.
    );

    CMSIS_SET
    (
        GPDMA1, MISR,
        MIS7  , true, // Allow interrupts for the DMA channel.
    );



    // Set the kernel clock source for the SPI peripheral.

    CMSIS_PUT(SPIx_KERNEL_SOURCE, STPY_SPIx_KERNEL_SOURCE);



    // Enable the SPI peripheral.

    CMSIS_PUT(SPIx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET
    (
        SPIx   , CFG2 ,
        MASTER , false, // SPI in slave mode.
        SSIOP  , 0b0  , // When 0b0, the NSS pin is active-low.
        CPOL   , 0b1  , // "1: SCK signal is at 1 when idle".
        CPHA   , 0b0  , // "0: the first clock transition is the first data capture edge".
        LSBFRST, false, // Whether or not LSb is transmitted first.
    );

    CMSIS_SET
    (
        SPIx   , CFG1          ,
        DSIZE  , bitsof(u8) - 1, // Smallest unit of SPI transfer we're assuming.
        FTHLV  , 4 - 1         , // Amount of bytes buffered to trigger an interrupt.
        CRCEN  , true          , // Enable CRC checking.
        CRCSIZE, 8 - 1         , // Amount of bits in CRC.
        RXDMAEN, true          , // Enable DMA for reception.
    );

    CMSIS_SET
    (
        SPIx   , CRCPOLY, // Set CRC polynomial with explicit MSb.
        CRCPOLY, 0x107  , // @/`OpenMV CRC Polynomial`.
    );

    CMSIS_SET
    (
        SPIx   , IER , // Enable interrupts for:
        MODFIE , true, //     - Mode fault.
        TIFREIE, true, //     - TI frame error.
        CRCEIE , true, //     - CRC mismatch.
        EOTIE  , true, //     - When all of the expected amount of bytes have been transferred.
    );

    CMSIS_SET(SPIx, CR2, TSIZE, SPI_BLOCK_SIZE); // Amount of bytes followed by the CRC.

    CMSIS_SET
    (
        SPIx    , CR1  ,
        RCRCINI , false, // Whether or not to initialize the CRC digest with all 1s for the receiver.
        CRC33_17, false, // Whether or not to use the full 33-bit/17-bit CRC polynomial.
    );



    // Enable the interrupts.

    NVIC_ENABLE(GPDMA1_Channel7);
    NVIC_ENABLE(SPIx);
    NVIC_SET_PENDING(SPIx); // The interrupt will configure for the first transfer.

}



////////////////////////////////////////////////////////////////////////////////



static enum SPIConfigureTransferResult : u32
{
    SPIConfigureTransferResult_transfer_queued,
    SPIConfigureTransferResult_discarding,
    SPIConfigureTransferResult_bug = BUG_CODE,
}
_SPI_configure_transfer(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
        bug; // DMA active for some reason?

    struct SPIBlock* block = RingBuffer_writing_pointer(&driver->reception);

    if (block)
    {
        CMSIS_SET(GPDMA1_Channel7, CTR1, DINC, true       ); // Increment the destination address on each transfer.
        CMSIS_SET(GPDMA1_Channel7, CDAR, DA  , (u32) block); // Initial destination address.
    }
    else // No more buffer space to put the next received block; just set up the DMA to discard the data.
    {
        static u32 __attribute__((aligned(4))) trash = {0};
        CMSIS_SET(GPDMA1_Channel7, CTR1, DINC, false       ); // Destination address is fixed.
        CMSIS_SET(GPDMA1_Channel7, CDAR, DA  , (u32) &trash); // Just dump the data here.
    }

    CMSIS_SET(GPDMA1_Channel7, CBR1, BNDT, sizeof(*block)); // Amount of bytes to transfer.
    CMSIS_SET(SPIx           , CR1 , SPE , false         ); // @/`SPI Activation Cycling`.
    CMSIS_SET(GPDMA1_Channel7, CCR , EN  , true          ); // DMA ready! Note that by disabling the SPI, the SPI FIFO is flushed.
    CMSIS_SET(SPIx           , CR1 , SPE , true          ); // @/`SPI Activation Cycling`.

    if (block)
    {
        return SPIConfigureTransferResult_transfer_queued;
    }
    else
    {
        return SPIConfigureTransferResult_discarding;
    }

}



static enum SPIUpdateOnceResult : u32
{
    SPIUpdateOnceResult_again,
    SPIUpdateOnceResult_yield,
    SPIUpdateOnceResult_bug = BUG_CODE,
}
_SPI_update_once(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    enum SPIInterruptEvent : u32
    {
        SPIInterruptEvent_none,
        SPIInterruptEvent_crc_mismatch,
        SPIInterruptEvent_end_of_transfer,
    };

    enum SPIInterruptEvent interrupt_event      = {0};
    u32                    dma_interrupt_status = GPDMA1_Channel7->CSR;
    u32                    spi_interrupt_status = SPIx->SR;
    u32                    spi_interrupt_enable = SPIx->IER;



    // The DMA failed to update the link-listed.
    //
    // @/pg 677/sec 16.4.17/`H533rm`.

    if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, ULEF))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // The DMA channel is done being suspended.
    //
    // @/pg 642/sec 16.4.3/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, SUSPF))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // The DMA channel got triggered too quickly by an external trigger.
    //
    // @/pg 672/sec 16.4.12/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, TOF))
    {
        bug; // Shouldn't happen; DMA channel isn't using an external trigger.
    }



    // The DMA configuration is invalid.
    //
    // @/pg 677/sec 16.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, USEF))
    {
        bug; // Something obvious like this shouldn't happen.
    }



    // The DMA encountered an error while trying to transfer the data.
    //
    // @/pg 676/sec 16.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, DTEF))
    {
        bug; // Something obvious like this shouldn't happen.
    }



    // Mode fault.

    else if (CMSIS_GET_FROM(spi_interrupt_status, SPIx, SR, MODF))
    {
        bug; // This error should only happen in SPI master mode.
    }



    // TI mode frame format error.

    else if (CMSIS_GET_FROM(spi_interrupt_status, SPIx, SR, TIFRE))
    {
        bug; // This error should only happen when using TI mode.
    }



    // CRC mismatch.

    else if (CMSIS_GET_FROM(spi_interrupt_status, SPIx, SR, CRCE))
    {
        CMSIS_SET(SPIx, IFCR, CRCEC, true);
        interrupt_event = SPIInterruptEvent_crc_mismatch;
    }



    // All expected data has been transferred successfully.

    else if (CMSIS_GET_FROM(spi_interrupt_status, SPIx, SR, EOT))
    {
        interrupt_event = SPIInterruptEvent_end_of_transfer;
    }



    // Nothing left to handle for now.

    else
    {

        if (spi_interrupt_status & spi_interrupt_enable)
            bug; // We overlooked handling an interrupt event...

        interrupt_event = SPIInterruptEvent_none;

    }



    // Handle the interrupt event.

    switch (interrupt_event)
    {



        // Nothing notable happened.

        case SPIInterruptEvent_none: switch (driver->state)
        {

            case SPIDriverState_not_transferring: // Need to set up the first transfer.
            {

                if (CMSIS_GET(SPIx, CR1, SPE))
                    bug; // Not transferring but SPI enabled..?

                enum SPIConfigureTransferResult configure_transfer_result = _SPI_configure_transfer(handle);

                switch (configure_transfer_result)
                {

                    case SPIConfigureTransferResult_transfer_queued:
                    {
                        driver->state = SPIDriverState_transfer_queued;
                    } break;

                    case SPIConfigureTransferResult_discarding : bug; // No reason to be discarding already...
                    case SPIConfigureTransferResult_bug        : bug;
                    default                                    : bug;

                }

                return SPIUpdateOnceResult_again;

            } break;

            case SPIDriverState_transfer_queued:
            case SPIDriverState_discarding:
            {
                return SPIUpdateOnceResult_yield; // Don't care.
            } break;

            default: bug;

        } break;



        // Whoops, data corruption...
        // We currently don't signal this to the user; they should have some sort
        // of checksum or sequence number to indicate a dropped packet anyways.

        case SPIInterruptEvent_crc_mismatch: switch (driver->state)
        {

            case SPIDriverState_transfer_queued: // Just go ahead and schedule the next transfer.
            case SPIDriverState_discarding:      // "
            {

                enum SPIConfigureTransferResult configure_transfer_result = _SPI_configure_transfer(handle);

                switch (configure_transfer_result)
                {

                    case SPIConfigureTransferResult_transfer_queued:
                    {
                        driver->state = SPIDriverState_transfer_queued;
                    } break;

                    case SPIConfigureTransferResult_discarding:
                    {
                        driver->state = SPIDriverState_discarding;
                    } break;

                    case SPIConfigureTransferResult_bug : bug;
                    default                             : bug;

                }

                return SPIUpdateOnceResult_again;

            } break;

            case SPIDriverState_not_transferring : bug;
            default                              : bug;

        } break;



        // Finished gathering all(?) the received SPI data.

        case SPIInterruptEvent_end_of_transfer: switch (driver->state)
        {

            case SPIDriverState_transfer_queued:
            case SPIDriverState_discarding:
            {

                if (!CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, TCF))
                    bug; // DMA didn't finish transferring by now..?

                CMSIS_SET(GPDMA1_Channel7, CFCR, TCF, true); // Acknowledge the completion of the DMA transfer.

                if (CMSIS_GET_FROM(spi_interrupt_status, SPIx, SR, OVR)) // Somehow got data over-run?
                {
                    // Although we use DMA, this can still happen if we were debugging for instance.
                    CMSIS_SET(SPIx, IFCR, OVRC, true); // Acknowledge that we missed some data.
                }
                else if (driver->state == SPIDriverState_transfer_queued)
                {
                    if (!RingBuffer_push(&driver->reception, nullptr))
                        bug; // There should've been buffer space for the DMA to transfer into...
                }

                enum SPIConfigureTransferResult configure_transfer_result = _SPI_configure_transfer(handle);

                switch (configure_transfer_result)
                {

                    case SPIConfigureTransferResult_transfer_queued:
                    {
                        driver->state = SPIDriverState_transfer_queued;
                    } break;

                    case SPIConfigureTransferResult_discarding:
                    {
                        driver->state = SPIDriverState_discarding;
                    } break;

                    case SPIConfigureTransferResult_bug : bug;
                    default                             : bug;

                }

                return SPIUpdateOnceResult_again;

            } break;

            case SPIDriverState_not_transferring : bug;
            default                              : bug;

        } break;



        default: bug;

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

                CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST, true);
                CMSIS_PUT(SPIx_RESET              , true);

                NVIC_DISABLE(GPDMA1_Channel7);
                NVIC_DISABLE(SPIx);

                memzero(driver);

                yield = true;

            } break;

        }

    }

}

INTERRUPT_GPDMA1_Channel7(void)
{
    NVIC_SET_PENDING(SPI2); // The user should have the DMA interrupt be of lower priority to the SPI interrupt.
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
