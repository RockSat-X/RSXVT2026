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

        struct Matrix*     new_angular_velocities = Matrix(3, 1);
        struct VN100Packet most_recent_imu        =
            {
                .QuatX  = 1.0f,
                .QuatY  = 2.0f,
                .QuatZ  = 3.0f,
                .QuatS  = 4.0f,
                .MagX   = 5.0f,
                .MagY   = 6.0f,
                .MagZ   = 7.0f,
                .AccelX = 8.0f,
                .AccelY = 9.0f,
                .AccelZ = 10.0f,
                .GyroX  = 11.0f,
                .GyroY  = 12.0f,
                .GyroZ  = 13.0f,
            };

        enum GNCUpdateResult result =
            GNC_update
            (
                new_angular_velocities,
                &most_recent_imu
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
