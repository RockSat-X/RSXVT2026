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

        while (_OVCAM_swapchain.reader == _OVCAM_swapchain.writer);

        i32 read_index = _OVCAM_swapchain.reader % countof(_OVCAM_swapchain.framebuffers);



        // Send the data over.


        stlink_tx(TV_TOKEN_START);

        _UXART_tx_raw_nonreentrant
        (
            UXARTHandle_stlink,
            (u8*) _OVCAM_swapchain.framebuffers[read_index].data,
            _OVCAM_swapchain.framebuffers[read_index].length
        );

        stlink_tx(TV_TOKEN_END);



        // Heart-beat.

        GPIO_TOGGLE(led_green);



        // Begin the next image capture.

        _OVCAM_swapchain.reader += 1;

        NVIC_SET_PENDING(GPDMA1_Channel7);

    }

}
