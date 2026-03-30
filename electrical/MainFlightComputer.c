#define WATCHDOG_ENABLE                  true
#define ALLOW_FILESYSTEM_TO_BE_FORMATTED true

#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"



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



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////



enum DiagnosticLEDBehavior : u32
{
    DiagnosticLEDBehavior_inactive,
    DiagnosticLEDBehavior_active,
    DiagnosticLEDBehavior_toggle,
};

#include "MainFlightComputer_diagnostics.meta"
/* #meta

    DIAGNOSTICS = ( # Ordered from highest priority to lowest priority.
        ('watchdog_reset'   , ('toggle'  , 'inactive', 'inactive'), 200, 3000),
        ('wiping_filesystem', ('toggle'  , 'toggle'  , 'toggle'  ),  25, 4000),
        ('logging_mishap'   , ('toggle'  , 'inactive', 'toggle'  ),  25,  250),
        ('esp32_mishap'     , ('inactive', 'inactive', 'toggle'  ),  50,  500),
        ('logging_heartbeat', ('inactive', 'active'  , 'inactive'),  25,   25),
    )

    Meta.enums('DiagnosticMask', 'u32', (
        (name, f'1 << {diagnostic_i}')
        for diagnostic_i, (name, (behavior_red, behavior_green, behavior_blue), delay_ms, duration_ms) in enumerate(DIAGNOSTICS)
    ))

    Meta.lut('DIAGNOSTIC_TABLE', ((
        (
            ('behavior_red'  , f'DiagnosticLEDBehavior_{behavior_red  }'),
            ('behavior_green', f'DiagnosticLEDBehavior_{behavior_green}'),
            ('behavior_blue' , f'DiagnosticLEDBehavior_{behavior_blue }'),
            ('delay_ms'      , f'{delay_ms}U'                           ),
            ('duration_ms'   , f'{duration_ms}U'                        ),
        )
        for diagnostic_i, (name, (behavior_red, behavior_green, behavior_blue), delay_ms, duration_ms) in enumerate(DIAGNOSTICS)
    )))

*/

FREERTOS_TASK(diagnostics, 1)
{

    u32 current_flags = 0;

    for (;;)
    {



        // Check and wait for there to be a new diagnostic to handle.

        b32 waiting = false;

        do
        {

            u32 new_flags = {0};

            BaseType_t got_notification =
                xTaskNotifyWait
                (
                    0,
                    (u32) -1, // Immediately acknowledge the new diagnostics.
                    (uint32_t*) &new_flags,
                    waiting ? 100 : 0
                );

            if (got_notification)
            {
                current_flags |= new_flags;
            }

            waiting = true;

        }
        while (!current_flags);



        // Handle the highest priority diagnostic with blinking LEDs.

        enum DiagnosticMask current_diagnostic = current_flags & -current_flags;

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

        if (current_diagnostic && __builtin_ctz(current_diagnostic) < countof(DIAGNOSTIC_TABLE))
        {

            auto info = &DIAGNOSTIC_TABLE[__builtin_ctz(current_diagnostic)];

            u32 start_timestamp_us = TIMEKEEPING_microseconds();

            while (TIMEKEEPING_microseconds() - start_timestamp_us < info->duration_ms * 1000)
            {



                // Control the LEDs.

                switch (info->behavior_red)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_red); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_red); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_red); break;
                    default                             : sus;
                }

                switch (info->behavior_green)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_green); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_green); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_green); break;
                    default                             : sus;
                }

                switch (info->behavior_blue)
                {
                    case DiagnosticLEDBehavior_inactive : GPIO_INACTIVE(led_channel_blue); break;
                    case DiagnosticLEDBehavior_active   : GPIO_ACTIVE  (led_channel_blue); break;
                    case DiagnosticLEDBehavior_toggle   : GPIO_TOGGLE  (led_channel_blue); break;
                    default                             : sus;
                }



                // See if there's a new higher priority diagnostic we should handle.

                u32 new_flags = 0;

                BaseType_t got_notification =
                    xTaskNotifyWait
                    (
                        0,
                        (u32) -1, // Immediately acknowledge the new diagnostics.
                        (uint32_t*) &new_flags,
                        0
                    );

                if (got_notification)
                {
                    current_flags |= new_flags; // Add the new diagnostics to the list of existing ones to process.
                }

                enum DiagnosticMask new_diagnostic = current_flags & -current_flags;

                if (new_diagnostic == current_diagnostic)
                {

                    // Although `xTaskNotifyWait` has a time-out argument that could be used
                    // as a delay that can also be interrupted early when there's a notification,
                    // it's possible that a task set the same diagnostic repeatedly over and
                    // over again to which a delay never really happens. By using a regular
                    // task delay here, we're making things slightly less responsive because
                    // the delay won't allow for the `diagnostics` task to handle a new
                    // diagnostic of higher priority, but since this is literally just for
                    // diagnostics, it's okay.

                    FREERTOS_delay_ms(info->delay_ms);

                }
                else
                {
                    break; // There's a new diagnostic of higher priority that we should handle.
                }

            }

        }
        else
        {
            sus; // Invalid diagnostic..?
        }

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

        current_flags &= ~current_diagnostic; // Acknowledge the completion of the diagnostic.

    }

}



