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

    for (;;)
    {
        stlink_tx("Sorry! The SPI driver is to be implemented. Come back later!\n");
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
