// Welcome to the sandbox! Please read the comments at the end of the file.

#include "defs.h"
#include "uxart.c"



extern noret void
main(void)
{
    STPY_init();
    UXART_init(UXARTHandle_stlink);


    #if TARGET_USES_FREERTOS

        FREERTOS_init(); // @/`Using FreeRTOS`.

    #else

        for (i32 iteration = 0;; iteration += 1)
        {

            // Blink the LED.

            if (GPIO_READ(button)) // @/`Nucleo Buttons`.
            {
                spinlock_nop(100'000'000);
            }
            else
            {
                spinlock_nop(50'000'000);
            }

            GPIO_TOGGLE(led_green);



            // Say stuff.

            stlink_tx("Hello! The name of the current target is: " STRINGIFY(TARGET_NAME) ".\n");
            stlink_tx("FreeRTOS is currently disabled.\n");
            stlink_tx("We're on iteration %d.\n", iteration);



            // Check if we got any data from the ST-Link and echo it back if so.

            for (char received_data = {0}; stlink_rx(&received_data);)
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

        b32 current_button_pressed = !GPIO_READ(button);



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

    // The Nucleo-H7S3L8 board has a red LED,
    // so this task will do actual work for that target.

    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        for (;;)
        {
            GPIO_TOGGLE(led_red);
            vTaskDelay(50);
        }

    #endif



    // The Nucleo-H533RE board doesn't have a red LED,
    // so this task will do nothing for that target.

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;);

    #endif

}



////////////////////////////////////////////////////////////////////////////////



FREERTOS_TASK(yellow_blinker, 1024, 0)
{

    // The Nucleo-H7S3L8 board has a yellow LED,
    // so this task will do actual work for that target.

    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        for (;;)
        {
            GPIO_TOGGLE(led_yellow);
            vTaskDelay(10);
        }

    #endif




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

        char input = {0};

        if (stlink_rx(&input)) // Check if we got data from the ST-Link.
        {
            stlink_tx("Heard %c!\n", input);
        }

    }
}



////////////////////////////////////////////////////////////////////////////////



// Hi again!
//
// Rather than have one massive comment at the start of the file,
// most lengthy comments are placed at the end. This makes surfing
// through code much easier as you aren't face-planted with "useless"
// copyright and exposition everytime you open a source file.
//
// Anyways, welcome to the sandbox!
// What you're looking at is a demo program. This is to help you get up
// to speed with how this project's development environment works.
//
// If you have a Nucleo-H533RE or a Nucleo-H7S3L8 board on hand, feel
// free to use those boards to flash this sandbox program. More specifically,
// use the "SandboxNucleoH533RE" or "SandboxNucleoH7S3L8" targets to do so.
// Once you flashed the program, try doing some debugging and step through
// the code. For fun, feel free to change some values, rebuild, and then
// reflash your board to see if anything changed.
//
// Alright, once you feel like you got the gist of it,
// let's go over some things real quick.
//
// First, check out @/`About Citations`;
// it's a comment with the same tag later in this file.
//
// Second, a "target" will essentially be a program to be flashed onto a
// particular microcontroller. Targets are defined in <./electrical/Shared.py>
// within the variable `TARGETS`, so go over there and check it out!
//
// ... and third, you might notice that `TARGETS` is defined within a Python
// file, and the truth is that a lot of this codebase is actually written
// in Python! If you go explore and look at other source files, there's actually
// a lot of Python code, most of which is inside C-style comments.
//
// Does this mean we are running Python code
// on the STM32 microcontrollers?
//
// No!
//
// Most of the Python code you see are what I call meta-directives;
// they are actually for the meta-preprocessor, a preprocessor program
// that I (Phuc Doan) wrote. What the Python code actually does is
// generate C code, a whole lot of C code!
//
// By now you probably already built the project, so to see the code that
// has been generated by the meta-directives, check out the <./build/meta/>
// directory. Every file you see in there is code generated by the
// meta-preprocessor. A lot of the code is for low-level stuff, like
// initializing GPIOs, configuring the microcontroller's clock-tree,
// setting things up for FreeRTOS, and so on.
//
// The meta-preprocessor -- if it's not apparent by now -- is very
// powerful because you can go ahead and change some settings in `TARGETS`
// and all of the generated code automatically update every time you
// rebuild the project!
//
// For example, `STLINK_BAUD` is defined in <./electrical/Shared.py>,
// and it defines the communication speed between the MCU and the ST-Link.
// If you change the value to be something else like `STLINK_BAUD = 8_000`
// and rebuild, not much seem to have happened, but in fact, a lot did!
//
// For background, one of the things that needs to be done on a
// microcontroller is configuring the clock-tree; this means figuring out
// what clock sources to use, what dividers are needed, and so on in order
// to clock different parts of the MCU at the different speeds. To keep
// things short, this is a VERY complicated process. Nonetheless, there's
// a meta-directive whose whole job is to essentially brute-force the
// clock-tree in order to get the microcontroller to satisfy our high-level
// constraints like the desired CPU clock speed and, of course, the
// desired `STLINK_BAUD` rate.
//
// As an excercise, I encourage you to find where and how the generated
// code changes when you modify the `STLINK_BAUD` value. Note that if you
// change `STLINK_BAUD` to certain values (e.g. 9600), it very likely will
// result in the meta-directive complaining that it couldn't brute-force
// the clock-tree.
//
// This application of meta-directives also extend to GPIOs. Every target
// will have a table of GPIO pins, each pin having properties assigned to
// it. It's quite nice, because like with the clock-tree, every time you
// rebuild the project, the code to initialize the GPIO pins for the
// microcontroller is also generated all on-the-fly. Introducing new GPIOs
// is literally as easy as copy-and-paste! For more information,
// see @/`How GPIOs Are Made`.
//
// Hopefully it has dawned upon you that the way we're approaching our
// project is likely unlike anything you've seen; it is more handmade,
// more homegrown, more driven by the need us as programmers. To see
// what I mean, read @/`STM32CubeMX Rant`.
//
// Alright, I think I've said enough. Remember, this file is a sandbox, so
// you can do whatever you want in here. This includes trying to make some
// LEDs blink in a unique pattern, writing your own meta-directives and
// experimenting with how the meta-preprocessor works, or going straight
// into writing some driver code! The world is your oyster here.
//
// If you have any questions, please make a ticket on GitHub or make a
// forum post on Discord. I (Phuc Doan) am the expert in all of this, and
// this is my passion, so don't be afraid to reach out!



