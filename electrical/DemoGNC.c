#include "system.h"
#include "uxart.c"
#include "gnc.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);

    #include "GNC_MOCK_SIMULATION.meta"
    /* #meta

        import deps.stpy.pxd.pxd as pxd
        import csv, collections

        VN100Register = collections.namedtuple(
            'VN100Register',
            (
                'QuatX',
                'QuatY',
                'QuatZ',
                'QuatS',
                'MagX',
                'MagY',
                'MagZ',
                'AccelX',
                'AccelY',
                'AccelZ',
                'GyroX',
                'GyroY',
                'GyroZ',
            )
        )

        entries = [
            VN100Register(*(
                float(field)
                for field in line[7:-3].split(',')
            ))
            for line in pxd.make_main_relative_path('./gnc/vn100_scripts/vn100_dataLog.txt').read_text().splitlines()
        ][1200:]

        Meta.lut('struct GNCInput', 'GNC_MOCK_SIMULATION', (
            (
                entry_i,
                ('most_recent_imu.QuatX' , f'{entry.QuatX  :f}f'),
                ('most_recent_imu.QuatY' , f'{entry.QuatY  :f}f'),
                ('most_recent_imu.QuatZ' , f'{entry.QuatZ  :f}f'),
                ('most_recent_imu.QuatS' , f'{entry.QuatS  :f}f'),
                ('most_recent_imu.MagX'  , f'{entry.MagX   :f}f'),
                ('most_recent_imu.MagY'  , f'{entry.MagY   :f}f'),
                ('most_recent_imu.MagZ'  , f'{entry.MagZ   :f}f'),
                ('most_recent_imu.AccelX', f'{entry.AccelX :f}f'),
                ('most_recent_imu.AccelY', f'{entry.AccelY :f}f'),
                ('most_recent_imu.AccelZ', f'{entry.AccelZ :f}f'),
                ('most_recent_imu.GyroX' , f'{entry.GyroX  :f}f'),
                ('most_recent_imu.GyroY' , f'{entry.GyroY  :f}f'),
                ('most_recent_imu.GyroZ' , f'{entry.GyroZ  :f}f'),
            )
            for entry_i, entry in enumerate(entries)
        ))

    */

    struct GNCContext context = {0};

    f32 angular_velocity_x = 0.0f;
    f32 angular_velocity_y = 0.0f;
    f32 angular_velocity_z = 0.0f;

    for (i32 index = 0; index < countof(GNC_MOCK_SIMULATION); index += 1)
    {

        // TODO: GNC_update(GNC_MOCK_SIMULATION[index], &context);

        f32 angular_acceleration_x = 0.5f + arm_cos_f32((f32) index / 30.0f + 10.0f) * 5.0f; // TODO.
        f32 angular_acceleration_y = arm_cos_f32((f32) index / 50.0f) * 15.0f; // TODO.
        f32 angular_acceleration_z = arm_cos_f32((f32) index / 10.0f + 5.0f) * 20.0f; // TODO.

        angular_velocity_x += angular_acceleration_x * 0.020f;
        angular_velocity_y += angular_acceleration_y * 0.020f;
        angular_velocity_z += angular_acceleration_z * 0.020f;

        struct PlotSnapshot plot_snapshot =
            {
                .magic                  = PLOT_SNAPSHOT_TOKEN,
                .timestamp_us           = (u32) index * 20'000,
                .QuatX                  = GNC_MOCK_SIMULATION[index].most_recent_imu.QuatX,
                .QuatY                  = GNC_MOCK_SIMULATION[index].most_recent_imu.QuatY,
                .QuatZ                  = GNC_MOCK_SIMULATION[index].most_recent_imu.QuatZ,
                .QuatS                  = GNC_MOCK_SIMULATION[index].most_recent_imu.QuatS,
                .MagX                   = GNC_MOCK_SIMULATION[index].most_recent_imu.MagX,
                .MagY                   = GNC_MOCK_SIMULATION[index].most_recent_imu.MagY,
                .MagZ                   = GNC_MOCK_SIMULATION[index].most_recent_imu.MagZ,
                .AccelX                 = GNC_MOCK_SIMULATION[index].most_recent_imu.AccelX,
                .AccelY                 = GNC_MOCK_SIMULATION[index].most_recent_imu.AccelY,
                .AccelZ                 = GNC_MOCK_SIMULATION[index].most_recent_imu.AccelZ,
                .GyroX                  = GNC_MOCK_SIMULATION[index].most_recent_imu.GyroX,
                .GyroY                  = GNC_MOCK_SIMULATION[index].most_recent_imu.GyroY,
                .GyroZ                  = GNC_MOCK_SIMULATION[index].most_recent_imu.GyroZ,
                .angular_velocity_x     = angular_velocity_x,
                .angular_velocity_y     = angular_velocity_y,
                .angular_velocity_z     = angular_velocity_z,
                .angular_acceleration_x = angular_acceleration_x,
                .angular_acceleration_y = angular_acceleration_y,
                .angular_acceleration_z = angular_acceleration_z,
            };

        UXART_tx_bytes(UXARTHandle_stlink, (u8*) &plot_snapshot, sizeof(plot_snapshot));

    }

    for (;;)
    {

        GPIO_TOGGLE(led_green);
        spinlock_nop(250'000'000);

    }

}
