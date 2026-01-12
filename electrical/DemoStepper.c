#include "system.h"
#include "uxart.c"
#include "stepper.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_stepper_uart);



    ////////////////////////////////////////////////////////////////////////////////



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // The stepper driver relies on other timer initializations to be done first.

    STEPPER_partial_init(StepperHandle_primary);



    ////////////////////////////////////////////////////////////////////////////////



    GPIO_LOW(driver_enable);

    for (;;)
    {

        #include "DemoStepper_STEPS.meta"
        /* #meta

            import math

            with Meta.enter(f'static const i8 STEPS[] ='):
                for i in range(4000):
                    Meta.line(f'''
                        {round(math.sin(i / 4000 * 2 * math.pi * 16) * 127)},
                    ''')

        */

        static i32 index = 0;

        while (!STEPPER_push_delta(StepperHandle_primary, STEPS[index]));

        index += 1;
        index %= countof(STEPS);

        pack_push
            struct ReadRequest
            {
                u8 sync;
                u8 node_address;
                u8 register_address;
                u8 crc;
            };
            struct ReadResponse
            {
                u8  sync;
                u8  master_address;
                u8  register_address;
                u32 data;
                u8  crc;
            };
        pack_pop

        struct ReadRequest request =
            {
                .sync             = 0b0000'0101,
                .node_address     = 0,
                .register_address = 0x06,
                .crc              = 0,
            };

        request.crc = _STEPPER_calculate_crc((u8*) &request, sizeof(request) - sizeof(request.crc));

        while (UXART_rx(UXARTHandle_stepper_uart, &(char) {0}));

        _UXART_tx_raw_nonreentrant
        (
            UXARTHandle_stepper_uart,
            (u8*) &request,
            sizeof(request)
        );

        for (i32 i = 0; i < sizeof(request); i += 1)
        {
            char byte = {0};
            while (!UXART_rx(UXARTHandle_stepper_uart, &byte));
            if (byte != ((char*) &request)[i])
                panic;
        }

        struct ReadResponse response = {0};

        for (i32 i = 0; i < sizeof(response); i += 1)
        {
            while (!UXART_rx(UXARTHandle_stepper_uart, &((char*) &response)[i]));
        }

        u8 digest = _STEPPER_calculate_crc((u8*) &response, sizeof(response) - sizeof(response.crc));

        if (digest != response.crc)
            panic;

        if (response.sync != 0b0000'0101)
            panic;

        if (response.master_address != 0xFF)
            panic;

        if (response.register_address != 0x06)
            panic;

        stlink_tx("0x%08X\n", response.data);

        spinlock_nop(100'000'000);

    }

}
