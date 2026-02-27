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

        struct Matrix* new_angular_velocities = Matrix(3, 1);

        enum GNCUpdateResult result =
            GNC_update
            (
                new_angular_velocities
            );

        switch (result)
        {

            case GNCUpdateResult_okay:
            {

                stlink_tx("New angular velocities:\n");
                MATRIX_stlink_tx(new_angular_velocities);

                GPIO_TOGGLE(led_green);
                spinlock_nop(100'000'000);

            } break;

            case GNCUpdateResult_bug : sorry
            default                  : sorry

        }

    }

}
