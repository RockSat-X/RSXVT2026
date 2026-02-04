////////////////////////////////////////////////////////////////////////////////



enum SPIRole : u32
{
    SPIRole_master,
    SPIRole_slave_receiver,
};



#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        terms       = lambda type, peripheral, handle, role: (
            ('{}'                    , 'expression' ),
            ('NVICInterrupt_{}'      , 'expression' ),
            ('STPY_{}_KERNEL_SOURCE' , 'expression' ),
            ('STPY_{}_BYPASS_DIVIDER', 'expression' ),
            ('STPY_{}_DIVIDER'       , 'expression' ),
            ('{}_RESET'              , 'cmsis_tuple'),
            ('{}_ENABLE'             , 'cmsis_tuple'),
            ('{}_KERNEL_SOURCE'      , 'cmsis_tuple'),
            ('{}'                    , 'interrupt'  ),
            ('SPIx_ROLE'             , 'expression' , f'SPIRole_{role}'),
        ),
    )

*/



enum SPIDriverMasterState : u32
{
    SPIDriverMasterState_standby,
    SPIDriverMasterState_scheduled_transfer,
    SPIDriverMasterState_transferring,
};



enum SPIDriverMasterError : u32
{
    SPIDriverMasterError_none,
};



struct SPIDriver
{
    union
    {
        struct
        {
            volatile enum SPIDriverMasterState state;
            u8*                                tx_buffer;
            u8*                                rx_buffer;
            i32                                amount;
            i32                                tx_progress;
            i32                                rx_progress;
            enum SPIDriverMasterError          error; // TODO Not needed?
        } master;

        struct
        {
            u8  buffer[1 << 8];
            u32 reader;
            u32 writer;
        } slave_receiver;
    };
};



static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum SPIDriverMasterError
SPI_blocking_transfer
(
    enum SPIHandle handle,
    u8*            tx_buffer,
    u8*            rx_buffer,
    i32            amount
)
{

    _EXPAND_HANDLE



    // Validation.

    if (SPIx_ROLE != SPIRole_master)
        panic;

    if (!tx_buffer)
        panic;

    if (!rx_buffer)
        panic;

    if (amount <= 0)
        panic;



    // Schedule the transfer.

    switch (driver->master.state)
    {
        case SPIDriverMasterState_standby:
        {

            driver->master.tx_buffer   = tx_buffer;
            driver->master.rx_buffer   = rx_buffer;
            driver->master.amount      = amount;
            driver->master.tx_progress = 0;
            driver->master.rx_progress = 0;
            driver->master.error       = SPIDriverMasterError_none;
            __DMB();
            driver->master.state       = SPIDriverMasterState_scheduled_transfer;

            NVIC_SET_PENDING(SPIx);

        } break;

        case SPIDriverMasterState_scheduled_transfer : panic;
        case SPIDriverMasterState_transferring       : panic;
        default                                      : panic;
    }



    // Wait until the transfer is done.

    while (true)
    {
        switch (driver->master.state)
        {

            // The driver just finished!

            case SPIDriverMasterState_standby:
            {
                return driver->master.error;
            } break;



            // The driver is still busy with our transfer.

            case SPIDriverMasterState_scheduled_transfer:
            case SPIDriverMasterState_transferring:
            {
                // Keep waiting...
            } break;



            default: panic;

        }
    }

}



