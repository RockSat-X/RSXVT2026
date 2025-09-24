#include "system.h"
#include "uxart.c"
#include "sd.c"


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

    SD_reinit(SDHandle_primary);

    for (i32 iteration = 0;; iteration += 1)
    {
        GPIO_TOGGLE(led_green);
        spinlock_nop(100'000'000);
    }

}
