#define SPI_BLOCK_SIZE 64

#include "system.h"
#include "uxart.c"
#include "spi.c"
#include "timekeeping.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Initialize stuff.
    //



    STPY_init();
    UXART_reinit(UXARTHandle_stlink);
    SPI_reinit(SPIHandle_primary);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Demonstrate SPI.
    //



    u32 watchdog_timestamp_us = TIMEKEEPING_microseconds();
    i32 block_index           = 0;

    for (;;)
    {

        SPIBlock block_data = {0};

        if (RingBuffer_pop(SPI_reception(SPIHandle_primary), &block_data))
        {

            watchdog_timestamp_us = TIMEKEEPING_microseconds();

            stlink_tx("[%d]\n", block_index);

            for (i32 i = 0; i < countof(block_data) / 16; i += 1)
            {
                for (i32 j = 0; j < 16; j += 1)
                {
                    stlink_tx("0x%02X ", block_data[i]);
                }
                stlink_tx("\n");
            }

            stlink_tx("\n");

            block_index += 1;

        }
        else if (TIMEKEEPING_microseconds() - watchdog_timestamp_us >= 5'000'000)
        {

            stlink_tx("Haven't gotten data in a while; resetting SPI driver...\n");

            SPI_reinit(SPIHandle_primary);

            watchdog_timestamp_us = TIMEKEEPING_microseconds();
            block_index           = 0;

        }

    }

}
