#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

    for (;;)
    {
        stlink_tx("Hello!\n");
        spinlock_nop(100'000'000);
    }

}