////////////////////////////////////////////////////////////////////////////////



static volatile _Atomic u32 most_recent_logging_timestamp_us = 0;

FREERTOS_TASK(watchdog, 3)
{

#if WATCHDOG_ENABLE

    // The main flight computer is fine to be resetted
    // as there's nothing critical to keep online throughout
    // the entire mission. A reset will result in a gap
    // in the data collection, but if it means potentially
    // recovering from a bugged scenario, it's worthwhile to
    // nonethless try. The same cannot be said about the vehicle
    // because it runs off of battery power which is controlled
    // by the vehicle flight computer, so if the MCU resets,
    // the vehicle powers off entirely.

    for (;;)
    {

        FREERTOS_delay_ms(1'000);

        // Currently the only condition for a watchdog reset
        // would be if we couldn't do any data logging in a while.
        // We could also reset if we haven't received any ESP32 data
        // in a while, but that'd be much more likely be due to the
        // vehicle going out of range rather than any hardware issues
        // on the main side.

        u32 observed_most_recent_logging_timestamp_us =
            atomic_load_explicit
            (
                &most_recent_logging_timestamp_us,
                memory_order_relaxed // No synchronization needed.
            );

        u32 current_timestamp_us = TIMEKEEPING_microseconds();

        if (current_timestamp_us - observed_most_recent_logging_timestamp_us >= 15'000'000) // Should be long enough to account for reformatting.
        {

            xTaskNotify
            (
                diagnostics_handle,
                DiagnosticMask_watchdog_reset,
                eSetBits
            );

            FREERTOS_delay_ms(3'000);

            WARM_RESET();

        }



        // Serves as an additional layer of recovery in we for
        // whatever reason ended up in a locked-up situation.

        WATCHDOG_KICK();

    }

#else

    for (;;)
    {
        FREERTOS_delay_ms(1'000);
    }

#endif

}



////////////////////////////////////////////////////////////////////////////////



static useret b32
esp32_get_byte(u8* dst)
{

    u32 start_timestamp_us = TIMEKEEPING_microseconds();

    u32 timeout_duration_us =
        start_timestamp_us < 10'000'000
            ? 10'000'000  // We'd like to give the vehicle extra time when we are first powering on.
            :  3'000'000; // Any other time, we'd like to reset the ESP32 more frequently.

    b32 got_byte = {0};

    while (true)
    {

        u32 current_timestamp_us = TIMEKEEPING_microseconds();

        if (UXART_rx(UXARTHandle_esp32, dst))
        {
            got_byte = true; // Got what we needed.
            break;
        }
        else if (current_timestamp_us - start_timestamp_us < timeout_duration_us)
        {
            FREERTOS_delay_ms(1); // Hmm, just give the ESP32 a moment...
        }
        else
        {
            got_byte = false; // The ESP32's been quiet... maybe it's bricked..?
            break;
        }

    }

    return got_byte;

}

static useret enum ESP32GetPacketResult : u32
{
    ESP32GetPacketResult_esp32,
    ESP32GetPacketResult_lora,
    ESP32GetPacketResult_timeout,
    ESP32GetPacketResult_crc_mismatch,
}
esp32_get_packet(struct ESP32Packet* dst_packet)
{

    if (!dst_packet)
        sus;

    memzero(dst_packet);



    // First find the starting token of the payload.

    i32                       esp32_match_length = 0;
    i32                       lora_match_length  = 0;
    enum ESP32GetPacketResult kind               = {0};

    while (true)
    {

        u8 data = {0};

        if (!esp32_get_byte(&data))
        {
            return ESP32GetPacketResult_timeout;
        }

        if (ESP32_TOKEN_START[esp32_match_length] != data) // Reset the search for the ESP32 token?
        {
            esp32_match_length = 0;
        }

        if (ESP32_TOKEN_START[esp32_match_length] == data)
        {

            esp32_match_length += 1;

            if (ESP32_TOKEN_START[esp32_match_length] == '\0')
            {
                kind = ESP32GetPacketResult_esp32;
                break; // Found the ESP32 start token!
            }

        }

        if (LORA_TOKEN_START[lora_match_length] != data) // Reset the search for the LoRa token?
        {
            lora_match_length = 0;
        }

        if (LORA_TOKEN_START[lora_match_length] == data)
        {

            lora_match_length += 1;

            if (LORA_TOKEN_START[lora_match_length] == '\0')
            {
                kind = ESP32GetPacketResult_lora;
                break; // Found the LoRa start token!
            }

        }

    }



    // Get the payload data based on the token.

    u8* write_pointer = kind == ESP32GetPacketResult_esp32 ? (u8*) dst_packet    : (u8*) &dst_packet->nonredundant;
    i32 write_size    = kind == ESP32GetPacketResult_esp32 ? sizeof(*dst_packet) : sizeof(dst_packet->nonredundant);

    for (i32 i = 0; i < write_size; i += 1)
    {
        if (!esp32_get_byte(&write_pointer[i]))
        {
            return ESP32GetPacketResult_timeout;
        }
    }



    // Verify integrity of the payload.

    u8 digest = ESP32_calculate_crc(write_pointer, write_size);

    if (digest)
    {
        return ESP32GetPacketResult_crc_mismatch;
    }
    else
    {
        return kind;
    }

}

FREERTOS_TASK(esp32, 0)
{
    for (;;)
    {

        // Reset the ESP32 for a bit...

        GPIO_ACTIVE(esp32_reset);
        FREERTOS_delay_ms(10);



        // Reboot our UART communication...

        UXART_reinit(UXARTHandle_esp32);



        // Reawaken the ESP32!

        FREERTOS_delay_ms(10);
        GPIO_INACTIVE(esp32_reset);



        // Begin parsing incoming packets.

        b32 need_to_reinitialize       = false;
        i32 consecutive_crc_mismatches = 0;

        while (!need_to_reinitialize)
        {

            struct ESP32Packet        packet = {0};
            enum ESP32GetPacketResult result = esp32_get_packet(&packet);

            switch (result)
            {



                // Got full ESP-NOW packet data.

                case ESP32GetPacketResult_esp32:
                {

                    consecutive_crc_mismatches = 0;

                    #if 0
                    {
                        stlink_tx
                        (
                            "QuatX                      : %f"    "\n"
                            "QuatY                      : %f"    "\n"
                            "QuatZ                      : %f"    "\n"
                            "QuatS                      : %f"    "\n"
                            "MagX                       : %f"    "\n"
                            "MagY                       : %f"    "\n"
                            "MagZ                       : %f"    "\n"
                            "AccelX                     : %f"    "\n"
                            "AccelY                     : %f"    "\n"
                            "AccelZ                     : %f"    "\n"
                            "GyroX                      : %f"    "\n"
                            "GyroY                      : %f"    "\n"
                            "GyroZ                      : %f"    "\n"
                            "timestamp_ms               : %u ms" "\n"
                            "rolling_sequence_number    : %u"    "\n"
                            "computer_vision_confidence : %u"    "\n"
                            "image_sequence_number      : %u"    "\n"
                            "\n",
                            packet.nonredundant.QuatX,
                            packet.nonredundant.QuatY,
                            packet.nonredundant.QuatZ,
                            packet.nonredundant.QuatS,
                            packet.MagX,
                            packet.MagY,
                            packet.MagZ,
                            packet.nonredundant.AccelX,
                            packet.nonredundant.AccelY,
                            packet.nonredundant.AccelZ,
                            packet.nonredundant.GyroX,
                            packet.nonredundant.GyroY,
                            packet.nonredundant.GyroZ,
                            packet.nonredundant.timestamp_ms,
                            packet.nonredundant.rolling_sequence_number,
                            packet.nonredundant.computer_vision_confidence,
                            packet.image_sequence_number
                        );
                    }
                    #elif 1
                    {

                        if (packet.image_sequence_number == 1) // @/`ESP32 Sequence Numbers`.
                        {
                            stlink_tx(TV_TOKEN_END);
                            stlink_tx(TV_TOKEN_START);
                        }

                        if (packet.image_sequence_number) // @/`ESP32 Sequence Numbers`.
                        {
                            UXART_tx_bytes(UXARTHandle_stlink, packet.image_bytes, countof(packet.image_bytes));
                        }

                    }
                    #endif
                } break;



                // Got the small LoRa packet data.

                case ESP32GetPacketResult_lora:
                {

                    consecutive_crc_mismatches = 0;

                    sorry

                } break;



                // Hmm, maybe the ESP32 is bricked?

                case ESP32GetPacketResult_timeout:
                {
                    need_to_reinitialize = true;
                } break;



                // Bad signal or perhaps the UART RX-FIFO overflowed...

                case ESP32GetPacketResult_crc_mismatch:
                {

                    consecutive_crc_mismatches += 1;

                    if (consecutive_crc_mismatches > 8)
                    {
                        need_to_reinitialize = true; // Too many errors... suspicious!
                    }
                    else // Hmm, maybe nothing to really worry yet.
                    {
                        xTaskNotify
                        (
                            diagnostics_handle,
                            DiagnosticMask_esp32_mishap,
                            eSetBits
                        );
                    }

                } break;



                // Weird.

                default:
                {
                    sus;
                    need_to_reinitialize = true;
                } break;

            }

        }



        xTaskNotify
        (
            diagnostics_handle,
            DiagnosticMask_esp32_mishap,
            eSetBits
        );

        FREERTOS_delay_ms(500);

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(vehicle_interface, 0)
{
    for (;;)
    {

        I2C_partial_reinit(I2CHandle_vehicle_interface);

        b32 need_to_reinitialize = false;

        while (!need_to_reinitialize)
        {

            struct VehicleInterfacePayload payload = {0};

            struct I2CDoJob job =
                {
                    .handle       = I2CHandle_vehicle_interface,
                    .address_type = I2CAddressType_seven,
                    .address      = VEHICLE_INTERFACE_SEVEN_BIT_ADDRESS,
                    .writing      = false,
                    .repeating    = false,
                    .pointer      = (u8*) &payload,
                    .amount       = sizeof(payload)
                };

            for (b32 yield = false; !yield;)
            {

                enum I2CDoResult result = I2C_do(&job);

                switch (result)
                {

                    case I2CDoResult_working:
                    {
                        FREERTOS_delay_ms(1); // We'll keep on spin-locking until the transfer is done...
                    } break;

                    case I2CDoResult_success:
                    {

                        // TODO CRC.

                        #if 1
                        {
                            stlink_tx
                            (
                                "timestamp_us   : %u"   "\n"
                                "stepper_issues : %d"   "\n"
                                "vn100_issues   : %d"   "\n"
                                "openmv_issues  : %d"   "\n"
                                "esp32_issues   : %d"   "\n"
                                "\n",
                                payload.timestamp_us,
                                payload.stepper_issues,
                                payload.vn100_issues,
                                payload.openmv_issues,
                                payload.esp32_issues
                            );
                        }
                        #endif

                        yield = true;

                    } break;

                    case I2CDoResult_no_acknowledge:
                    case I2CDoResult_clock_stretch_timeout:
                    case I2CDoResult_bus_misbehaved:
                    case I2CDoResult_watchdog_expired:
                    case I2CDoResult_bug:
                    default:
                    {
                        need_to_reinitialize = true;
                        yield                = true;
                    } break;

                }

            }

            FREERTOS_delay_ms(100);

        }

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(logger, 0)
{

    static union
    {
        struct Sector formatting[8];
        struct Sector logging[2];
    } sectors = {0};

    #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
    #define WIPE_FILESYSTEM_THRESHOLD 16
    i32 completely_wipe_filesystem_tick = 0;
    #endif

    for (;;)
    {

        REINITIALIZE:;



        // Try setting up the file-system.

        #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
        {
            if (completely_wipe_filesystem_tick >= WIPE_FILESYSTEM_THRESHOLD)
            {
                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_wiping_filesystem,
                    eSetBits
                );
            }
        }
        #endif

        enum FileSystemReinitResult reinit_result =
            FILESYSTEM_reinit
            (
                SDHandle_primary,
                sectors.formatting,
                #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
                (
                    completely_wipe_filesystem_tick >= WIPE_FILESYSTEM_THRESHOLD
                        ? countof(sectors.formatting)
                        : 0
                )
                #else
                (
                    0
                )
                #endif
            );

        #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
        {
            if (completely_wipe_filesystem_tick >= WIPE_FILESYSTEM_THRESHOLD)
            {
                completely_wipe_filesystem_tick = 0;
            }
        }
        #endif

        switch (reinit_result)
        {



            // Ready to go!

            case FileSystemReinitResult_success:
            {

                completely_wipe_filesystem_tick = 0;

                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_logging_heartbeat,
                    eSetBits
                );

            } break;



            // Couldn't successfully communicate with the SD card,
            // likely because of poor connection or there's no card mounted.
            // Either way, we indicate this as a soft error.

            case FileSystemReinitResult_couldnt_ready_card:
            case FileSystemReinitResult_transfer_error:
            {

                xTaskNotify
                (
                    diagnostics_handle,
                    DiagnosticMask_logging_mishap,
                    eSetBits
                );

                goto REINITIALIZE;

            } break;



            // Something went wrong trying to mount the file-system;
            // best thing we can do is format the card and hope it'll work out.

            case FileSystemReinitResult_no_more_space_for_new_file:
            case FileSystemReinitResult_invalid_filesystem:
            case FileSystemReinitResult_fatfs_internal_error:
            {

                #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
                {
                    completely_wipe_filesystem_tick += 1;
                }
                #endif

                goto REINITIALIZE;

            } break;



            // Something weird happened.

            case FileSystemReinitResult_bug:
            default:
            {
                goto REINITIALIZE;
            } break;

        }



        // Alright, file-system is ready. Let's start logging!

        u32 most_recent_heartbeat_timestamp_us  = 0;
        u32 most_recent_stlink_log_timestamp_us = 0;

        while (true)
        {

            // Make the log entry.

            i32 log_entry_length =
                snprintf_
                (
                    (char*) sectors.logging,
                    sizeof(sectors.logging),
                    "[%lu us]"            "\n"
                    "Something here : %s" "\n"
                    "\n",
                    TIMEKEEPING_microseconds(),
                    "meow!"
                );



            // Formatting error, if for some reason.

            if (log_entry_length <= -1)
            {
                sus;
                log_entry_length = 0;
            }



            // Log entry too long; just truncate.

            if (log_entry_length > sizeof(sectors.logging))
            {
                sus;
                log_entry_length = sizeof(sectors.logging);
            }



            // The remaining space in the log buffer will be used for the divider.

            memset
            (
                (u8*) sectors.logging + log_entry_length,
                '.',
                (u32) (sizeof(sectors.logging) - log_entry_length)
            );

            sectors.logging[countof(sectors.logging) - 1].bytes[sizeof(struct Sector) - 2] = '\n'; // Don't care if this stamps over the string.
            sectors.logging[countof(sectors.logging) - 1].bytes[sizeof(struct Sector) - 1] = '\n'; // "



            // Save onto SD card for us to verify the vehicle
            // is operating as intended after sequence testing.

            enum FileSystemSaveResult save_result =
                FILESYSTEM_save
                (
                    SDHandle_primary,
                    sectors.logging,
                    countof(sectors.logging)
                );

            switch (save_result)
            {



                // Successfully saved the log entry;
                // periodically indicate that we're logging data too.

                case FileSystemSaveResult_success:
                {

                    atomic_store_explicit
                    (
                        &most_recent_logging_timestamp_us,
                        TIMEKEEPING_microseconds(),
                        memory_order_relaxed // No synchronization necessary.
                    );

                    if (TIMEKEEPING_microseconds() - most_recent_heartbeat_timestamp_us >= 1'000'000)
                    {

                        most_recent_heartbeat_timestamp_us = TIMEKEEPING_microseconds();

                        xTaskNotify
                        (
                            diagnostics_handle,
                            DiagnosticMask_logging_heartbeat,
                            eSetBits
                        );

                    }

                } break;



                // Some sort of error happened; let's just reinitialize the file-system
                // as quickly as possible to be able to continue logging, if still possible.

                case FileSystemSaveResult_transfer_error:
                case FileSystemSaveResult_no_more_space_for_data:
                case FileSystemSaveResult_fatfs_internal_error:
                case FileSystemSaveResult_bug:
                default:
                {

                    xTaskNotify
                    (
                        diagnostics_handle,
                        DiagnosticMask_logging_mishap,
                        eSetBits
                    );

                    goto REINITIALIZE;

                } break;

            }

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(debug_board, 0)
{
    for (;;)
    {

        I2C_partial_reinit(I2CHandle_debug_board);

        b32 need_to_reinitialize = false;

        while (!need_to_reinitialize)
        {

            struct MainFlightComputerDebugPacket packet =
                {
                    .timestamp_us           = TIMEKEEPING_microseconds(),
                    .solarboard_voltages[0] = 67, // TODO.
                    .solarboard_voltages[1] = 69, // TODO.
                    .flags                  = 0,  // TODO.
                };

            packet.crc = DEBUG_BOARD_calculate_crc((u8*) &packet, sizeof(packet) - sizeof(packet.crc));

            struct I2CDoJob job =
                {
                    .handle       = I2CHandle_debug_board,
                    .address_type = I2CAddressType_seven,
                    .address      = MFC_DEBUG_BOARD_SEVEN_BIT_ADDRESS,
                    .writing      = true,
                    .repeating    = false,
                    .pointer      = (u8*) &packet,
                    .amount       = sizeof(packet)
                };

            for (b32 yield = false; !yield;)
            {

                enum I2CDoResult result = I2C_do(&job);

                switch (result)
                {

                    case I2CDoResult_working:
                    {
                        FREERTOS_delay_ms(1); // We'll keep on spin-locking until the transfer is done...
                    } break;

                    case I2CDoResult_success:
                    {
                        yield = true;
                    } break;

                    case I2CDoResult_no_acknowledge:
                    case I2CDoResult_clock_stretch_timeout:
                    case I2CDoResult_bus_misbehaved:
                    case I2CDoResult_watchdog_expired:
                    case I2CDoResult_bug:
                    default:
                    {
                        need_to_reinitialize = true;
                        yield                = true;
                    } break;

                }

            }

            FREERTOS_delay_ms(100);

        }

    }
}
