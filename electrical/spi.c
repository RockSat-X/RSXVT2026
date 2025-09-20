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
};



struct SPIDriver
{
    volatile enum SPIDriverState state;
};



static struct SPIDriver _SPI_drivers[SPIHandle_COUNT] = {0};



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
        SPIx   , CFG1                    , // TODO Review.
        BPASS  , STPY_SPIx_BYPASS_DIVIDER, // Whether or not the divider is needed.
        MBR    , STPY_SPIx_DIVIDER       , // Divider to generate the baud.
        DSIZE  , 8 - 1                   , // Amount of bits in a single chunk of data.
        FTHLV  , 1 - 1                   , // Amount of chunks of data that the FIFO considers as a packet.
    );

    CMSIS_SET
    (
        SPIx   , CFG2 , // TODO Review.
        MASTER , true , // SPI in master mode.
        SSOE   , true , // Enable the Slave-Select pin.
        SSIOP  , false, // SS  pin level during inactivity.
        CPOL   , false, // SCK pin level during inactivity.
        CPHA   , false, // Capture data on the second clock transistion?
        LSBFRST, false, // LSb transmitted first?
        AFCNTR , true , // The pins' alternate functions are always enabled even the SPI is disabled.
        MSSI   , 15   , // Amount of delay (in SPI clock cycles) to wait after enabling the slave (safe assumption that the slave is slow).
    );

    CMSIS_SET(SPIx, CR2, TSIZE, 6); // TODO.



    // TMP.

    CMSIS_SET(SPIx, CR1, SPE   , false);
    CMSIS_SET(SPIx, CR1, SPE   , true );
    CMSIS_SET(SPIx, CR1, CSTART, true );
    CMSIS_SET
    (
        SPIx , IER , // Enable interrupts for:
        OVRIE, true, //     - Overrun error.
        UDRIE, true, //     - Underrun error.
        EOTIE, true, //     - End of transfer.
        TXPIE, true, //     - FIFO has space for a packet to be sent; gets cleared when TXTF happens.
        RXPIE, true, //     - FIFO received a packet to be read.
    );

}



static void
_SPI_update_entirely(enum SPIHandle handle)
{

    _EXPAND_HANDLE

    u32 spi_sr = SPIx->SR;

    // Data overrun/underrun error... somehow?
    if
    (
        CMSIS_GET_FROM(spi_sr, SPIx, SR, OVR) ||
        CMSIS_GET_FROM(spi_sr, SPIx, SR, UDR)
    )
    {
        sorry
    }

    // A packet is available to read from the FIFO?
    else if (CMSIS_GET_FROM(spi_sr, SPIx, SR, RXP))
    {
        (void) *(u8*) &SPIx->RXDR;
    }

    // A packet of data can be written to the FIFO (and should be written)?
    else if
    (
        CMSIS_GET_FROM(spi_sr, SPIx, SR, TXP) &&
        CMSIS_GET(SPIx, IER, TXPIE)
    )
    {
        *(u8*) &SPIx->TXDR = 0xAB;
    }

    // All data has been sent/received for the transfer?
    else if (CMSIS_GET_FROM(spi_sr, SPI, SR, EOT))
    {
        sorry
    }

}
