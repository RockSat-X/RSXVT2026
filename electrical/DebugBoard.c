#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "buzzer.c"
#include "ssd1306.c"



////////////////////////////////////////////////////////////////////////////////



static RingBuffer(struct MainFlightComputerDebugPacket, 16) packets                         = {0};
static volatile _Atomic u32                                 most_recent_packet_timestamp_us = {0};



////////////////////////////////////////////////////////////////////////////////



#define panic panic_()
static noret void
panic_(void)
{

    vTaskSuspendAll(); // Let the below pattern play out entirely.
    WATCHDOG_KICK();   // "

    for (i32 i = 0; i < 32; i += 1)
    {

        GPIO_ACTIVE  (led_channel_red_A  );
        GPIO_INACTIVE(led_channel_green_A);
        GPIO_INACTIVE(led_channel_blue_A );
        GPIO_ACTIVE  (led_channel_red_B  );
        GPIO_INACTIVE(led_channel_green_B);
        GPIO_INACTIVE(led_channel_blue_B );
        GPIO_ACTIVE  (led_channel_red_C  );
        GPIO_INACTIVE(led_channel_green_C);
        GPIO_INACTIVE(led_channel_blue_C );
        GPIO_ACTIVE  (led_channel_red_D  );
        GPIO_INACTIVE(led_channel_green_D);
        GPIO_INACTIVE(led_channel_blue_D );
        spinlock_us(50'000);

        GPIO_INACTIVE(led_channel_red_A  );
        GPIO_INACTIVE(led_channel_green_A);
        GPIO_INACTIVE(led_channel_blue_A );
        GPIO_INACTIVE(led_channel_red_B  );
        GPIO_INACTIVE(led_channel_green_B);
        GPIO_INACTIVE(led_channel_blue_B );
        GPIO_INACTIVE(led_channel_red_C  );
        GPIO_INACTIVE(led_channel_green_C);
        GPIO_INACTIVE(led_channel_blue_C );
        GPIO_INACTIVE(led_channel_red_D  );
        GPIO_INACTIVE(led_channel_green_D);
        GPIO_INACTIVE(led_channel_blue_D );
        spinlock_us(50'000);

    }

    WARM_RESET();

}



////////////////////////////////////////////////////////////////////////////////



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Configure the other registers to get timekeeping up and going.

    TIMEKEEPING_partial_init();



    // Initialize the SSD1306 display.

    for (b32 yield = false; !yield;)
    {

        enum SSD1306ReinitResult result = SSD1306_reinit();

        switch (result)
        {

            case SSD1306ReinitResult_success:
            {
                yield = true;
            } break;

            case SSD1306ReinitResult_failed_to_initialize_with_i2c:
            {
                // Something went wrong; we'll try again.
            } break;

            case SSD1306ReinitResult_bug : panic;
            default                      : panic;

        }

    }



    // Initialize peripheral for communicating with the debugged-device.

    I2C_partial_reinit(I2CHandle_communication);



    // Configure the other registers to get the buzzer up and going.

    BUZZER_partial_init();
    BUZZER_play(BuzzerTune_nokia);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(display, 0)
{

    enum MainFlightComputerDebugStatusFlag first_status_flag        = 0;
    u32                                    status_flag_timestamp_us = TIMEKEEPING_microseconds();

    while (true)
    {



        // Get information about the latest debug packet we have so far.

        struct MainFlightComputerDebugPacket packet_data  = {0};
        b32                                  packet_exist = RingBuffer_pop_to_latest(&packets, &packet_data);

        u32 observed_most_recent_packet_timestamp_us =
            atomic_load_explicit
            (
                &most_recent_packet_timestamp_us,
                memory_order_relaxed // No synchronization needed.
            );

        b32 havent_received_a_packet_in_a_while = TIMEKEEPING_microseconds() - observed_most_recent_packet_timestamp_us >= 3'000'000;



        // Update framebuffer.

        memset(SSD1306_framebuffer, 0, sizeof(*SSD1306_framebuffer));

        SSD1306_write_format
        (
            0,
            0,
            "DB-T  %7.2f s"  "\n"
            "MFC-T %7.2f s"  "\n"
            "SCB-A %7.2f V" "\n"
            "SCB-B %7.2f V" "\n",
            (f32) TIMEKEEPING_microseconds() / 1000.0f / 1000.0f,
            packet_exist ? (f32) packet_data.timestamp_us / 1000.0f / 1000.0f : NAN,
            packet_exist ? packet_data.solarboard_voltages[0] / 100.0f        : NAN, // TODO Arbitrary right now...
            packet_exist ? packet_data.solarboard_voltages[1] / 100.0f        : NAN  // TODO Arbitrary right now...
        );

        for (i32 i = 0; i < 4; i += 1)
        {

            enum MainFlightComputerDebugStatusFlag flag = (first_status_flag + i) % MainFlightComputerDebugStatusFlag_COUNT;

            SSD1306_write_format
            (
                0,
                4 + i,
                "%-10s %s",
                MainFlightComputerDebugStatusFlag_TABLE[flag].name,
                packet_exist
                    ? packet_data.flags & (1 << flag)
                        ? "OK"
                        : "NOPE"
                    : "???"
            );

        }

        if (TIMEKEEPING_microseconds() - status_flag_timestamp_us >= 500'000)
        {
            status_flag_timestamp_us  = TIMEKEEPING_microseconds();
            first_status_flag        += 1;
            first_status_flag        %= MainFlightComputerDebugStatusFlag_COUNT;
        }



        // Swap framebuffer.

        enum SSD1306RefreshResult result =
            SSD1306_refresh
            (
                TIMEKEEPING_microseconds() / 1000 / 1000 / 2 % 2,
                !!havent_received_a_packet_in_a_while
            );

        switch (result)
        {
            case SSD1306RefreshResult_success            : break;
            case SSD1306RefreshResult_i2c_transfer_error : panic;
            case SSD1306RefreshResult_bug                : panic;
            default                                      : panic;
        }

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// TODO Check if there's enough errors to the point where we
//      should induce a reset to see if it'll fix anything.
//



FREERTOS_TASK(kicker, 0)
{
    for (;;)
    {
        WATCHDOG_KICK();
    }
}



////////////////////////////////////////////////////////////////////////////////



INTERRUPT_I2Cx_communication(enum I2CSlaveCallbackEvent event, u8* data)
{

    static i32 byte_index = 0;

    switch (event)
    {



        case I2CSlaveCallbackEvent_data_available_to_read:
        {

            struct MainFlightComputerDebugPacket* packet = RingBuffer_writing_pointer(&packets);

            if (packet)
            {

                if (byte_index < sizeof(*packet))
                {
                    ((u8*) packet)[byte_index] = *data;
                }

                byte_index += 1;

            }
            else
            {
                // No available space to write the received packet to yet.
            }

        } break;



        case I2CSlaveCallbackEvent_master_initiates_write:
        case I2CSlaveCallbackEvent_master_repeats_write:
        case I2CSlaveCallbackEvent_stop_signaled:
        {

            struct MainFlightComputerDebugPacket* packet = RingBuffer_writing_pointer(&packets);

            if (!packet)
            {
                // The buffer's full, so we definitely didn't fill out any new debug packet.
            }
            else if (byte_index != sizeof(*packet))
            {
                // Either we missed some received bytes, or too many bytes were sent.
            }
            else
            {

                u8 digest = DEBUG_BOARD_calculate_crc((u8*) packet, sizeof(*packet));

                if (digest)
                {
                    // Checksum mismatch!
                }
                else
                {

                    // Alright, we actually got valid debug data packet.
                    // Queue it up for processing.

                    if (!RingBuffer_push(&packets, nullptr))
                        panic;

                    atomic_store_explicit
                    (
                        &most_recent_packet_timestamp_us,
                        TIMEKEEPING_microseconds(),
                        memory_order_relaxed // No synchronization needed.
                    );

                }

            }

            byte_index = 0;

        } break;



        case I2CSlaveCallbackEvent_master_initiates_read  : panic; // We currently don't support read operations.
        case I2CSlaveCallbackEvent_master_repeats_read    : panic; // "
        case I2CSlaveCallbackEvent_ready_to_transmit_data : panic; // "
        case I2CSlaveCallbackEvent_clock_stretch_timeout  : panic; // Something weird happened with the I2C communication.
        case I2CSlaveCallbackEvent_bus_misbehaved         : panic; // "
        case I2CSlaveCallbackEvent_watchdog_expired       : panic; // "
        case I2CSlaveCallbackEvent_bug                    : panic; // "
        default                                           : panic; // "

    }

}
