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



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Enable the peripheral.

    CMSIS_SET(RCC, APB2ENR, TIM1EN, true);



    // Configure the divider to set the rate at
    // which the timer's counter will increment.

    CMSIS_SET(TIM1, PSC, PSC, STPY_TIM1_DIVIDER);



    // Set the value at which the timer's counter
    // will reach and then reset; this is when an
    // update event happens.

    CMSIS_SET(TIM1, ARR, ARR, STPY_TIM1_MODULATION);



    // Set the mode of the OC1 pin to be in "PWM mode 1".
    // @/pg 1551/sec 36.6.8/`H533RErm`.

    CMSIS_SET(TIM1, CCMR1, OC1M, 0b0110);



    // In "PWM mode 1", the counter will be compared with the
    // value in this register. If the counter is smaller, then
    // the OC1 pin will be "active", or otherwise "inactive".
    // Thus if we set the compare-value to be halfway between zero
    // and the modulation value, then we'd get a 50% duty cycle.

    CMSIS_SET(TIM1, CCR1, CCR1, STPY_TIM1_MODULATION / 2);



    // Trigger an update event so that the shadow registers
    // ARR, PSC, and CCRx are what we initialize them to be.
    // The hardware uses shadow registers in order for updates
    // to these registers not result in a corrupt timer output.

    CMSIS_SET(TIM1, EGR, UG, true);



    // Enable the OC1 pin output.

    CMSIS_SET(TIM1, CCER, CC1E, true);



    // Enable the timer's counter.

    CMSIS_SET(TIM1, CR1, CEN, true);



    // Master output enable.

    CMSIS_SET(TIM1, BDTR, MOE, true);



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
