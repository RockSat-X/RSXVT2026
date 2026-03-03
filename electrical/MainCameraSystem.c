#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "ovcam.c"



////////////////////////////////////////////////////////////////////////////////



static void
try_reinitializing_ovcam(void)
{
    for (i32 attempts = 0;; attempts += 1)
    {

        enum OVCAMReinitResult result = OVCAM_reinit();

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
                    // Perhaps poor connection; we'll try again.
                }
                else
                {
                    sorry // Something's wrong with the camera module...
                }
            } break;

            case OVCAMReinitResult_bug:
            default:
            {
                sorry // Something catastrophic happened...
            } break;

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

    GPIO_ACTIVE  (led_channel_red  );
    GPIO_INACTIVE(led_channel_green);
    GPIO_ACTIVE  (led_channel_blue );

    enum FileSystemReinitResult reinit_result =
        FILESYSTEM_reinit
        (
            SDHandle_primary,
            nullptr,
            0
        );

    switch (reinit_result)
    {

        case FileSystemReinitResult_success:
        {
            // We're ready to log some data.
        } break;

        case FileSystemReinitResult_couldnt_ready_card:
        {
            sorry // Perhaps no SD card is mounted?
        } break;

        case FileSystemReinitResult_transfer_error:
        {
            sorry // Perhaps poor connection? or a fluke?
        } break;

        case FileSystemReinitResult_invalid_filesystem:
        {
            sorry // TODO Format?
        } break;

        case FileSystemReinitResult_no_more_space_for_new_file:
        {
            sorry // TODO Format?
        } break;

        case FileSystemReinitResult_fatfs_internal_error:
        {
            sorry // TODO Format?
        } break;

        case FileSystemReinitResult_bug:
        default:
        {
            sorry // TODO Reset?
        } break;

    }



    // Set up the camera module.

    try_reinitializing_ovcam();



    // Capture images and save 'em.

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



                    // We first insert some meta-data about the image.

                    pack_push

                        union ImageChapter // TODO Document.
                        {
                            Sector sector;
                            struct
                            {
                                u8  ending_token[sizeof(TV_TOKEN_END) - 1];
                                u32 sequence_number;
                                u32 image_size;
                                u32 image_timestamp_us;
                                u32 cycle_count;
                                u8  padding[512 - (sizeof(TV_TOKEN_END) - 1) - (sizeof(TV_TOKEN_START) - 1) - 4 * sizeof(u32)];
                                u8  starting_token[sizeof(TV_TOKEN_START) - 1];
                            };
                        };

                    pack_pop

                    union ImageChapter chapter = // TODO Fill out.
                        {
                            .ending_token   = TV_TOKEN_END,
                            .starting_token = TV_TOKEN_START,
                        };

                    {

                        static_assert(sizeof(chapter) == sizeof(Sector));

                        enum FileSystemSaveResult save_result =
                            FILESYSTEM_save
                            (
                                SDHandle_primary,
                                &chapter.sector,
                                1
                            );

                        switch (save_result)
                        {

                            case FileSystemSaveResult_success:
                            {
                                // All good saving the meta-data.
                            } break;

                            case FileSystemSaveResult_transfer_error:
                            case FileSystemSaveResult_no_more_space_for_data:
                            case FileSystemSaveResult_fatfs_internal_error:
                            case FileSystemSaveResult_bug:
                            default:
                            {

                                // Uh oh, something bad happened.
                                // We'll just reset the MCU and the MCU initialization
                                // will handle the error if it still persists.

                                sorry

                            } break;

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

                            case FileSystemSaveResult_transfer_error:
                            case FileSystemSaveResult_no_more_space_for_data:
                            case FileSystemSaveResult_fatfs_internal_error:
                            case FileSystemSaveResult_bug:
                            default:
                            {

                                // Uh oh, something bad happened.
                                // We'll just reset the MCU and the MCU initialization
                                // will handle the error if it still persists.

                                sorry

                            } break;

                        }

                    }



                    // Heart-beat.

                    GPIO_INACTIVE(led_channel_red  );
                    GPIO_TOGGLE  (led_channel_green);
                    GPIO_INACTIVE(led_channel_blue );

                }
            } break;



            // Something bad happened with the camera module, so we'll go ahead and reinitialize it.
            // TODO Count reinitialization attempts?
            // TODO Indicator?

            case OVCAMSwapFramebufferResult_too_many_bad_jpeg_frames:
            case OVCAMSwapFramebufferResult_timeout:
            {
                try_reinitializing_ovcam();
            } break;



            // Catastrophic error!

            case OVCAMSwapFramebufferResult_bug:
            default:
            {
                sorry
            } break;

        }

    }

}
