#define ALLOW_FILESYSTEM_TO_BE_FORMATTED true

#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "ovcam.c"



////////////////////////////////////////////////////////////////////////////////



#define panic panic_()
static noret void
panic_(void)
{

    WATCHDOG_KICK(); // Let the below pattern play out entirely.

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
                    panic; // Something's wrong with the camera module...
                }
            } break;

            case OVCAMReinitResult_bug : panic;
            default                    : panic;

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

    WATCHDOG_KICK();
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
                    &(struct Sector) {0},
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

                    // On March 15th, 2026, I ran a quick experiment to see how long it'd take to fill an SD card.
                    // After 10 hours and 45 minutes and 37 seconds, a binary blob file of 45,964,307,968 bytes bytes
                    // was made. This results in an average data throughput of ~1.13 MiB/s, which means it'd take 8 hours
                    // to record a 32 GiB file, double that if we're using a 64 GiB card. There's some file-system overhead
                    // to account for, so a 64 GiB SD card is not really 64 GiB, but overall, the camera system will not be
                    // generating enough data to have any worry about maxing out the SD card after doing a bunch of
                    // sequence tests and what not. Of course these numbers will change and have to be reevaluated if
                    // the camera drivers are updated or depending on the complexity of the scene that the camera is taking
                    // images of, but yeah.
                    //
                    // Regardless, we still need to address this edge-case of what happens if the file-system on the
                    // SD card is unmountable. This is likely due to a corrupted file-system, but whether or not we
                    // should lock-up because of it or reformat the card with a new file-system is the question.
                    //
                    // If we were to power-on very early in the experiment's run-time, like right when GSE-1 is enabled,
                    // then we should do a file-system wipe. However, if during the flight, after most of the interesting
                    // stuff has already happened and is recorded, we shouldn't do a file-system wipe. Unfortunately,
                    // there's no good way to have the best of both worlds, because there's really no good way to tell
                    // if the data on the SD card is worthwhile to keep.
                    //
                    // What we will likely do in the end is have one camera system be programmed to wipe the filesystem
                    // and another to never do it. This doesn't really eliminate the problem, but it's a compromise of
                    // something at least...

                    if (!ALLOW_FILESYSTEM_TO_BE_FORMATTED)
                    {
                        // Nothing we can honestly do...
                    }
                    else if (attempts < 256)
                    {
                        // Let's recheck to be SUPER sure we actually need to wipe the SD card...
                    }
                    else if (completely_wipe_filesystem)
                    {
                        // Hmm... we'll just try to reformat again...
                    }
                    else
                    {
                        WATCHDOG_KICK();
                        completely_wipe_filesystem = true;
                    }

                } break;

                case FileSystemReinitResult_bug : panic;
                default                         : panic;

            }

        }

    }
    WATCHDOG_KICK();



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

                    WATCHDOG_KICK();



                    // We first insert some meta-data about the image.

                    struct ImageMetadata metadata =
                        {
                            .ending_token       = TV_TOKEN_END,
                            .image_index        = image_index,
                            .image_size         = (u32) OVCAM_current_framebuffer->length,
                            .image_timestamp_us = OVCAM_current_framebuffer->timestamp_us,
                            .cpu_cycle_counter  = CPU_CYCLE_COUNTER(),
                            .starting_token     = TV_TOKEN_START,
                        };

                    static_assert(sizeof(struct ImageMetadata) == sizeof(struct Sector));

                    {

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

                            case FileSystemSaveResult_transfer_error         : WARM_RESET();
                            case FileSystemSaveResult_no_more_space_for_data : WARM_RESET();
                            case FileSystemSaveResult_fatfs_internal_error   : panic;
                            case FileSystemSaveResult_bug                    : panic;
                            default                                          : panic;

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
                                (struct Sector*) OVCAM_current_framebuffer->data,
                                (OVCAM_current_framebuffer->length + sizeof(struct Sector) - 1) / sizeof(struct Sector)
                            );

                        switch (save_result)
                        {

                            case FileSystemSaveResult_success:
                            {
                                // All good saving the meta-data.
                            } break;

                            case FileSystemSaveResult_transfer_error         : WARM_RESET();
                            case FileSystemSaveResult_no_more_space_for_data : WARM_RESET();
                            case FileSystemSaveResult_fatfs_internal_error   : panic;
                            case FileSystemSaveResult_bug                    : panic;
                            default                                          : panic;

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

            case OVCAMSwapFramebufferResult_bug : panic;
            default                             : panic;

        }

    }

}
