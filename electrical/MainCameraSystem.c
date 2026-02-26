#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "ovcam.c"



////////////////////////////////////////////////////////////////////////////////



pack_push
    struct ImageChapter
    {
        u8  ending_token[sizeof(TV_TOKEN_END) - 1];
        u32 sequence_number;
        u32 image_size;
        u32 image_timestamp_us;
        u32 cycle_count;
        u8  padding[512 - (sizeof(TV_TOKEN_END) - 1) - (sizeof(TV_TOKEN_START) - 1) - 4 * sizeof(u32)];
        u8  starting_token[sizeof(TV_TOKEN_START) - 1];
    } __attribute__((aligned(4)));
pack_pop

static_assert(sizeof(struct ImageChapter) == sizeof(Sector));



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Heartbeat.
//



FREERTOS_TASK(blinker, 256, 0)
{
    for (;;)
    {
        GPIO_TOGGLE(led_channel_green);
        vTaskDelay(250);
    }
}
