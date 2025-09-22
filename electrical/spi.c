// TODO Make transmission/reception vs. tx/rx consistent with I2C.

////////////////////////////////////////////////////////////////////////////////



#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_ALIASES(
        driver_name = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        identifiers = (
            'NVICInterrupt_{}',
            'STPY_{}_KERNEL_SOURCE',
            'STPY_{}_BYPASS_DIVIDER',
            'STPY_{}_DIVIDER',
        ),
        cmsis_tuple_tags = (
            '{}_RESET',
            '{}_ENABLE',
            '{}_KERNEL_SOURCE',
        ),
    )

    for target in PER_TARGET():

        for handle, instance in target.drivers.get('SPI', ()):

            Meta.line(f'''

                static void
                _SPI_update_entirely(enum SPIHandle handle);

                INTERRUPT_{instance}
                {{
                    _SPI_update_entirely(SPIHandle_{handle});
                }}

            ''')

*/



enum SPIDriverState : u32
{
    SPIDriverState_standby,
    SPIDriverState_scheduled_transfer,
    SPIDriverState_transferring,
};




enum SPIDriverError : u32
{
    SPIDriverError_none,
};



struct SPIDriver
{
    volatile enum SPIDriverState state;
    u8*                          tx_pointer;
    u8*                          rx_pointer;
    i32                          amount;
    i32                          tx_progress;
    i32                          rx_progress;
    enum SPIDriverError          error;
};



