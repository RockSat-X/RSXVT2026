#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "sd.c"
#include "filesystem.c"
#include "ovcam.c"



////////////////////////////////////////////////////////////////////////////////



pack_push
    struct ImageChapter
    {
        u8  ending_token[sizeof(TV_TOKEN_END) - 1];
        u32 sequence_number;
        u32 image_size;
        u32 image_timestamp_us;
        u32 cycle_count;
        u8  padding[512 - (sizeof(TV_TOKEN_END) - 1) - (sizeof(TV_TOKEN_START) - 1) - 4 * sizeof(u32)];
        u8  starting_token[sizeof(TV_TOKEN_START) - 1];
    } __attribute__((aligned(4)));
pack_pop

static_assert(sizeof(struct ImageChapter) == sizeof(Sector));



static void
reinitialize_ovcam(void)
{

    while (true)
    {
        enum OVCAMReinitResult result = OVCAM_reinit();

        switch (result)
        {
            case OVCAMReinitResult_success                       : return;
            case OVCAMReinitResult_failed_to_initialize_with_i2c : break;
            case OVCAMReinitResult_bug                           : sorry
            default                                              : sorry
        }
    }

}



static void
try_swap(void)
{

    enum OVCAMSwapFramebufferResult result = OVCAM_swap_framebuffer();

    switch (result)
    {

        case OVCAMSwapFramebufferResult_attempted:
        {
            // An attempt was made to get the next framebuffer.
        } break;

        case OVCAMSwapFramebufferResult_too_many_bad_jpeg_frames:
        case OVCAMSwapFramebufferResult_timeout:
        {
            reinitialize_ovcam(); // Something bad happened, so we'll reinitialize everything.
        } break;

        case OVCAMSwapFramebufferResult_bug : sorry
        default                             : sorry

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



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



    // TODO.

    enum FileSystemReinitResult reinit_result =
        FILESYSTEM_reinit
        (
            SDHandle_primary,
            &(Sector) {0},
            1
        );

    switch (reinit_result)
    {
        case FileSystemReinitResult_success                    : break;
        case FileSystemReinitResult_couldnt_ready_card         : sorry
        case FileSystemReinitResult_transfer_error             : sorry
        case FileSystemReinitResult_invalid_filesystem         : sorry
        case FileSystemReinitResult_no_more_space_for_new_file : sorry
        case FileSystemReinitResult_fatfs_internal_error       : sorry
        case FileSystemReinitResult_bug                        : sorry
        default                                                : sorry
    }

    reinitialize_ovcam();

    for (;;)
    {

        // See if we received a write command.

        u8 code = {0};

        if (stlink_rx(&code) && code == TV_WRITE_BYTE)
        {

            // Update one of the camera module's register.

            pack_push
                struct WriteCommand
                {
                    u16 address;
                    u8  content;
                };
            pack_pop

            struct WriteCommand command = {0};

            for (i32 i = 0; i < sizeof(command); i += 1)
            {
                while (!stlink_rx((u8*) &command + i));
            }



            // Note: this is only done for testing purposes.

            {

                struct I2CDoJob job =
                    {
                        .handle       = I2CHandle_ovcam_sccb,
                        .address_type = I2CAddressType_seven,
                        .address      = OVCAM_SEVEN_BIT_ADDRESS,
                        .writing      = true,
                        .repeating    = false,
                        .pointer      = (u8*) &command,
                        .amount       = sizeof(command)
                    };

                enum I2CDoResult transfer_result = {0};
                do transfer_result = I2C_do(&job);
                while (transfer_result == I2CDoResult_working);

                if (transfer_result != I2CDoResult_success)
                    sorry

            }



            // Flush whatever images are in the swapchain
            // so the new images with the new settings
            // will be next.

            while (true)
            {

                try_swap();

                if (!OVCAM_current_framebuffer)
                {
                    break;
                }

            }

        }



        // See if the next image frame is available.

        try_swap();

        if (OVCAM_current_framebuffer)
        {

            // Send the data over.

            struct ImageChapter chapter =
                {
                    .ending_token   = TV_TOKEN_END,
                    .starting_token = TV_TOKEN_START,
                };

            {

                enum FileSystemSaveResult save_result =
                    FILESYSTEM_save
                    (
                        SDHandle_primary,
                        (Sector*) &chapter,
                        1
                    );

                switch (save_result)
                {
                    case FileSystemSaveResult_success                : break;
                    case FileSystemSaveResult_transfer_error         : sorry
                    case FileSystemSaveResult_no_more_space_for_data : sorry
                    case FileSystemSaveResult_fatfs_internal_error   : sorry
                    case FileSystemSaveResult_bug                    : sorry
                    default                                          : sorry
                }

            }
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
                    case FileSystemSaveResult_success                : break;
                    case FileSystemSaveResult_transfer_error         : sorry
                    case FileSystemSaveResult_no_more_space_for_data : sorry
                    case FileSystemSaveResult_fatfs_internal_error   : sorry
                    case FileSystemSaveResult_bug                    : sorry
                    default                                          : sorry
                }

            }



            // Heart-beat.

            GPIO_TOGGLE(led_channel_green);

        }

    }



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Heartbeat.
//



FREERTOS_TASK(blinker, 256, 0)
{
    for (;;)
    {
        GPIO_TOGGLE(led_channel_green);
        vTaskDelay(250);
    }
}
