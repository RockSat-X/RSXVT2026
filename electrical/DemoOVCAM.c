#include "system.h"
#include "uxart.c"
#include "i2c.c"
#include "ovcam.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);



    {
        enum OVCAMReinitResult result = OVCAM_reinit();
        switch (result)
        {
            case OVCAMReinitResult_success                       : break;
            case OVCAMReinitResult_failed_to_initialize_with_i2c : panic;
            case OVCAMReinitResult_bug                           : panic;
            default                                              : panic;
        }
    }



    for (;;)
    {

        // See if we received a write command.

        u8 code = {0};

        if (stlink_rx(&code) && code == TV_WRITE_BYTE)
        {

            // Update one of the camera module's register.

            struct WriteCommand
            {
                u16 address;
                u8  content;
            } __attribute__((packed));

            struct WriteCommand command = {0};

            for (i32 i = 0; i < sizeof(command); i += 1)
            {
                while (!stlink_rx((u8*) &command + i));
            }

            OVCAM_write_register(command.address, command.content);



            // Flush whatever images are in the swapchain
            // so the new images with the new settings
            // will be next.

            while (true)
            {

                enum OVCAMSwapFramebufferResult result = OVCAM_swap_framebuffer();

                switch (result)
                {
                    case OVCAMSwapFramebufferResult_success : break;
                    case OVCAMSwapFramebufferResult_bug     : panic;
                    default                                 : panic;
                }

                if (!OVCAM_current_framebuffer)
                {
                    break;
                }

            }

        }



        // See if the next image frame is available.

        enum OVCAMSwapFramebufferResult result = OVCAM_swap_framebuffer();

        switch (result)
        {
            case OVCAMSwapFramebufferResult_success : break;
            case OVCAMSwapFramebufferResult_bug     : panic;
            default                                 : panic;
        }

        if (OVCAM_current_framebuffer)
        {

            // Send the data over.

            stlink_tx(TV_TOKEN_START);
            UXART_tx_bytes(UXARTHandle_stlink, OVCAM_current_framebuffer->data, OVCAM_current_framebuffer->length);
            stlink_tx(TV_TOKEN_END);



            // Heart-beat.

            GPIO_TOGGLE(led_green);

        }

    }

}
