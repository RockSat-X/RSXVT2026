#define nop() __asm__("nop")

static void
delay_nop(u32 count)
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

#define sorry panic_(false);
#define panic panic_(true)
static noret void
panic_(b32 hard_error)
{
    __disable_irq();

    if (hard_error)
    {
        GPIO_HIGH(led_red);
    }
    else
    {
        GPIO_HIGH(led_yellow);
    }

    for (;;) // Here, the timing of the blinking is so that it is noticable even with different CPU clock speeds.
    {
        u32 i = 10'000'000;

        for (; i > 1'000; i /= 2)
        {
            for (u32 j = 0; j < 8; j += 1)
            {
                delay_nop(i);

                if (hard_error)
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
}
