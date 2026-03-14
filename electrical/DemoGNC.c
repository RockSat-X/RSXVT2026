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

        entries = collections.defaultdict(lambda: [])

        for entry in csv.DictReader(
            pxd.make_main_relative_path('./misc/GNC_MOCK_SIMULATION.csv')
                .read_text()
                .splitlines()
        ):
            for key, value in entry.items():
                entries[key] += [value]

        Meta.lut('struct GNCInput', 'GNC_MOCK_SIMULATION', (
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

    struct GNCContext context = {0};

    for (;;)
    {

        for (i32 index = 0; index < countof(GNC_MOCK_SIMULATION); index += 1)
        {

            GNC_update(GNC_MOCK_SIMULATION[index], &context);

            stlink_tx
            (
                "[%d/%d]\n",
                index + 1,
                countof(GNC_MOCK_SIMULATION)
            );

        }

        stlink_tx("\n");

        GPIO_TOGGLE(led_green);
        spinlock_nop(250'000'000);

    }

}
