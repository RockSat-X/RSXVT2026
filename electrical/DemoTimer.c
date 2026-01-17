#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Miscellaneous initialization.
    //

    STPY_init();
    UXART_init(UXARTHandle_stlink);



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



    // Enable the update interrupt.

    CMSIS_SET(TIM1, DIER, UIE, true);



    // Enable the update interrupt in NVIC.

    NVIC_ENABLE(TIM1_UP);



    ////////////////////////////////////////////////////////////////////////////////



    // We can also enable the complement of OC1;
    // the corresponding OC1N pin will have opposite
    // polarity of OC1.

    CMSIS_SET(TIM1, CCER, CC1NE, true);



    ////////////////////////////////////////////////////////////////////////////////
    //
    // Spinlock away until the timer's update interrupt triggers.
    //

    for (;;);

}



// This interrupt routine executes every time the timer's counter gets reset.

INTERRUPT_TIM1_UP(void)
{

    CMSIS_SET(TIM1, SR, UIF, false); // Clear the update flag.

    GPIO_TOGGLE(led_green);

}
