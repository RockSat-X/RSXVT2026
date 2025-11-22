#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"



#define DEMO_MODE 1 // See below for different kinds of tests.



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

            i32 driver_error_count = 0;
            i32 task_error_count   = 0;

            for (u32 address = 0;; address += 1)
            {

                // Schedule a sector-read and block on it until it's done.

                struct Sector sector    = {0};
                enum SDDo     do_result = {0};

                do
                {
                    do_result =
                        SD_do
                        (
                            SDHandle_primary,
                            SDOperation_single_read,
                            &sector,
                            address
                        );
                }
                while (do_result == SDDo_task_in_progress);



                // Handle the results of the sector-read.

                switch (do_result)
                {

                    case SDDo_task_completed:
                    {
                        // The sector-read was a success!
                    } break;

                    case SDDo_task_error:
                    {

                        // The sector-read failed for some reason.
                        // To clear the error condition, we do a poll.

                        task_error_count += 1;

                        enum SDPoll poll_result = SD_poll(SDHandle_primary);

                        if (poll_result != SDPoll_cleared_task_error)
                            panic;



                        // Although not necessary, we'll
                        // reinitialize the SD driver.
                        // TODO A better approach here would
                        //      be to count back-to-back task errors
                        //      and reinitialize based on that.
                        //      The SD driver could also handle detecting
                        //      when to reinitialize the SD card on its own.

                        SD_reinit(SDHandle_primary);

                    } break;

                    case SDDo_driver_error:
                    {

                        // Something unexpected happened for the SD driver,
                        // so we'll have to reinitialize the entire driver
                        // to clear the error condition.

                        driver_error_count += 1;

                        SD_reinit(SDHandle_primary);

                    } break;

                    case SDDo_task_in_progress : panic;
                    case SDDo_bug              : panic;
                    default                    : panic;

                }



                // Output the results.

                stlink_tx
                (
                    "[Sector 0x%08X] Driver errors: %d | Task errors: %d.\n",
                    address,
                    driver_error_count,
                    task_error_count
                );

                if (do_result == SDDo_task_completed)
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

                    stlink_tx("\n");
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
