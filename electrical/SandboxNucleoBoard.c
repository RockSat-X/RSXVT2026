#include "system.h"
#include "uxart.c"



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);

    #if TARGET_USES_FREERTOS

        FREERTOS_init(); // @/`Using FreeRTOS`.

    #else

        for (i32 iteration = 0;; iteration += 1)
        {

            // Blink the LED.

            if (GPIO_READ(button)) // @/`Nucleo Buttons`.
            {
                spinlock_nop(50'000'000);
            }
            else
            {
                spinlock_nop(100'000'000);
            }

            GPIO_TOGGLE(led_green);



            // Say stuff.

            stlink_tx("Hello! The name of the current target is: " STRINGIFY(TARGET_NAME) ".\n");
            stlink_tx("FreeRTOS is currently disabled.\n");
            stlink_tx("We're on iteration %d.\n", iteration);



            // Check if we got any data from the ST-Link and echo it back if so.

            for (u8 received_data = {0}; stlink_rx(&received_data);)
            {
                stlink_tx("Got '%c'!\n", received_data);
            }

        }

    #endif

}



////////////////////////////////////////////////////////////////////////////////



// This is a global variable that's used by the task "button_observer"
// and the task "green_blinker". It is marked "volatile" so that the
// compiler will not do any tricky optimizations with it, because
// a task switch might change the value at any time.

static volatile b32 blink_green_led_fast = false;



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(button_observer, 1024, 0)
{
    for (;;)
    {

        b32 previous_button_pressed = false;



        // @/`Nucleo Buttons`.

        b32 current_button_pressed = GPIO_READ(button);



        // We look for a transition of unpressed to pressed.

        if (!previous_button_pressed && current_button_pressed)
        {
            blink_green_led_fast = !blink_green_led_fast;
        }



        previous_button_pressed = current_button_pressed;

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(green_blinker, 1024, 0)
{
    for (;;)
    {

        GPIO_TOGGLE(led_green);



        // Note that the amount of delay is dependent
        // on `configTICK_RATE_HZ` in <FreeRTOSConfig.h>.

        if (blink_green_led_fast)
        {
            vTaskDelay(10);
        }
        else
        {
            vTaskDelay(50);
        }

    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(red_blinker, 1024, 0)
{

    // The Nucleo-H533RE board doesn't have a red LED,
    // so this task will do nothing for that target.

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;);

    #endif

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(yellow_blinker, 1024, 0)
{

    // The Nucleo-H533RE board doesn't have a yellow LED,
    // so this task will do nothing for that target.

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;);

    #endif

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(yapper, 1024, 0)
{
    for (;;) // We send out text to the ST-Link.
    {
        i32 iteration = 0;

        stlink_tx("Hello! The name of the current target is: " STRINGIFY(TARGET_NAME) ".\n");
        stlink_tx("FreeRTOS is currently enabled.\n");
        stlink_tx("We're on iteration %d.\n", iteration);

        vTaskDelay(100);

        iteration += 1;
    }
}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(captain_allears, 1024, 0)
{
    for (;;)
    {

        u8 input = {0};

        if (stlink_rx(&input)) // Check if we got data from the ST-Link.
        {
            stlink_tx("Heard %c!\n", input);
        }

    }
}



////////////////////////////////////////////////////////////////////////////////



// @/`Using FreeRTOS`:
//
// FreeRTOS is a library that will allow us to have the microcontroller
// be able to switch between multiple concurrent tasks automatically.
// By default, it is disabled, because using it properly without shooting
// yourself will takes some skill.
//
// Nontheless, you can see how FreeRTOS will be very useful for us by going
// to <Shared.py> and flipping the `.use_freertos` option to `True` for the
// target you're using.
//
// When you rebuild and reflash, the behavior of the Sandbox program will
// change slightly. For one, you will notice that the Nucleo board is
// seemingly able to do multiple things at a time now. For instance, you
// can open the COM port of the ST-Link (via `./cli.py talk`) and see the
// data that the ST-Link is receiving; while that's happening, you can also
// type into the shell and your keys will be sent to the MCU (with the
// ST-Link being the middleman). When that happens, the MCU *immediately*
// echos it back.
//
// If we don't use FreeRTOS, then this responsiveness of the MCU disappears
// entirely, because the MCU is now instead doing a dumb delay that blocks
// everything.
//
// But as I said, using FreeRTOS effectively will take some experience, and
// a lot of that experience will be gained through the hard way. Rather than
// dealing with that right now, it's best for you to start off without a
// task-scheduler (that is, have `.use_freertos = False`).
//
// You can write your SPI drivers and what not, and then later we worry about
// how to make sure we can use it in a concurrent-safe way.



// @/`Nucleo Buttons`:
//
// There's two buttons on Nucleo boards: one for reset and one for the
// user to do whatever with. The latter is connected to one of the pins
// of the MCU, and which pin it is can be found either by reading through
// the user manual or just by inspecting the PCB file of the Nucleo board.
// The way we define GPIOs is in the file <Shared.py> with the option `gpios`.
//
// Note that the way the button is implemented on the Nucleo boards varies;
// sometimes the button connect the input GPIO to VDD or GND, and sometimes
// there's a pull resistor and sometimes not.
