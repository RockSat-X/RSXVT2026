#include "system.h"
#include "uxart.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);

    #if 1
    {
        for (i32 iteration = 0;; iteration += 1)
        {

            for (i32 index = 0; index < 8; index += 1)
            {

                GPIO_SET(led_channel_red  , (index >> 0) & 1);
                GPIO_SET(led_channel_green, (index >> 1) & 1);
                GPIO_SET(led_channel_blue , (index >> 2) & 1);

                stlink_tx("Color index: %d\n", index);

                char input = {0};
                if (stlink_rx(&input))
                {
                    stlink_tx("Got '%c'!\n", input);
                }

                spinlock_nop(100'000'000);

            }

        }
    }
    #endif

    #if 1 // TODO Copy-pasta from DemoSDMMC, which will be improved upon soon...
    {

        SD_reinit(SDHandle_primary);

        i32 driver_error_count = 0;
        i32 task_error_count   = 0;

        for (u32 address = 0;; address += 1)
        {

            // Schedule a sector-read and block on it until it's done.

            struct Sector sector    = {0};
            enum SDDo     do_result = {0};

            do
            {
                do_result =
                    SD_do
                    (
                        SDHandle_primary,
                        SDOperation_single_read,
                        &sector,
                        address
                    );
            }
            while (do_result == SDDo_task_in_progress);



            // Handle the results of the sector-read.

            switch (do_result)
            {

                case SDDo_task_completed:
                {
                    // The sector-read was a success!
                } break;

                case SDDo_task_error:
                {

                    // The sector-read failed for some reason.
                    // To clear the error condition, we do a poll.

                    task_error_count += 1;

                    enum SDPoll poll_result = SD_poll(SDHandle_primary);

                    if (poll_result != SDPoll_cleared_task_error)
                        panic;



                    // Although not necessary, we'll
                    // reinitialize the SD driver.
                    // TODO A better approach here would
                    //      be to count back-to-back task errors
                    //      and reinitialize based on that.
                    //      The SD driver could also handle detecting
                    //      when to reinitialize the SD card on its own.

                    SD_reinit(SDHandle_primary);

                } break;

                case SDDo_driver_error:
                {

                    // Something unexpected happened for the SD driver,
                    // so we'll have to reinitialize the entire driver
                    // to clear the error condition.

                    driver_error_count += 1;

                    SD_reinit(SDHandle_primary);

                } break;

                case SDDo_task_in_progress : panic;
                case SDDo_bug              : panic;
                default                    : panic;

            }



            // Output the results.

            stlink_tx
            (
                "[Sector 0x%08X] Driver errors: %d | Task errors: %d.\n",
                address,
                driver_error_count,
                task_error_count
            );

            if (do_result == SDDo_task_completed)
            {
                for (i32 byte_i = 0; byte_i < 512; byte_i += 1)
                {
                    if (byte_i % 32 == 0)
                    {
                        stlink_tx("%03d | ", byte_i);
                    }

                    stlink_tx
                    (
                        "%02X%c",
                        sector.data[byte_i],
                        (byte_i % 32 == 31) ? '\n' : ' '
                    );
                }

                stlink_tx("\n");
            }



            // Bit of breather...

            GPIO_TOGGLE(led_channel_green);
            spinlock_nop(50'000'000);

        }

    }
    #endif

    #if 1

        UXART_init(UXARTHandle_stepper_uart);

        GPIO_ACTIVE(battery_allowed);

        spinlock_nop(1'000'000); // TODO



        ////////////////////////////////////////////////////////////////////////////////



        // Set the prescaler that'll affect all timers' kernel frequency.

        CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



        // The stepper driver relies on other timer
        // and UART initializations to be done first.

        STEPPER_partial_init(StepperHandle_axis_x);



        ////////////////////////////////////////////////////////////////////////////////

        for (;;)
        {
            #include "VehicleFlightComputer_VELOCITIES.meta"
            /* #meta

                import math

                with Meta.enter(f'static const i32 VELOCITIES[] ='):

                    for i in range(500):
                        Meta.line(f'''
                            {round(math.sin(math.sin(i / 500 * 2 * math.pi) * i / 125 * 2 * math.pi * 16) * 10_000)},
                        ''')

            */

            static i32 index = 0;

            while (!STEPPER_push_velocity(StepperHandle_axis_x, VELOCITIES[index]));

            index += 1;
            index %= countof(VELOCITIES);
        }

    #endif

}
