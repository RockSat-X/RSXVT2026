#define ALLOW_FILESYSTEM_TO_BE_FORMATTED true

#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "ovcam.c"



////////////////////////////////////////////////////////////////////////////////



static noret void
ERROR_FATAL(void)
{

    WATCHDOG_kick(); // Let the below pattern play out entirely.

    for (i32 i = 0; i < 16; i += 1)
    {

        GPIO_ACTIVE  (led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );
        spinlock_us(50'000);

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );
        spinlock_us(50'000);

    }

    WARM_RESET();

}



////////////////////////////////////////////////////////////////////////////////



static void
try_reinitializing_ovcam(void)
{
    for (i32 attempts = 0;; attempts += 1)
    {

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_ACTIVE  (led_channel_blue );

        enum OVCAMReinitResult result = OVCAM_reinit();

        GPIO_INACTIVE(led_channel_red  );
        GPIO_INACTIVE(led_channel_green);
        GPIO_INACTIVE(led_channel_blue );

        switch (result)
        {

            case OVCAMReinitResult_success:
            {
                return; // Everything's all good.
            } break;

            case OVCAMReinitResult_failed_to_initialize_with_i2c:
            {
                if (attempts < 16)
                {
                    spinlock_us(50'000); // Perhaps poor connection; we'll try again.
                }
                else
                {
                    ERROR_FATAL(); // Something's wrong with the camera module...
                }
            } break;

            case OVCAMReinitResult_bug : ERROR_FATAL();
            default                    : ERROR_FATAL();

        }

    }
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



    // Set up the file-system.

    WATCHDOG_kick();
    {

        i32 attempts                   = 0;
        b32 completely_wipe_filesystem = false;

        for (b32 yield = false; !yield;)
        {

            attempts += 1;

            if (completely_wipe_filesystem)
            {
                for (i32 i = 0; i < 64; i += 1) // Indicate that we're about to do something destructive...
                {

                    GPIO_INACTIVE(led_channel_red  );
                    GPIO_INACTIVE(led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );
                    spinlock_us(25'000);

                    GPIO_ACTIVE(led_channel_red  );
                    GPIO_ACTIVE(led_channel_green);
                    GPIO_ACTIVE(led_channel_blue );
                    spinlock_us(25'000);

                }
            }
            else // Indicate that we're in the process of mounting the file-system.
            {
                GPIO_ACTIVE  (led_channel_red  );
                GPIO_INACTIVE(led_channel_green);
                GPIO_ACTIVE  (led_channel_blue );
            }

            enum FileSystemReinitResult reinit_result =
                FILESYSTEM_reinit
                (
                    SDHandle_primary,
                    &(Sector) {0},
                    !!completely_wipe_filesystem
                );

            GPIO_INACTIVE(led_channel_red  );
            GPIO_INACTIVE(led_channel_green);
            GPIO_INACTIVE(led_channel_blue );

            switch (reinit_result)
            {

                case FileSystemReinitResult_success:
                {
                    yield = true; // We're ready to log some data!
                } break;

                case FileSystemReinitResult_couldnt_ready_card:
                case FileSystemReinitResult_transfer_error:
                {
                    spinlock_us(25'000); // Maybe it was a fluke; let's try again...
                } break;

                case FileSystemReinitResult_invalid_filesystem:
                case FileSystemReinitResult_no_more_space_for_new_file:
                case FileSystemReinitResult_fatfs_internal_error:
                {
                    #if ALLOW_FILESYSTEM_TO_BE_FORMATTED
                    {
                        if (attempts < 256)
                        {
                            // Let's recheck to be SUPER sure we actually need to wipe the SD card...
                        }
                        else
                        {

                            if (!completely_wipe_filesystem)
                            {
                                WATCHDOG_kick();
                            }

                            completely_wipe_filesystem = true;

                        }
                    }
                    #else
                    {
                        // Nothing we can honestly do...
                    }
                    #endif
                } break;

                case FileSystemReinitResult_bug : ERROR_FATAL();
                default                         : ERROR_FATAL();

            }

        }

    }
    WATCHDOG_kick();



    // Set up the camera module.

    try_reinitializing_ovcam();



    // Capture images and save 'em.

    u32 image_index = 0;

    for (;;)
    {

        enum OVCAMSwapFramebufferResult result = OVCAM_swap_framebuffer();

        switch (result)
        {



            // No issues with the camera module so far.

            case OVCAMSwapFramebufferResult_attempted:
            {
                if (OVCAM_current_framebuffer)
                {

                    WATCHDOG_kick();



                    // We first insert some meta-data about the image.

                    pack_push

                        union ImageMetadata
                        {
                            Sector sector;
                            struct
                            {
                                u8  ending_token[sizeof(TV_TOKEN_END) - 1]; // @/`Image Metadata Tokens`.
                                u32 image_index;
                                u32 image_size;
                                u32 image_timestamp_us;
                                u32 cpu_cycle_counter;
                                u8  padding[512 - (sizeof(TV_TOKEN_END) - 1) - (sizeof(TV_TOKEN_START) - 1) - 4 * sizeof(u32)];
                                u8  starting_token[sizeof(TV_TOKEN_START) - 1]; // @/`Image Metadata Tokens`.

                            };
                        };

                    pack_pop

                    union ImageMetadata metadata =
                        {
                            .ending_token       = TV_TOKEN_END,
                            .image_index        = image_index,
                            .image_size         = (u32) OVCAM_current_framebuffer->length,
                            .image_timestamp_us = OVCAM_current_framebuffer->timestamp_us,
                            .cpu_cycle_counter  = CPU_CYCLE_COUNTER(),
                            .starting_token     = TV_TOKEN_START,
                        };

                    {

                        static_assert(sizeof(metadata) == sizeof(Sector));

                        enum FileSystemSaveResult save_result =
                            FILESYSTEM_save
                            (
                                SDHandle_primary,
                                &metadata.sector,
                                1
                            );

                        switch (save_result)
                        {

                            case FileSystemSaveResult_success:
                            {
                                // All good saving the meta-data.
                            } break;

                            case FileSystemSaveResult_transfer_error         : ERROR_FATAL();
                            case FileSystemSaveResult_no_more_space_for_data : ERROR_FATAL();
                            case FileSystemSaveResult_fatfs_internal_error   : ERROR_FATAL();
                            case FileSystemSaveResult_bug                    : ERROR_FATAL();
                            default                                          : ERROR_FATAL();

                        }

                    }



                    // Now actually save the image data.
                    //
                    // Note that we round the image data up the nearest sector size.
                    // This is to keep read/writes to the SD card sector-aligned, as
                    // writing data not in multiple of sectors is more complicated and
                    // doesn't actually really offer any performance benefits. We waste
                    // a bit of space, but that's fine. Whatever data is after the image
                    // in the framebuffer will be leftover data from the previous image
                    // that was in that framebuffer slot, so it's pretty much garbage we
                    // should ignore when we parse the image.

                    {

                        enum FileSystemSaveResult save_result =
                            FILESYSTEM_save
                            (
                                SDHandle_primary,
                                (Sector*) OVCAM_current_framebuffer->data,
                                (OVCAM_current_framebuffer->length + sizeof(Sector) - 1) / sizeof(Sector)
                            );

                        switch (save_result)
                        {

                            case FileSystemSaveResult_success:
                            {
                                // All good saving the meta-data.
                            } break;

                            case FileSystemSaveResult_transfer_error         : ERROR_FATAL();
                            case FileSystemSaveResult_no_more_space_for_data : ERROR_FATAL();
                            case FileSystemSaveResult_fatfs_internal_error   : ERROR_FATAL();
                            case FileSystemSaveResult_bug                    : ERROR_FATAL();
                            default                                          : ERROR_FATAL();

                        }

                    }



                    // Heart-beat.

                    GPIO_INACTIVE(led_channel_red  );
                    GPIO_TOGGLE  (led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );

                    image_index += 1;

                }
            } break;



            // Something bad happened with the camera module, so we'll go ahead and reinitialize it.

            case OVCAMSwapFramebufferResult_too_many_bad_jpeg_frames:
            case OVCAMSwapFramebufferResult_timeout:
            {
                try_reinitializing_ovcam();
            } break;



            // Catastrophic error!

            case OVCAMSwapFramebufferResult_bug : ERROR_FATAL();
            default                             : ERROR_FATAL();

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`Image Metadata Tokens`:
//
// The fact that the image meta-data header begins with the TV end token
// and ends with the TV start token is to allow for the `tv` verb to be
// able to parse the logged data as if it was image data that was streamed
// from `DemoOVCAM`. It also furthermore denotes the start and (rough) end
// of an image frame.