// @/`STM32CubeMX Rant`:
//
// You should realize that nothing being done here with the
// meta-preprocessor is actually all that new. If you're familiar with
// STM32s, you might've used STM32CubeMX (or use it within STM32CubeIDE).
// What STM32CubeMX does is not all too different from what we're doing
// right now with the meta-preprocessor; STM32CubeMX also generates code,
// and rather than using Python code to configure the project, there's a
// GUI to click on instead. However, it should be noted that is where all
// the similarities end. If you have ever worked with STM32CubeMX, you
// should know that it's actually straight-up HOSTILE TO THE USER.
//
// First, working with STM32CubeMX is AWFULLY SLOW, and what I mean by that
// is me adding a new GPIO pin should be INSTANTANEOUS, and I bet you can't
// open STM32CubeMX, add a new GPIO pin, and regenerate the code and have all
// that taking less than even TEN SECONDS! You might not think this is a big
// deal, but you should take offense to bad software like this.
//
// To put it into perspective, as of writing, the meta-preprocessor
// takes 60ms to run on my beefy PC, and with how the build system works,
// we are doing a FULL, CLEAN BUILD every time! NO INCREMENTAL BUILDS!
// This means the meta-preprocessor is brute-forcing the clock-tree
// and generating the code to configure the microcontroller all in 60ms
// EVERY TIME WE BUILD! Yet it takes STM32CubeMX SECONDS to do the same
// every time we CLICK THE REGENERATE BUTTON! If it took STM32CubeMX
// to generate the code in 1 second, then I could've run the
// meta-preprocessor 1/0.060 = 16.67 times back-to-back!
//
// Second, the infrastructure and culture around using STM32CubeMX is
// unfortunately opaque and is nothing but a blackbox to new learners;
// that is, people who are new to the STM32 embedded environment will rely
// on STM32CubeMX to generate the code for them (because they obviously
// don't know how they'd do it themselves yet).
//
// But what happens if the person is courageous enough to actually put in the
// effort to learn the magic? Well guess what: STM32CubeMX's generated code
// is absolute dog-water! The amount of function calls needed to do something
// as simple as configuring a GPIO pin or initialize a UART is absolutely
// ABSURD. Remember, most of the time, all the microcontroller is doing
// is literally reading and writing to registers, so why not generate code
// that do JUST that?!
//
// In other words, what STM32CubeMX should generate, if ST cared at all, is
// something that looks like `SYSTEM_init` where a good chunk of the code is
// just read/modify/write statements. If you're curious, feel free to bust
// open the reference manual and read what each register does and how it
// affects the microcontroller. Furthermore, you could always just read
// the meta-directive that generated the code! No black-magic going on here.
//
// Now, it should be stated that STM32CubeMX does have its place. In particular,
// the damn thing is made by ST, and so they have software support for 99% of
// all of their MCUs. This meta-preprocessor we're using, as of writing, only
// has support for two, and the support is very incomplete.
//
// For instance, for this year's project, we're going to need to handroll our
// own drivers for SPI, I2C, and so on. Getting a peripheral to work on
// STM32CubeMX is much easier; often it's just clicking a button or two and
// you have something up and running.
//
// However -- if I have not made it clear -- this ease is nothing but
// an ILLUSION. You might get something up and working fast, but you will
// now be severely limited to STM32CubeMX's blackbox constraints. If we're
// going to have quality in our project, we must do it ourselves and do it right!
//
// This will mean we will spend more effort in reinventing the wheel,
// but realize that this effort will be spent in LEARNING how microcontrollers
// work, LEARNING how to read reference manuals, LEARNING how to debug complex
// systems with the tools we make ourselves.
//
// ... or we take the "easy" route with STM32CubeMX, and end up "LEARNING" what
// buttons make STM32CubeMX happy, "LEARNING" what files we need to copy and
// paste from the internet to make SD drivers work, "LEARNING" what settings
// to tick and click in the Eclipse clone that is STM32CubeIDE.
//
// Either way, we will end up spending a lot of effort doing something,
// but personally, I rather put in the effort to learn new things that actually
// have real-world applications, even if it means slower progress!



// @/`About Citations`:
//
// Something we'll be doing for this year is making sure we use citations
// throughout our development; most citations will be to reference manuals,
// datasheets, and such. To learn more about the syntax of citations,
// see @/url:`https://github.com/PhucXDoan/pxd`.
//
// If you want to use the citation manager, invoke it by running `cli.py cite`.
// The citation manager can list all citations found within the codebase,
// find any typos, and allow for renaming of sources. It's pretty nifty!



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
// Note that the way the button is implemented on the Nucleo boards is that
// pressing it will tie the GPIO to ground. Thus, if we read a high value
// on the GPIO, then the button is unpressed; if it's zero, then the button
// is pressed.
