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

        // Wait for the next image frame.

        struct OVCAMFramebuffer* framebuffer = {0};

        do
        {
            framebuffer = OVCAM_get_next_framebuffer();
        }
        while (!framebuffer);



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
