#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "buzzer.c"
#include "ssd1306.c"



////////////////////////////////////////////////////////////////////////////////



static RingBuffer(struct MainFlightComputerDebugPacket, 16) display_packets                 = {0};
static RingBuffer(struct MainFlightComputerDebugPacket, 16) logger_packets                  = {0};
static volatile _Atomic u32                                 most_recent_packet_timestamp_us = {0};



////////////////////////////////////////////////////////////////////////////////



#define panic panic_()
static noret void
panic_(void)
{

    vTaskSuspendAll(); // Let the below pattern play out entirely.
    WATCHDOG_KICK();   // "
    __disable_irq();   // "

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

    enum MainFlightComputerDebugStatusFlag first_status_flag               = 0;
    u32                                    status_flag_timestamp_us        = TIMEKEEPING_microseconds();
    u32                                    last_bad_condition_timestamp_us = TIMEKEEPING_microseconds();

    while (true)
    {



        // Get information about the latest debug packet we have so far.

        struct MainFlightComputerDebugPacket packet_data  = {0};
        b32                                  packet_exist = RingBuffer_pop_to_latest(&display_packets, &packet_data);

        u32 observed_most_recent_packet_timestamp_us =
            atomic_load_explicit
            (
                &most_recent_packet_timestamp_us,
                memory_order_relaxed // No synchronization needed.
            );



        // Indicate that we might be disconnected from the debugged-device.

        b32 havent_received_a_packet_in_a_while = TIMEKEEPING_microseconds() - observed_most_recent_packet_timestamp_us >= 3'000'000;

        if (havent_received_a_packet_in_a_while && !BUZZER_current_tune())
        {
            BUZZER_play(BuzzerTune_hazard);
            GPIO_TOGGLE  (led_channel_red_C  );
            GPIO_INACTIVE(led_channel_green_C);
            GPIO_INACTIVE(led_channel_blue_C );
        }



        // Update framebuffer.

        memset(SSD1306_framebuffer, 0, sizeof(*SSD1306_framebuffer));

        SSD1306_write_format
        (
            0,
            0,
            "DB-T  %7.2f s" "\n"
            "MFC-T %7.2f s" "\n"
            "SCB-A %7.2f V" "\n"
            "SCB-B %7.2f V" "\n",
            (f32) TIMEKEEPING_microseconds() / 1000.0f / 1000.0f,
            packet_exist ? (f32) packet_data.timestamp_us / 1000.0f / 1000.0f : NAN,
            packet_exist ? packet_data.solarboard_voltages[0]                 : NAN,
            packet_exist ? packet_data.solarboard_voltages[1]                 : NAN
        );

        GPIO_INACTIVE(led_channel_red_B  );
        GPIO_ACTIVE  (led_channel_green_B);
        GPIO_INACTIVE(led_channel_blue_B );

        for (i32 i = 0; i < 4; i += 1) // Display as many system statuses as we can on the little screen.
        {

            enum MainFlightComputerDebugStatusFlag flag = (first_status_flag + i) % MainFlightComputerDebugStatusFlag_COUNT;

            const char* condition = {0};

            if (!packet_exist) // Still haven't gotten the first packet yet.
            {
                condition = "???";

                GPIO_ACTIVE(led_channel_red_B  );
                GPIO_ACTIVE(led_channel_green_B);
                GPIO_ACTIVE(led_channel_blue_B );

            }
            else if (packet_data.flags & (1 << flag)) // Debugged-device reports that this system is working.
            {
                condition = "OK";
            }
            else // Debugged-device reports that a particular system is not working.
            {

                condition = "NOPE";

                GPIO_ACTIVE  (led_channel_red_B  );
                GPIO_INACTIVE(led_channel_green_B);
                GPIO_INACTIVE(led_channel_blue_B );

                if (TIMEKEEPING_microseconds() - last_bad_condition_timestamp_us >= 5'000'000 && !BUZZER_current_tune())
                {
                    last_bad_condition_timestamp_us = TIMEKEEPING_microseconds();
                    BUZZER_play(BuzzerTune_deep_beep);
                }

            }

            SSD1306_write_format
            (
                0,
                4 + i,
                "%-10s %s",
                MainFlightComputerDebugStatusFlag_TABLE[flag].name,
                condition
            );

        }

        if (TIMEKEEPING_microseconds() - status_flag_timestamp_us >= 500'000) // Scroll the statuses slowly over time.
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

        WATCHDOG_KICK();

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(logger, 0)
{

    static struct Sector working_sectors[2]         = {0};
    b32                  completely_wipe_filesystem = false;

    for (;;)
    {

        REINITIALIZE:;



        // If need be, we can format the SD card. Data saved on the debug
        // board SD card is not critical as it's only for diagnostic purposes
        // for things like sequence testing.

        if (completely_wipe_filesystem)
        {
            BUZZER_play(BuzzerTune_tetris);
            GPIO_ACTIVE(led_channel_red_D  );
            GPIO_ACTIVE(led_channel_green_D);
            GPIO_ACTIVE(led_channel_blue_D );
        }
        else
        {
            GPIO_ACTIVE  (led_channel_red_D  );
            GPIO_INACTIVE(led_channel_green_D);
            GPIO_ACTIVE  (led_channel_blue_D );
        }



        // Try setting up the file-system.

        enum FileSystemReinitResult reinit_result =
            FILESYSTEM_reinit
            (
                SDHandle_primary,
                working_sectors,
                completely_wipe_filesystem ? countof(working_sectors) : 0
            );

        completely_wipe_filesystem = false;

        GPIO_INACTIVE(led_channel_red_D  );
        GPIO_INACTIVE(led_channel_green_D);
        GPIO_INACTIVE(led_channel_blue_D );

        switch (reinit_result)
        {



            // Ready to go!

            case FileSystemReinitResult_success:
            {
                GPIO_INACTIVE(led_channel_red_D  );
                GPIO_ACTIVE  (led_channel_green_D);
                GPIO_INACTIVE(led_channel_blue_D );
            } break;



            // Couldn't successfully communicate with the SD card,
            // likely because of poor connection or there's no card mounted.
            // Either way, we indicate this as a soft error.

            case FileSystemReinitResult_couldnt_ready_card:
            case FileSystemReinitResult_transfer_error:
            {

                for (i32 i = 0; i < 32; i += 1)
                {
                    GPIO_TOGGLE  (led_channel_red_D  );
                    GPIO_INACTIVE(led_channel_green_D);
                    GPIO_TOGGLE  (led_channel_blue_D );
                    FREERTOS_delay_ms(25);
                }

                FREERTOS_delay_ms(1'000);

                goto REINITIALIZE;

            } break;



            // Something went wrong trying to mount the file-system;
            // best thing we can do is format the card and hope it'll work out.

            case FileSystemReinitResult_invalid_filesystem:
            case FileSystemReinitResult_no_more_space_for_new_file:
            case FileSystemReinitResult_fatfs_internal_error:
            case FileSystemReinitResult_bug:
            default:
            {

                for (i32 i = 0; i < 128; i += 1)
                {
                    GPIO_TOGGLE(led_channel_red_D  );
                    GPIO_TOGGLE(led_channel_green_D);
                    GPIO_TOGGLE(led_channel_blue_D );
                    FREERTOS_delay_ms(25);
                }

                FREERTOS_delay_ms(1'000);

                completely_wipe_filesystem = true;

                goto REINITIALIZE;

            } break;

        }



        // Alright, file-system is ready. Let's start logging!

        while (true)
        {



            // Make the log entry.

            struct MainFlightComputerDebugPacket packet_data = {0};

            if (RingBuffer_pop(&logger_packets, &packet_data))
            {

                i32 log_entry_length = 0;

                {

                    i32 append_length =
                        snprintf_
                        (
                            ((char*) working_sectors) + log_entry_length,
                            (u32) (sizeof(working_sectors) - log_entry_length),
                            "DB-T       : %7.2f s" "\n"
                            "MFC-T      : %7.2f s" "\n"
                            "SCB-A      : %7.2f V" "\n"
                            "SCB-B      : %7.2f V" "\n",
                            (f32) TIMEKEEPING_microseconds() / 1000.0f / 1000.0f,
                            (f32) packet_data.timestamp_us / 1000.0f / 1000.0f,
                            packet_data.solarboard_voltages[0],
                            packet_data.solarboard_voltages[1]
                        );

                    if (append_length <= -1)
                    {
                        sus;
                    }
                    else
                    {

                        log_entry_length += append_length;

                        if (log_entry_length > sizeof(working_sectors))
                        {
                            sus;
                            log_entry_length = sizeof(working_sectors);
                        }

                    }

                }

                for (enum MainFlightComputerDebugStatusFlag flag = {0}; flag < MainFlightComputerDebugStatusFlag_COUNT; flag += 1)
                {

                    i32 append_length =
                        snprintf_
                        (
                            ((char*) working_sectors) + log_entry_length,
                            (u32) (sizeof(working_sectors) - log_entry_length),
                            "%-10s : %s%s" "\n",
                            MainFlightComputerDebugStatusFlag_TABLE[flag].name,
                            (packet_data.flags & (1 << flag))
                                ? "OK"
                                : "NOPE",
                            flag + 1 == MainFlightComputerDebugStatusFlag_COUNT
                                ? "\n"
                                : ""
                        );

                    if (append_length <= -1)
                    {
                        sus;
                    }
                    else
                    {

                        log_entry_length += append_length;

                        if (log_entry_length > sizeof(working_sectors))
                        {
                            sus;
                            log_entry_length = sizeof(working_sectors);
                        }

                    }

                }



                // The remaining space in the log buffer will be used for the divider.

                memset
                (
                    (u8*) working_sectors + log_entry_length,
                    '.',
                    (u32) (sizeof(working_sectors) - log_entry_length)
                );

                working_sectors[countof(working_sectors) - 1].bytes[sizeof(struct Sector) - 2] = '\n'; // Don't care if this stamps over the string.
                working_sectors[countof(working_sectors) - 1].bytes[sizeof(struct Sector) - 1] = '\n'; // "



                // Save onto SD card for us to verify the vehicle
                // is operating as intended after sequence testing.

                enum FileSystemSaveResult save_result =
                    FILESYSTEM_save
                    (
                        SDHandle_primary,
                        working_sectors,
                        countof(working_sectors)
                    );

                switch (save_result)
                {



                    // Successfully saved the log entry;
                    // periodically indicate that we're logging data too.

                    case FileSystemSaveResult_success:
                    {
                        GPIO_INACTIVE(led_channel_red_D  );
                        GPIO_TOGGLE  (led_channel_green_D);
                        GPIO_INACTIVE(led_channel_blue_D );
                    } break;



                    // Small error occured; let's just reinitialize the file-system
                    // as quickly as possible to be able to continue logging, if still possible.

                    case FileSystemSaveResult_transfer_error:
                    {
                        goto REINITIALIZE;
                    } break;



                    // Uh oh, let's just wipe the SD card so we can continue logging new data.

                    case FileSystemSaveResult_no_more_space_for_data:
                    case FileSystemSaveResult_fatfs_internal_error:
                    {
                        completely_wipe_filesystem = true;
                        goto REINITIALIZE;
                    } break;



                    // Something weird happened. Let's just reinitialize
                    // the file-system and hope for the best...

                    case FileSystemSaveResult_bug:
                    default:
                    {
                        goto REINITIALIZE;
                    } break;

                }

            }

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(buzzer_indicator, 0)
{
    for (;;)
    {

        while (BUZZER_current_tune())
        {

            GPIO_ACTIVE  (led_channel_red_A  );
            GPIO_INACTIVE(led_channel_green_A);
            GPIO_INACTIVE(led_channel_blue_A );
            FREERTOS_delay_ms(20);

            GPIO_ACTIVE  (led_channel_red_A  );
            GPIO_ACTIVE  (led_channel_green_A);
            GPIO_INACTIVE(led_channel_blue_A );
            FREERTOS_delay_ms(20);

            GPIO_INACTIVE(led_channel_red_A  );
            GPIO_ACTIVE  (led_channel_green_A);
            GPIO_INACTIVE(led_channel_blue_A );
            FREERTOS_delay_ms(20);

            GPIO_INACTIVE(led_channel_red_A  );
            GPIO_ACTIVE  (led_channel_green_A);
            GPIO_ACTIVE  (led_channel_blue_A );
            FREERTOS_delay_ms(20);

            GPIO_INACTIVE(led_channel_red_A  );
            GPIO_INACTIVE(led_channel_green_A);
            GPIO_ACTIVE  (led_channel_blue_A );
            FREERTOS_delay_ms(20);

            GPIO_ACTIVE  (led_channel_red_A  );
            GPIO_INACTIVE(led_channel_green_A);
            GPIO_ACTIVE  (led_channel_blue_A );
            FREERTOS_delay_ms(20);

        }

        GPIO_INACTIVE(led_channel_red_A  );
        GPIO_INACTIVE(led_channel_green_A);
        GPIO_INACTIVE(led_channel_blue_A );

        FREERTOS_delay_ms(100);

    }
}



////////////////////////////////////////////////////////////////////////////////



INTERRUPT_I2Cx_communication(enum I2CSlaveCallbackEvent event, u8* data)
{

    static i32                                  byte_index = 0;
    static struct MainFlightComputerDebugPacket packet     = {0};

    switch (event)
    {



        case I2CSlaveCallbackEvent_data_available_to_read:
        {

            if (byte_index < sizeof(packet))
            {
                ((u8*) &packet)[byte_index] = *data;
            }

            byte_index += 1;

        } break;



        case I2CSlaveCallbackEvent_master_initiates_write:
        case I2CSlaveCallbackEvent_master_repeats_write:
        case I2CSlaveCallbackEvent_stop_signaled:
        {

            if (byte_index != sizeof(packet))
            {
                // Either we missed some received bytes, or too many bytes were sent.
            }
            else
            {

                u8 digest = DEBUG_BOARD_calculate_crc((u8*) &packet, sizeof(packet));

                if (digest) // Checksum mismatch?
                {
                    GPIO_ACTIVE  (led_channel_red_C  );
                    GPIO_INACTIVE(led_channel_green_C);
                    GPIO_INACTIVE(led_channel_blue_C );
                }
                else
                {

                    // Alright, we actually got valid debug data packet.
                    // Queue it up for processing.

                    atomic_store_explicit
                    (
                        &most_recent_packet_timestamp_us,
                        TIMEKEEPING_microseconds(),
                        memory_order_relaxed // No synchronization needed.
                    );

                    if (!RingBuffer_push(&display_packets, &packet))
                    {
                        // Ring-buffer full! Should be fine though...
                    }

                    if (!RingBuffer_push(&logger_packets, &packet))
                    {
                        // Ring-buffer full! Should be fine though...
                    }

                    GPIO_INACTIVE(led_channel_red_C  );
                    GPIO_TOGGLE  (led_channel_green_C);
                    GPIO_INACTIVE(led_channel_blue_C );

                }

            }

            byte_index = 0;
            memzero(&packet);

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
