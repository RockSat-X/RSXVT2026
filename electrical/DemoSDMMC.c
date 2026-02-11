#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"



#define DEMO_MODE 0 // See below for different kinds of tests.



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    SD_reinit(SDHandle_primary);

    switch (DEMO_MODE)
    {

        ////////////////////////////////////////////////////////////////////////////////
        //
        // Test the SD driver.
        //

        case 0:
        {

            i32 count_successful            = 0; // TODO Metatize.
            i32 count_task_error            = 0; // "
            i32 count_card_likely_unmounted = 0; // "
            i32 count_unsupported_card      = 0; // "
            i32 count_maybe_bus_problem     = 0; // "
            i32 count_voltage_check_failed  = 0; // "
            i32 count_could_not_ready_card  = 0; // "
            i32 count_card_glitch           = 0; // "
            i32 count_bugs                  = 0; // "

            for (u32 address = 0;; address += 1)
            {



                // Schedule a sector-read and block on it until it's done.

                struct Sector sector = {0};

                enum SDDoResult do_result =
                    SD_do
                    (
                        SDHandle_primary,
                        SDOperation_single_read,
                        &sector,
                        address
                    );



                // Handle the results of the sector-read.

                switch (do_result)
                {

                    case SDDoResult_success:
                    {
                        count_successful += 1; // The sector-read was a success!
                    } break;

                    case SDDoResult_task_error:
                    {
                        count_task_error += 1; // The operation failed for some reason.
                    } break;

                    {
                        case SDDoResult_card_likely_unmounted : count_card_likely_unmounted += 1; goto SD_DRIVER_ERROR;
                        case SDDoResult_unsupported_card      : count_unsupported_card      += 1; goto SD_DRIVER_ERROR;
                        case SDDoResult_maybe_bus_problem     : count_maybe_bus_problem     += 1; goto SD_DRIVER_ERROR;
                        case SDDoResult_voltage_check_failed  : count_voltage_check_failed  += 1; goto SD_DRIVER_ERROR;
                        case SDDoResult_could_not_ready_card  : count_could_not_ready_card  += 1; goto SD_DRIVER_ERROR;
                        case SDDoResult_card_glitch           : count_card_glitch           += 1; goto SD_DRIVER_ERROR;
                        SD_DRIVER_ERROR:

                        SD_reinit(SDHandle_primary); // Something went wrong; we're going have to restart the SD driver.

                    } break;

                    case SDDoResult_bug:
                    default:
                    {
                        count_bugs += 1;
                        SD_reinit(SDHandle_primary); // Something went REALLY wrong; we're going have to restart the SD driver.
                    } break;

                }



                // Output the results.

                stlink_tx
                (
                    "\n"
                    "[Sector 0x%08X]"                  "\n"
                    "count_successful            : %d" "\n"
                    "count_task_error            : %d" "\n"
                    "count_card_likely_unmounted : %d" "\n"
                    "count_unsupported_card      : %d" "\n"
                    "count_maybe_bus_problem     : %d" "\n"
                    "count_voltage_check_failed  : %d" "\n"
                    "count_could_not_ready_card  : %d" "\n"
                    "count_card_glitch           : %d" "\n"
                    "count_bugs                  : %d" "\n",
                    address,
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

                if (do_result == SDDoResult_success)
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
                            sector.data[byte_i],
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
        // Test the file-system.
        //

        case 1:
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
