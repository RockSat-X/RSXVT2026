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

        with Meta.enter('static struct GNCParameters GNC_MOCK_SIMULATION[] ='):

            for entry_i, entry in enumerate(csv.DictReader(
                pxd.make_main_relative_path('./misc/GNC_MOCK_SIMULATION.csv')
                    .read_text()
                    .splitlines()
            )):
                Meta.line(f'''
                    [{entry_i}] = {{ {','.join(f'.{key} = {value}' for key, value in entry.items())} }},
                ''')


    */



    for (;;)
    {

        for (i32 index = 0; index < countof(GNC_MOCK_SIMULATION); index += 1)
        {

            GNC_update(&GNC_MOCK_SIMULATION[index]);

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
