#include "system.h"
#include "uxart.c"
#include "i2c.c"
#include "ovcam.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    I2C_reinit(I2CHandle_ovcam_sccb);



    ////////////////////////////////////////////////////////////////////////////////



    OVCAM_init();

    for (;;)
    {

        // Change configuration of the camera module if requested.

        u8 jpeg_ctrl3 = {0};

        if (stlink_rx((char*) &jpeg_ctrl3))
        {

            // Write the updated register settings.

            enum I2CMasterError error =
                I2C_blocking_transfer
                (
                    I2CHandle_ovcam_sccb,
                    OVCAM_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_write,
                    (u8[])
                    {
                        0x44, 0x03,
                        jpeg_ctrl3
                    },
                    3
                );

            if (error)
                sorry



            // Flush whatever images are in the swapchain
            // so the new images with the new settings
            // will be next.

            while (OVCAM_get_next_framebuffer())
            {
                OVCAM_free_framebuffer();
            }

        }



        // See if the next image frame is available.

        struct OVCAMFramebuffer* framebuffer = OVCAM_get_next_framebuffer();

        if (framebuffer)
        {

            // Send the data over.

            stlink_tx(TV_TOKEN_START);

            _UXART_tx_raw_nonreentrant
            (
                UXARTHandle_stlink,
                (u8*) framebuffer->data,
                framebuffer->length
            );

            stlink_tx(TV_TOKEN_END);



            // Heart-beat.

            GPIO_TOGGLE(led_green);



            // Release the current image frame to get the next one.

            OVCAM_free_framebuffer();
        }

    }

}
