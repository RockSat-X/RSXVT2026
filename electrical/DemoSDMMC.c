#define SD_PROFILER_ENABLE true
#define DEMO_MODE          2 // See below for different kinds of tests.

#include "system.h"
#include "uxart.c"
#include "timekeeping.c"
#include "sd.c"
#include "filesystem.c"



static i32 count_success               = 0;
static i32 count_transfer_error        = 0;
static i32 count_card_likely_unmounted = 0;
static i32 count_unsupported_card      = 0;
static i32 count_maybe_bus_problem     = 0;
static i32 count_voltage_check_failed  = 0;
static i32 count_could_not_ready_card  = 0;
static i32 count_card_glitch           = 0;
static i32 count_bug                   = 0;

static useret b32
try_doing_transfer(struct SDDoJob job)
{

    while (true)
    {

        enum SDDoResult do_result = SD_do(&job);

        switch (do_result)
        {

            case SDDoResult_still_initializing:
            {
                // SD driver still initializing the SD card.
            } break;

            case SDDoResult_working:
            {
                // The job is being handled...
            } break;

            case SDDoResult_success:
            {
                count_success += 1;
                return true;
            } break;

            {

                case SDDoResult_transfer_error        : count_transfer_error        += 1; goto SD_ERROR;
                case SDDoResult_card_likely_unmounted : count_card_likely_unmounted += 1; goto SD_ERROR;
                case SDDoResult_unsupported_card      : count_unsupported_card      += 1; goto SD_ERROR;
                case SDDoResult_maybe_bus_problem     : count_maybe_bus_problem     += 1; goto SD_ERROR;
                case SDDoResult_voltage_check_failed  : count_voltage_check_failed  += 1; goto SD_ERROR;
                case SDDoResult_could_not_ready_card  : count_could_not_ready_card  += 1; goto SD_ERROR;
                case SDDoResult_card_glitch           : count_card_glitch           += 1; goto SD_ERROR;
                case SDDoResult_bug                   : count_bug                   += 1; goto SD_ERROR;
                default                               : count_bug                   += 1; goto SD_ERROR;
                SD_ERROR:

                SD_reinit(SDHandle_primary);

                return false;

            } break;

        }

    }

}