static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum SPIDriverError
SPI_blocking_transfer
(
    enum SPIHandle handle,
    u8*            tx_pointer,
    u8*            rx_pointer,
    i32            amount
)
{

    _EXPAND_HANDLE



    // Validation.

    if (!tx_pointer)
        panic;

    if (!rx_pointer)
        panic;

    if (amount <= 0)
        panic;



    // Schedule the transfer.

    switch (driver->state)
    {
        case SPIDriverState_standby:
        {

            driver->tx_pointer  = tx_pointer;
            driver->rx_pointer  = rx_pointer;
            driver->amount      = amount;
            driver->tx_progress = 0;
            driver->rx_progress = 0;
            driver->error       = SPIDriverError_none;
            __DMB();
            driver->state       = SPIDriverState_scheduled_transfer;

            NVIC_SET_PENDING(SPIx);

        } break;

        case SPIDriverState_scheduled_transfer : panic;
        case SPIDriverState_transferring       : panic;
        default                                : panic;
    }



    // Wait until the transfer is done.

    while (true)
    {
        switch (driver->state)
        {

            // The driver just finished!

            case SPIDriverState_standby:
            {
                return driver->error;
            } break;



            // The driver is still busy with our transfer.

            case SPIDriverState_scheduled_transfer:
            case SPIDriverState_transferring:
            {
                // Keep waiting...
            } break;



            default: panic;

        }
    }

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
        SPIx   , CFG1                    ,
        BPASS  , STPY_SPIx_BYPASS_DIVIDER, // Whether or not the divider is needed.
        MBR    , STPY_SPIx_DIVIDER       , // Divider to generate the baud.
        DSIZE  , 8 - 1                   , // Bits per word.
        FTHLV  , 1 - 1                   , // Amount of words for an interrupt.
    );

    CMSIS_SET
    (
        SPIx   , CFG2 ,
        MASTER , true , // SPI in master mode.
        SSOE   , true , // Enable the Slave-Select pin.
        SSIOP  , false, // The SS  pin level during inactivity.
        CPOL   , false, // The SCK pin level during inactivity.
        CPHA   , false, // Capture data on the second clock edge?
        LSBFRST, false, // LSb transmitted first?
        AFCNTR , true , // Don't leave pins floating when SPI is disabled.
        MSSI   , 15   , // Amount of delay after asserting the slave-select.
    );

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
        SPIInterruptEvent_data_available_to_read,
        SPIInterruptEvent_data_needs_to_be_written,
        SPIInterruptEvent_end_of_transfer,
    };
    enum SPIInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = SPIx->SR;



    // Data overrun error... somehow?

    if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, OVR))
    {
        panic;
    }



    // Data underrun error... somehow?

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, UDR))
    {
        panic;
    }



    // A packet is available to read from the FIFO?

    else if (CMSIS_GET_FROM(interrupt_status, SPIx, SR, RXP))
    {
        interrupt_event = SPIInterruptEvent_data_available_to_read;
    }



    // A packet of data can be written to the FIFO (and should be written)?

    else if
    (
        CMSIS_GET_FROM(interrupt_status, SPIx, SR, TXP) &&
        CMSIS_GET(SPIx, IER, TXPIE)
    )
    {
        interrupt_event = SPIInterruptEvent_data_needs_to_be_written;
    }



    // All data has been sent/received for the transfer?

    else if (CMSIS_GET_FROM(interrupt_status, SPI, SR, EOT))
    {
        interrupt_event = SPIInterruptEvent_end_of_transfer;
    }



    switch (driver->state)
    {

        // Nothing to do until the user schedules a transfer.

        case SPIDriverState_standby:
        {

            if (interrupt_event)
                panic;

            return SPIUpdateOnce_yield;

        } break;



        // The user has a transfer they want to do.

        case SPIDriverState_scheduled_transfer:
        {

            if (interrupt_event)
                panic;


            // TODO Asserts.

            CMSIS_SET(SPIx, CR2, TSIZE , driver->amount);
            CMSIS_SET(SPIx, CR1, SPE   , false         ); // TODO Explain.
            CMSIS_SET(SPIx, CR1, SPE   , true          );
            CMSIS_SET(SPIx, CR1, CSTART, true          );
            CMSIS_SET // TODO Explain.
            (
                SPIx , IER , // Enable interrupts for:
                OVRIE, true, //     - Overrun error.
                UDRIE, true, //     - Underrun error.
                EOTIE, true, //     - End of transfer.
                TXPIE, true, //     - FIFO has space for a packet to be sent; gets cleared when TXTF happens.
                RXPIE, true, //     - FIFO received a packet to be read.
            );

            driver->state = SPIDriverState_transferring;

            return SPIUpdateOnce_again;

        } break;



        // We're in the process of doing the transfer.

        case SPIDriverState_transferring: switch (interrupt_event)
        {

            // Nothing to do until there's an interrupt status.

            case SPIInterruptEvent_none:
            {
                return SPIUpdateOnce_yield;
            } break;



            // TODO.

            case SPIInterruptEvent_data_available_to_read:
            {

                driver->rx_pointer[driver->rx_progress]  = *(u8*) &SPIx->RXDR;
                driver->rx_progress                     += 1;

                return SPIUpdateOnce_again;

            } break;



            // TODO.

            case SPIInterruptEvent_data_needs_to_be_written:
            {

                *(u8*) &SPIx->TXDR   = driver->tx_pointer[driver->tx_progress];
                driver->tx_progress += 1;



                // Once we have finished pushing all the data for the transfer
                // into the FIFO, the TXTF interrupt flag gets asserted and the
                // TXPIE interrupt automatically gets disabled.

                if (driver->tx_progress == driver->amount)
                {
                    CMSIS_SET(SPIx, IFCR, TXTFC, true); // Clear the asserted TXTF flag.
                }

                return SPIUpdateOnce_again;

            } break;



            // TODO.

            case SPIInterruptEvent_end_of_transfer:
            {

                CMSIS_SET(SPIx, IFCR, EOTC, true); // Clear the flag.

                // TODO Asserts.

                driver->state = SPIDriverState_standby;

                return SPIUpdateOnce_again;

            } break;



            default: panic;

        } break;



        default: panic;

    }

}



static void
_SPI_update_entirely(enum SPIHandle handle)
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
