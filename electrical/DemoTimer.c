#include "system/defs.h"
#include "uxart.c"

extern noret void
main(void)
{

    SYSTEM_init();
    UXART_init(UXARTHandle_stlink);

    #define RCC_CFGR_TIMPRE_init false
    #define TIM1_PSC_PSC_init    1000
    #define TIM1_ARR_ARR_init    150

    CMSIS_SET(RCC , CFGR1  , TIMPRE, RCC_CFGR_TIMPRE_init); // Set the prescaler mode that affects all timers.
    CMSIS_SET(RCC , APB2ENR, TIM1EN, true                ); // Clock the timer peripheral.
    CMSIS_SET(TIM1, PSC    , PSC   , TIM1_PSC_PSC_init   ); // Set the divider and modulation to get the desired update frequency.
    CMSIS_SET(TIM1, ARR    , ARR   , TIM1_ARR_ARR_init   ); // "
    CMSIS_SET(TIM1, CCMR1  , OC1M  , 0b0011              ); // The channel output toggles on each timer update.
    CMSIS_SET(TIM1, CCER   , CC1E  , true                ); // Enable the channel.
    CMSIS_SET(TIM1, CR1    , CEN   , true                ); // Enable the timer's counter.
    CMSIS_SET(TIM1, BDTR   , MOE   , true                ); // Master output enable.

    for (;;)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
