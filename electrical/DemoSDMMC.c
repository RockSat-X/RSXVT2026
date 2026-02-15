#define SD_PROFILER_ENABLE true

#include "system.h"
#include "uxart.c"
#include "timekeeping.c"
#include "sd.c"
#include "filesystem.c"



#define DEMO_MODE 1 // See below for different kinds of tests.



static i32 count_successful            = 0; // TODO Metatize.
static i32 count_task_error            = 0; // "
static i32 count_card_likely_unmounted = 0; // "
static i32 count_unsupported_card      = 0; // "
static i32 count_maybe_bus_problem     = 0; // "
static i32 count_voltage_check_failed  = 0; // "
static i32 count_could_not_ready_card  = 0; // "
static i32 count_card_glitch           = 0; // "
static i32 count_bugs                  = 0; // "

static useret b32
try_doing_operation
(
    enum SDHandle    handle,
    enum SDOperation operation,
    Sector*          sector,
    u32              address
)
{



    // Do the operation.

    enum SDDoResult do_result = SD_do(handle, operation, sector, address);



    // Interpret the results.

    switch (do_result)
    {

        case SDDoResult_success:
        {
            count_successful += 1;
        } break;

        case SDDoResult_task_error:
        {

            count_task_error += 1;

            SD_reinit(SDHandle_primary);

        } break;

        {

            case SDDoResult_card_likely_unmounted : count_card_likely_unmounted += 1; goto SD_DRIVER_ERROR;
            case SDDoResult_unsupported_card      : count_unsupported_card      += 1; goto SD_DRIVER_ERROR;
            case SDDoResult_maybe_bus_problem     : count_maybe_bus_problem     += 1; goto SD_DRIVER_ERROR;
            case SDDoResult_voltage_check_failed  : count_voltage_check_failed  += 1; goto SD_DRIVER_ERROR;
            case SDDoResult_could_not_ready_card  : count_could_not_ready_card  += 1; goto SD_DRIVER_ERROR;
            case SDDoResult_card_glitch           : count_card_glitch           += 1; goto SD_DRIVER_ERROR;
            SD_DRIVER_ERROR:

            SD_reinit(SDHandle_primary);

        } break;

        case SDDoResult_bug:
        default:
        {
            count_bugs += 1;
            SD_reinit(SDHandle_primary);
        } break;

    }

    return do_result == SDDoResult_success;

}

static void
print_stats(void)
{

    stlink_tx
    (
        "count_successful            : %d" "\n"
        "count_task_error            : %d" "\n"
        "count_card_likely_unmounted : %d" "\n"
        "count_unsupported_card      : %d" "\n"
        "count_maybe_bus_problem     : %d" "\n"
        "count_voltage_check_failed  : %d" "\n"
        "count_could_not_ready_card  : %d" "\n"
        "count_card_glitch           : %d" "\n"
        "count_bugs                  : %d" "\n",
        count_successful,
        count_task_error,
        count_card_likely_unmounted,
        count_unsupported_card,
        count_maybe_bus_problem,
        count_voltage_check_failed,
        count_could_not_ready_card,
        count_card_glitch,
        count_bugs
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
                    try_doing_operation
                    (
                        SDHandle_primary,
                        SDOperation_single_read,
                        &sector,
                        address
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
                        try_doing_operation
                        (
                            SDHandle_primary,
                            SDOperation_single_read,
                            &sector,
                            address
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

                for (u32 sector_index = 0; sector_index < amount_of_sectors_to_test; sector_index += 1)
                {

                    Sector sector = {0};

                    for (u32 byte_index = 0; byte_index < countof(sector); byte_index += 1)
                    {
                        sector[byte_index] += (u8) (counter * (sector_index + 1) + byte_index);
                    }

                    b32 success =
                        try_doing_operation
                        (
                            SDHandle_primary,
                            SDOperation_single_write,
                            &sector,
                            address + sector_index
                        );

                    if (!success)
                    {
                        goto STRESS_FAILED;
                    }

                }



                // Read in the sectors with data we should be expecting.

                for (u32 sector_index = 0; sector_index < amount_of_sectors_to_test; sector_index += 1)
                {

                    Sector sector = {0};

                    b32 success =
                        try_doing_operation
                        (
                            SDHandle_primary,
                            SDOperation_single_read,
                            &sector,
                            address + sector_index
                        );

                    if (!success)
                    {
                        goto STRESS_FAILED;
                    }

                    for (u32 byte_index = 0; byte_index < countof(sector); byte_index += 1)
                    {
                        if (sector[byte_index] != (u8) (counter * (sector_index + 1) + byte_index))
                            panic;
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

            /* Append a line */
            f_printf(&fil, "%02u/%02u/%u, %2u:%02u\n", 1, 2, 3, 4, 5);

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
