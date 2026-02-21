#define SD_PROFILER_ENABLE true
#define DEMO_MODE          2 // See below for different kinds of tests.

#include "system.h"
#include "uxart.c"
#include "timekeeping.c"
#include "sd.c"
#include "filesystem.c"



static Sector cluster_buffer[64] = {0};



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
                return true;
            } break;

            case SDDoResult_transfer_error:
            case SDDoResult_card_likely_unmounted:
            case SDDoResult_unsupported_card:
            case SDDoResult_maybe_bus_problem:
            case SDDoResult_voltage_check_failed:
            case SDDoResult_could_not_ready_card:
            case SDDoResult_card_glitch:
            case SDDoResult_bug:
            default:
            {
                SD_reinit(SDHandle_primary);
                return false;
            } break;

        }

    }
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
                            .handle              = SDHandle_primary,
                            .writing             = false,
                            .consecutive_caching = true,
                            .sector              = &sector,
                            .address             = address,
                            .count               = 1,
                        }
                    );

                stlink_tx("\n[0x%08X]\n", address);
                SD_profiler_report();

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
                        try_doing_transfer
                        (
                            (struct SDDoJob)
                            {
                                .handle              = SDHandle_primary,
                                .writing             = false,
                                .consecutive_caching = false,
                                .sector              = &sector,
                                .address             = address,
                                .count               = 1,
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
                                    .handle              = SDHandle_primary,
                                    .writing             = true,
                                    .consecutive_caching = true,
                                    .sector              = (Sector*) { cluster_buffer },
                                    .address             = address + (u32) cluster_index * countof(cluster_buffer),
                                    .count               = sectors_in_cluster,
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
                                    .handle              = SDHandle_primary,
                                    .writing             = false,
                                    .consecutive_caching = true,
                                    .sector              = (Sector*) { cluster_buffer },
                                    .address             = address + (u32) cluster_index * countof(cluster_buffer),
                                    .count               = sectors_in_cluster,
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

                SD_profiler_report();
                GPIO_TOGGLE(led_green);
                spinlock_nop(50'000'000);

                continue;
                STRESS_FAILED:;

                SD_profiler_report();
                stlink_tx("Failed!\n");
                spinlock_nop(200'000'000);

            }

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Test the file-system.
        // Data on the SD card will be overwritten.
        //

        case 2:
        {

            FRESULT fatfs_error = {0};



            // Format the SD card with the default file-system.

            fatfs_error =
                f_mkfs
                (
                    "",
                    nullptr,
                    (Sector) {0},
                    sizeof(Sector)
                );

            if (fatfs_error)
                panic;



            // Mount the file-system.

            FATFS fatfs_system_handle = {0};

            fatfs_error =
                f_mount
                (
                    &fatfs_system_handle,
                    "", // "The string without drive number means the default drive."
                    1   // "1: Force mounted the volume to check if it is ready to work."
                );

            if (fatfs_error)
                panic;



            // Stress test the file-system.

            i32 file_count = 0;

            while (true)
            {

                if (stlink_rx(&(u8) {0}))
                {
                    for(;;);
                }



                // Create a new file.

                {

                    FIL file_handle = {0};

                    char file_name[32] = {0};
                    snprintf_(file_name, countof(file_name), "my_file_%d.txt", file_count);

                    fatfs_error =
                        f_open
                        (
                            &file_handle,
                            file_name,
                            FA_CREATE_NEW
                        );

                    if (fatfs_error)
                        panic;

                    fatfs_error = f_close(&file_handle);

                    if (fatfs_error)
                        panic;

                }

                file_count += 1;



                // Write new data to every file so far.

                #define DUMB_HASH(A, B, C, D)                                 \
                    ((u8) (((((((((((u32) (A)) * 0x9E37'79B1) ^ ((u32) (B)))  \
                                               * 0x9E37'79B1) ^ ((u32) (C)))  \
                                               * 0x9E37'79B1) ^ ((u32) (D)))  \
                                               * 0x9E37'79B1) >> 24) & 0xFF))

                for (i32 file_index = 0; file_index < file_count; file_index += 1)
                {



                    // Open the file.

                    FIL file_handle = {0};

                    char file_name[32] = {0};
                    snprintf_(file_name, countof(file_name), "my_file_%d.txt", file_index);

                    fatfs_error =
                        f_open
                        (
                            &file_handle,
                            file_name,
                            FA_WRITE
                        );

                    if (fatfs_error)
                        panic;



                    // Write a bunch of sectors.

                    UINT bytes_written = {0};

                    for (i32 cluster_index = 0; cluster_index < file_count; cluster_index += 1)
                    {
                        for (i32 sector_index = 0; sector_index < countof(cluster_buffer); sector_index += 1)
                        {
                            for (i32 byte_index = 0; byte_index < countof(cluster_buffer[sector_index]); byte_index += 1)
                            {
                                cluster_buffer[sector_index][byte_index] =
                                    DUMB_HASH
                                    (
                                        file_count + file_index,
                                        cluster_index,
                                        sector_index,
                                        byte_index
                                    );
                            }
                        }

                        fatfs_error =
                            f_write
                            (
                              &file_handle,
                              cluster_buffer,
                              sizeof(cluster_buffer),
                              &bytes_written
                            );

                        if (fatfs_error)
                            panic;

                        if (bytes_written != sizeof(cluster_buffer))
                            panic;

                    }



                    // Close the file.

                    fatfs_error = f_close(&file_handle);

                    if (fatfs_error)
                        panic;

                    SD_profiler_report();
                    GPIO_TOGGLE(led_green);

                }



                // Read in all of the file data we should be expecting.

                for (i32 file_index = 0; file_index < file_count; file_index += 1)
                {



                    // Open the file.

                    FIL file_handle = {0};

                    char file_name[32] = {0};
                    snprintf_(file_name, countof(file_name), "my_file_%d.txt", file_index);

                    fatfs_error =
                        f_open
                        (
                            &file_handle,
                            file_name,
                            FA_READ
                        );

                    if (fatfs_error)
                        panic;



                    // Check the data.

                    i32 amount_of_data_remaining = file_count * sizeof(cluster_buffer);
                    i32 cluster_index            = 0;

                    while (true)
                    {

                        UINT bytes_read = {0};

                        fatfs_error =
                            f_read
                            (
                              &file_handle,
                              cluster_buffer,
                              sizeof(cluster_buffer),
                              &bytes_read
                            );

                        if (fatfs_error)
                            panic;

                        amount_of_data_remaining -= (i32) bytes_read;

                        if (amount_of_data_remaining <= -1)
                            panic;

                        if (bytes_read)
                        {

                            if (bytes_read != sizeof(cluster_buffer))
                                panic;

                            for (i32 sector_index = 0; sector_index < countof(cluster_buffer); sector_index += 1)
                            {
                                for (i32 byte_index = 0; byte_index < countof(cluster_buffer[sector_index]); byte_index += 1)
                                {

                                    u8 digest =
                                        DUMB_HASH
                                        (
                                            file_count + file_index,
                                            cluster_index,
                                            sector_index,
                                            byte_index
                                        );

                                    if (cluster_buffer[sector_index][byte_index] != digest)
                                        panic;

                                }
                            }

                            cluster_index += 1;

                        }
                        else
                        {

                            if (amount_of_data_remaining)
                                panic;

                            break;
                        }

                    }



                    // Close the file.

                    fatfs_error = f_close(&file_handle);

                    if (fatfs_error)
                        panic;

                    SD_profiler_report();
                    GPIO_TOGGLE(led_green);

                }

            }

        } break;



        default: panic;

    }

}