static void
print_stats(void)
{

    stlink_tx
    (
        "count_success               : %d" "\n"
        "count_transfer_error        : %d" "\n"
        "count_card_likely_unmounted : %d" "\n"
        "count_unsupported_card      : %d" "\n"
        "count_maybe_bus_problem     : %d" "\n"
        "count_voltage_check_failed  : %d" "\n"
        "count_could_not_ready_card  : %d" "\n"
        "count_card_glitch           : %d" "\n"
        "count_bug                   : %d" "\n",
        count_success,
        count_transfer_error,
        count_card_likely_unmounted,
        count_unsupported_card,
        count_maybe_bus_problem,
        count_voltage_check_failed,
        count_could_not_ready_card,
        count_card_glitch,
        count_bug
    );

    SD_profiler_report();

}



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    SD_reinit(SDHandle_primary);

    {

        // Set the prescaler that'll affect all timers' kernel frequency.

        CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



        // Enable the peripheral.

        CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



        // Configure the divider to set the rate at
        // which the timer's counter will increment.

        CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



        // Trigger an update event so that the shadow registers
        // are what we initialize them to be.
        // The hardware uses shadow registers in order for updates
        // to these registers not result in a corrupt timer output.

        CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



        // Enable the timer's counter.

        CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);

    }

    switch (DEMO_MODE)
    {



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Do some basic reads. Data on the SD card will not be tampereed with.
        //

        case 0:
        {

            for (u32 address = 0;; address += 1)
            {

                Sector sector = {0};

                b32 success =
                    try_doing_transfer
                    (
                        (struct SDDoJob)
                        {
                            .handle        = SDHandle_primary,
                            .writing       = false,
                            .random_access = false,
                            .sector        = &sector,
                            .address       = address,
                            .count         = 1,
                        }
                    );

                stlink_tx("\n[0x%08X]\n", address);
                print_stats();

                if (success)
                {
                    for (i32 byte_i = 0; byte_i < 512; byte_i += 1)
                    {
                        if (byte_i % 32 == 0)
                        {
                            stlink_tx("%03d | ", byte_i);
                        }

                        stlink_tx
                        (
                            "%02X%c",
                            sector[byte_i],
                            (byte_i % 32 == 31) ? '\n' : ' '
                        );
                    }
                }



                // Bit of breather...

                GPIO_TOGGLE(led_green);
                spinlock_nop(100'000'000);

            }

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Stress test the SD card and SD API.
        // Data on the SD card will be overwritten.
        //

        case 1:
        {

            static Sector cluster_buffer[64] = {0};

            for (u32 address = 0;; address += 1)
            {

                u32 amount_of_sectors_to_test = address + 1;

                stlink_tx
                (
                    "\n[0x%08X - 0x%08X] (%d sectors)\n",
                    address,
                    address + amount_of_sectors_to_test - 1,
                    amount_of_sectors_to_test
                );



                // Read a sector that'll determine the counter's initial value.

                u8 counter = {0};

                {

                    Sector sector = {0};

                    b32 success =
                        try_doing_transfer
                        (
                            (struct SDDoJob)
                            {
                                .handle        = SDHandle_primary,
                                .writing       = false,
                                .random_access = true,
                                .sector        = &sector,
                                .address       = address,
                                .count         = 1,
                            }
                        );

                    if (!success)
                    {
                        goto STRESS_FAILED;
                    }

                    for (u32 i = 0; i < countof(sector); i += 1)
                    {
                        counter += (u8) ((sector[i] + 1) * (i + 1) | (sector[i] >> 4)); // Something strange.
                    }

                }



                // Fill out bunch of sectors with data we can predict.

                {

                    for
                    (
                        i32 cluster_index = 0;
                        cluster_index < (i32) (amount_of_sectors_to_test + countof(cluster_buffer) - 1) / countof(cluster_buffer);
                        cluster_index += 1
                    )
                    {

                        i32 sectors_in_cluster = (i32) amount_of_sectors_to_test - cluster_index * countof(cluster_buffer);

                        if (sectors_in_cluster > countof(cluster_buffer))
                        {
                            sectors_in_cluster = countof(cluster_buffer);
                        }

                        for (i32 sector_index = 0; sector_index < sectors_in_cluster; sector_index += 1)
                        {
                            for (i32 byte_index = 0; byte_index < countof(cluster_buffer[sector_index]); byte_index += 1)
                            {
                                cluster_buffer[sector_index][byte_index] = (u8) (counter + (cluster_index * countof(cluster_buffer) + sector_index) * 3 + byte_index);
                            }
                        }

                        b32 success =
                            try_doing_transfer
                            (
                                (struct SDDoJob)
                                {
                                    .handle        = SDHandle_primary,
                                    .writing       = true,
                                    .random_access = false,
                                    .sector        = (Sector*) { cluster_buffer },
                                    .address       = address + (u32) cluster_index * countof(cluster_buffer),
                                    .count         = sectors_in_cluster,
                                }
                            );

                        if (!success)
                        {
                            goto STRESS_FAILED;
                        }

                    }

                }



                // Read in the sectors with data we should be expecting.

                {

                    for
                    (
                        i32 cluster_index = 0;
                        cluster_index < (i32) (amount_of_sectors_to_test + countof(cluster_buffer) - 1) / countof(cluster_buffer);
                        cluster_index += 1
                    )
                    {

                        i32 sectors_in_cluster = (i32) amount_of_sectors_to_test - cluster_index * countof(cluster_buffer);

                        if (sectors_in_cluster > countof(cluster_buffer))
                        {
                            sectors_in_cluster = countof(cluster_buffer);
                        }

                        b32 success =
                            try_doing_transfer
                            (
                                (struct SDDoJob)
                                {
                                    .handle        = SDHandle_primary,
                                    .writing       = false,
                                    .random_access = false,
                                    .sector        = (Sector*) { cluster_buffer },
                                    .address       = address + (u32) cluster_index * countof(cluster_buffer),
                                    .count         = sectors_in_cluster,
                                }
                            );

                        if (!success)
                        {
                            goto STRESS_FAILED;
                        }

                        for (i32 sector_index = 0; sector_index < sectors_in_cluster; sector_index += 1)
                        {
                            for (i32 byte_index = 0; byte_index < countof(cluster_buffer[sector_index]); byte_index += 1)
                            {

                                u8 byte = cluster_buffer[sector_index][byte_index];

                                if (byte != (u8) (counter + (cluster_index * countof(cluster_buffer) + sector_index) * 3 + byte_index))
                                    panic;

                            }
                        }

                    }

                }



                // Bit of breather...

                print_stats();
                GPIO_TOGGLE(led_green);
                spinlock_nop(50'000'000);

                continue;
                STRESS_FAILED:;

                print_stats();
                stlink_tx("Failed!\n");
                spinlock_nop(200'000'000);

            }

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Test the file-system.
        //

        case 2:
        {

            FRESULT fr;
            FATFS   fs;
            FIL     fil;

            fr = f_mkfs("", NULL, (Sector) {0}, sizeof(Sector));
            if (fr != FR_OK) panic;

            /* Open or create a log file and ready to append */
            f_mount(&fs, "", 0);
            {
                /* Opens an existing file. If not exist, creates a new file. */
                fr = f_open(&fil, "logfile.txt", FA_WRITE | FA_OPEN_ALWAYS);
                if (fr == FR_OK) {
                    /* Seek to end of the file to append data */
                    fr = f_lseek(&fil, f_size(&fil));
                    if (fr != FR_OK)
                        f_close(&fil);
                }
            }

            if (fr != FR_OK) panic;

            u8 data[] = "what's up?";
            fr =
                f_write
                (
                  &fil,
                  data,
                  sizeof(data),
                  &(UINT) {0}
                );

            /* Close the file */
            f_close(&fil);

            for (;;)
            {
                GPIO_TOGGLE(led_green);
                spinlock_nop(100'000'000);
            }

        } break;



        default: panic;

    }

}
