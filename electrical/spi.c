////////////////////////////////////////////////////////////////////////////////



#include "spi_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_ALIASES(
        driver_name = 'SPI',
        cmsis_name  = 'SPI',
        common_name = 'SPIx',
        identifiers = (
            'NVICInterrupt_{}',
        ),
        cmsis_tuple_tags = (
            '{}_RESET',
            '{}_ENABLE',
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

    // TODO.



    // Enable the peripheral.

    CMSIS_PUT(SPIx_ENABLE, true);



    // Configure the peripheral.

    sorry

}



static void
_SPI_update_entirely(enum SPIHandle handle)
{
    sorry
}
