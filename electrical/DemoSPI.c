#define SPI_BLOCK_SIZE 64

#include "system.h"
#include "uxart.c"
#include "spi.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);
    SPI_reinit(SPIHandle_primary);

    i32 block_index = 0;

    for (;;)
    {

        SPIBlock block_data = {0};

        if (RingBuffer_pop(SPI_ring_buffer(SPIHandle_primary), &block_data))
        {

            stlink_tx("%d : ", block_index);

            for (i32 i = 0; i < countof(block_data); i += 1)
            {
                stlink_tx("0x%02X ", block_data[i]);
            }

            stlink_tx("\n");

            block_index += 1;

        }

    }

}