static useret b32
SPI_receive_byte(enum SPIHandle handle, u8* dst)
{

    _EXPAND_HANDLE

    if (SPIx_ROLE != SPIRole_slave_receiver)
        panic;

    b32 data_available = driver->slave_receiver.writer != driver->slave_receiver.reader;

    if (dst && data_available)
    {

        i32 index = driver->slave_receiver.reader % countof(driver->slave_receiver.buffer);
        *dst                           = driver->slave_receiver.buffer[index];
        driver->slave_receiver.reader += 1;
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
        SPIx   , CFG2                       ,
        MASTER , SPIx_ROLE == SPIRole_master, // SPI in master or slave mode.
        SSOE   , true                       , // Enable the Slave-Select pin (when master).
        SSIOP  , false                      , // The SS  pin level during inactivity.
        CPOL   , false                      , // The SCK pin level during inactivity.
        CPHA   , false                      , // Capture data on the second clock edge?
        LSBFRST, false                      , // LSb transmitted first?
        AFCNTR , true                       , // Don't leave pins floating when SPI is disabled.
        MSSI   , 15                         , // Amount of delay after asserting the slave-select (when master).
    );

    CMSIS_SET
    (
        SPIx   , CFG1                    ,
        BPASS  , STPY_SPIx_BYPASS_DIVIDER, // Whether or not the divider is needed (when master).
        MBR    , STPY_SPIx_DIVIDER       , // Divider to generate the baud (when master).
        DSIZE  , 8 - 1                   , // Bits per word.
        FTHLV  , 1 - 1                   , // Amount of words for an interrupt.
    );



    // The slave-receiver can be enabled immediately.

    if (SPIx_ROLE == SPIRole_slave_receiver)
    {

        CMSIS_SET
        (
            SPIx , IER , // Enable interrupts for:
            OVRIE, true, //     - Overrun error.
            RXPIE, true, //     - FIFO received a packet to be read.
        );

        CMSIS_SET(SPIx, CR1, SPE, true);

    }

}



