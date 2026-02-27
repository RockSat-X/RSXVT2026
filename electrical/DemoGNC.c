#include "system.h"
#include "uxart.c"
#include "gnc.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);

    for (i32 index = 0;; index += 1)
    {

        stlink_tx("\n[Index %d]\n", index);

        enum GNCUpdateResult result = GNC_update();

        switch (result)
        {

            case GNCUpdateResult_okay:
            {
                GPIO_TOGGLE(led_green);
                spinlock_nop(100'000'000);
            } break;

            case GNCUpdateResult_bug : sorry
            default                  : sorry

        }

    }

}
