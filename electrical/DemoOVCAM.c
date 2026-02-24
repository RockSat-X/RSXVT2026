// Because `./cli.py tv` can do register-writes that causes
// some frames to be dropped, we need to avoid sporatic OVCAM
// resets by having a much longer time-out duration than usual.
#define OVCAM_TIMEOUT_US 10'000'000

#include "system.h"
#include "uxart.c"
#include "i2c.c"
#include "timekeeping.c"
#include "ovcam.c"



static void
reinitialize_ovcam(void)
{

    enum OVCAMReinitResult result = OVCAM_reinit();

    switch (result)
    {
        case OVCAMReinitResult_success                       : break;
        case OVCAMReinitResult_failed_to_initialize_with_i2c : sorry
        case OVCAMReinitResult_bug                           : sorry
        default                                              : sorry
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

        case OVCAMSwapFramebufferResult_bug:
        {
            reinitialize_ovcam(); // Something bad happened, so we'll reinitialize everything.
        } break;

        default: sorry

    }

}



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

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

                enum I2CTransferResult result =
                    I2C_transfer
                    (
                        I2CHandle_ovcam_sccb,
                        OVCAM_SEVEN_BIT_ADDRESS,
                        I2CAddressType_seven,
                        I2COperation_single_write,
                        (u8*) &command,
                        sizeof(command)
                    );

                if (result != I2CTransferResult_transfer_done)
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

            stlink_tx(TV_TOKEN_START);
            UXART_tx_bytes(UXARTHandle_stlink, OVCAM_current_framebuffer->data, OVCAM_current_framebuffer->length);
            stlink_tx(TV_TOKEN_END);



            // Heart-beat.

            GPIO_TOGGLE(led_green);

        }



        // To induce a spurious reset.

        GPIO_SET(ovcam_reset, GPIO_READ(button));

    }

}