static useret enum SPIUpdateOnce : u32
{
    SPIUpdateOnce_again,
    SPIUpdateOnce_yield,
}
_SPI_update_once(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    switch (SPIx_ROLE)
    {

        case SPIRole_master:
        {

            enum SPIInterruptEvent : u32
            {
                SPIInterruptEvent_none,
                SPIInterruptEvent_data_available_to_read,
                SPIInterruptEvent_data_needs_to_be_written,
                SPIInterruptEvent_end_of_transfer,
            };
            enum SPIInterruptEvent interrupt_event  = {0};
            u32                    interrupt_status = SPIx->SR;
            u32                    interrupt_enable = SPIx->IER;



            // There's new data available from the slave but the RX-FIFO
            // is full. However, this shouldn't be possible, because we
            // prioritize reading from the RX-FIFO first before pushing
            // data into the TX-FIFO to advance the SPI transfer.
            // @/pg 2417/sec 52.5.2/`H533rm`.

            if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, OVR))
            {
                panic;
            }



            // The hardware attempted to read from the TX-FIFO to transmit
            // the data, but the TX-FIFO was empty. This shouldn't be
            // possible, however, because this is only during
            // slave-transmitter mode.
            // @/pg 2418/sec 52.5.2/`H533rm`.

            else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, UDR))
            {
                panic;
            }



            // There is a word available to be popped from the RX-FIFO.
            // We must prioritize reading from the RX-FIFO first before
            // we push any new data in the TX-FIFO so we can avoid a
            // data overrun situation.

            else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, RXP))
            {
                interrupt_event = SPIInterruptEvent_data_available_to_read;
            }



            // The next word can be pushed into the TX-FIFO.
            // When we have pushed all the data that's needed for the
            // transfer, the TXTF status gets asserted and disables
            // the TXP interrupt; nonetheless, the TXP status can still
            // be set, so we have to make sure there's actually new data
            // to even be pushed into the TX-FIFO.
            // @/pg 2458/sec 52.11.6/`H533rm`.

            else if
            (
                CMSIS_GET_FROM(interrupt_status, SPIx, SR, TXP) &&
                CMSIS_GET_FROM(interrupt_enable, SPIx, IER, TXPIE)
            )
            {
                interrupt_event = SPIInterruptEvent_data_needs_to_be_written;
            }



            // The last bit of the last word has been sent out. Note that
            // this could lead to a potential issue where we disable and
            // reenable the SPI peripheral before the last bit is sampled
            // properly by the receiver. The most robust workaround this is
            // probably the one suggested by the errata where we insert a
            // delay between EOT and the disabling of SPI; however, this
            // issue is pretty niche, and most of the time does not manifest.
            // If data corrupt crops up and we find this to be the cause, we
            // can implement a delay here somewhere.
            // @/pg 26/sec 2.16.2/`H533er`.

            else if (CMSIS_GET_FROM(interrupt_status, SPI, SR, EOT))
            {
                interrupt_event = SPIInterruptEvent_end_of_transfer;
            }



            switch (driver->master.state)
            {

                // Nothing to do until the user schedules a transfer.

                case SPIDriverMasterState_standby:
                {

                    if (interrupt_event)
                        panic;

                    if (interrupt_status & interrupt_enable)
                        panic;

                    return SPIUpdateOnce_yield;

                } break;



                // The user has a transfer they want to do.

                case SPIDriverMasterState_scheduled_transfer:
                {

                    if (interrupt_event)
                        panic;

                    if (interrupt_status & interrupt_enable)
                        panic;

                    if (CMSIS_GET(SPIx, CR1, CSTART))
                        panic;

                    // @/pg 2382/tbl 569/`H533rm`.
                    if (!(0 <= driver->master.amount && driver->master.amount <= 65535))
                        panic;



                    // We have to disable and renable the SPI peripheral
                    // between every transaction since this is the way to
                    // "restart the internal state machine properly".
                    // @/pg 2413/sec 52.42.12/`H533rm`.

                    CMSIS_SET(SPIx, CR1, SPE, false);
                    {
                        CMSIS_SET(SPIx, CR2, TSIZE, driver->master.amount);
                    }
                    CMSIS_SET(SPIx, CR1, SPE, true);



                    CMSIS_SET
                    (
                        SPIx , IER , // Enable interrupts for:
                        OVRIE, true, //     - Overrun error.
                        UDRIE, true, //     - Underrun error.
                        EOTIE, true, //     - End of transfer.
                        TXPIE, true, //     - FIFO has space for a packet to be sent; gets cleared when TXTF happens.
                        RXPIE, true, //     - FIFO received a packet to be read.
                    );



                    // Begin the transfer!

                    CMSIS_SET(SPIx, CR1, CSTART, true);

                    driver->master.state = SPIDriverMasterState_transferring;

                    return SPIUpdateOnce_again;

                } break;



                // We're in the process of doing the transfer.

                case SPIDriverMasterState_transferring: switch (interrupt_event)
                {

                    // Nothing to do until there's an interrupt status.

                    case SPIInterruptEvent_none:
                    {

                        if (interrupt_status & interrupt_enable)
                            panic;

                        return SPIUpdateOnce_yield;

                    } break;



                    // Pop a word from the RX-FIFO.

                    case SPIInterruptEvent_data_available_to_read:
                    {

                        if (!(0 <= driver->master.rx_progress && driver->master.rx_progress < driver->master.amount))
                            panic;

                        driver->master.rx_buffer[driver->master.rx_progress]  = *(u8*) &SPIx->RXDR;
                        driver->master.rx_progress                           += 1;

                        return SPIUpdateOnce_again;

                    } break;



                    // Push a word into the TX-FIFO.

                    case SPIInterruptEvent_data_needs_to_be_written:
                    {

                        if (!(0 <= driver->master.tx_progress && driver->master.tx_progress < driver->master.amount))
                            panic;

                        *(u8*) &SPIx->TXDR          = driver->master.tx_buffer[driver->master.tx_progress];
                        driver->master.tx_progress += 1;



                        // Once we have finished pushing all the data for the transfer
                        // into the FIFO, the TXTF interrupt flag gets asserted and the
                        // TXP interrupt automatically gets disabled.

                        if (driver->master.tx_progress == driver->master.amount)
                        {

                            if (!CMSIS_GET(SPIx, SR, TXTF))
                                panic; // Should've been asserted.

                            if (CMSIS_GET(SPIx, IER, TXPIE))
                                panic; // Should've been cleared.

                            CMSIS_SET(SPIx, IFCR, TXTFC, true); // Clear the asserted TXTF flag.

                        }



                        return SPIUpdateOnce_again;

                    } break;



                    // We can now conclude the SPI transfer.

                    case SPIInterruptEvent_end_of_transfer:
                    {

                        CMSIS_SET(SPIx, IFCR, EOTC, true); // Clear the flag.

                        if (driver->master.rx_progress != driver->master.amount)
                            panic;

                        if (driver->master.tx_progress != driver->master.amount)
                            panic;

                        driver->master.state = SPIDriverMasterState_standby;

                        return SPIUpdateOnce_again;

                    } break;



                    default: panic;

                } break;



                default: panic;

            }

        } break;



        case SPIRole_slave_receiver:
        {

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

                    if (driver->slave_receiver.writer - driver->slave_receiver.reader < countof(driver->slave_receiver.buffer))
                    {
                        i32 index = driver->slave_receiver.writer % countof(driver->slave_receiver.buffer);
                        driver->slave_receiver.buffer[index]  = data;
                        driver->slave_receiver.writer        += 1;
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
