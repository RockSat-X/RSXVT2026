#define nop() __asm__("nop")



static void
spinlock_nop(u32 count)
{
    // This is unrolled so that the branch penalty will be reduced.
    // Otherwise, the amount of delay that this procedure will produce
    // will be more inconsistent (potentially due to flash cache reasons?).
    for (u32 nop = 0; nop < count; nop += 256)
    {
        #define NOP4   nop(); nop(); nop(); nop();
        #define NOP16  NOP4   NOP4   NOP4   NOP4
        #define NOP64  NOP16  NOP16  NOP16  NOP16
        #define NOP256 NOP64  NOP64  NOP64  NOP64
        NOP256
        #undef NOP4
        #undef NOP16
        #undef NOP64
        #undef NOP256
    }
}



#if false

    static volatile u32 epoch_ms = 0;

    INTERRUPT(SysTick)
    {
        epoch_ms += 1;
    }

    static void
    spinlock_ms(u32 duration_ms)
    {
        u32 start_ms = epoch_ms;
        while (epoch_ms - start_ms < duration_ms);
    }

#endif



#define sorry halt_(false); // @/`Halting`.
#define panic halt_(true)   // "
static noret void           // "
halt_(b32 panicking)        // "
{
    __disable_irq();


    #if TARGET_NAME_IS_SandboxNucleoH7S3L8

        if (panicking)
        {
            GPIO_HIGH(led_red);
        }
        else
        {
            GPIO_HIGH(led_yellow);
        }

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 8; j += 1)
                {
                    spinlock_nop(i);

                    if (panicking)
                    {
                        GPIO_TOGGLE(led_red);
                    }
                    else
                    {
                        GPIO_TOGGLE(led_yellow);
                    }
                }
            }
        }

    #endif

    #if TARGET_NAME_IS_SandboxNucleoH533RE

        for (;;)
        {
            u32 i = 10'000'000;

            for (; i > 1'000; i /= 2)
            {
                for (u32 j = 0; j < 4; j += 1)
                {
                    GPIO_HIGH(led_green);
                    spinlock_nop(i);

                    GPIO_LOW(led_green);
                    spinlock_nop(i * (panicking ? 1 : 4));
                }
            }
        }

    #endif

    for(;;); // Panic! Something horrible has happened!
}



//////////////////////////////////////////////////////////////// Notes /////////////////////////////////////////////////////////////////

// @/`Halting`:
//
//
//
// A `sorry` is used for code-paths that haven't been implemented yet.
// Eventually, when we want things to be production-ready, we replace
// all `sorry`s that we can with proper code, or at the very least, a `panic`.
// e.g:
// >
// >    if (is_sd_card_mounted_yet())
// >    {
// >        do_stuff();
// >    }
// >    else
// >    {
// >        sorry   <- Note that `sorry` is specifically defined to be able to
// >                   be written without the terminating semicolon. This is
// >                   purely aesthetic; it makes `sorry` look very out-of-place.
// >    }
// >
//
//
//
// A `panic` is for absolute irrecoverable error conditions,
// like stack overflows or a function given ill-formed parameters.
// In other words, stuff like: "something horrible happened.
// I don't know how we got here. We need to reset!".
// It is useful to make this distinction from `sorry` because we can
// scan through the code-base and see what stuff is work-in-progress or just
// an irrecoverable error condition.
// e.g:
// >
// >    switch (option)
// >    {
// >        case Option_A: ...
// >        case Option_B: ...
// >        case Option_C: ...
// >        default: panic;     <- The compiler will enforce switch-statements to be exhaustive
// >                               for enumerations. The compiler will also enforce all
// >                               switch-statements to have a `default` case even if we are
// >                               exhaustive. In the event that the enumeration `option` is somehow
// >                               not any of the valid values, we will trigger a `panic` rather than
// >                               have the switch-statement silently pass.
// >    }
// >
//
//
//
// When a `sorry` or `panic` is triggered during deployment, the microcontroller will undergo a reset
// through the watchdog timer (TODO). During development, however, we'd like for the controller to actually
// stop dead in its track (rather than reset) so that we can debug. To make it even more useful,
// the microcontroller can also blink an LED indicating whether or not a `panic` or a `sorry` condition has
// occured; how this is implemented, if at all, is entirely dependent upon the target.
