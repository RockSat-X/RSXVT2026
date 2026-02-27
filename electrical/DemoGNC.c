#include "system.h"
#include "uxart.c"
#include "gnc.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);



    struct GNCMockInput
    {
        struct VN100Packet  vn100;
        struct OpenMVPacket openmv;
    };

    #include "GNC_MOCK_SIMULATION.meta"
    /* #meta

        import deps.stpy.pxd.pxd as pxd
        import csv, collections

        entries = collections.defaultdict(lambda: [])

        for entry in csv.DictReader(
            pxd.make_main_relative_path('./misc/GNC_MOCK_SIMULATION.csv')
                .read_text()
                .splitlines()
        ):
            for key, value in entry.items():
                entries[key] += [value]

        Meta.lut('struct GNCMockInput', 'GNC_MOCK_SIMULATION', (
            (
                entry_i,
                *entry.items()
            )
            for entry_i, entry in enumerate(csv.DictReader(
                pxd.make_main_relative_path('./misc/GNC_MOCK_SIMULATION.csv')
                    .read_text()
                    .splitlines()
            ))
        ))

    */



    for (;;)
    {

        for (i32 index = 0; index < countof(GNC_MOCK_SIMULATION); index += 1)
        {

            struct Matrix* new_angular_velocities = Matrix(3, 1);

            enum GNCUpdateResult result =
                GNC_update
                (
                    new_angular_velocities,
                    &GNC_MOCK_SIMULATION[index].vn100,
                    &GNC_MOCK_SIMULATION[index].openmv
                );

            switch (result)
            {

                case GNCUpdateResult_okay:
                {

                    stlink_tx
                    (
                        "[%d/%d] <%f, %f, %f>\n",
                        index + 1,
                        countof(GNC_MOCK_SIMULATION),
                        new_angular_velocities->values[0],
                        new_angular_velocities->values[1],
                        new_angular_velocities->values[2]
                    );

                } break;

                case GNCUpdateResult_bug : sorry
                default                  : sorry

            }

        }

        stlink_tx("\n");

        GPIO_TOGGLE(led_green);
        spinlock_nop(250'000'000);

    }

}
