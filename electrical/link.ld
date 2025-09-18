#define CONCAT_(X, Y) X##Y
#define CONCAT(X, Y)  CONCAT_(X, Y)

ENTRY(INTERRUPT_Reset) /* Just to indicate where program execution would begin for debugging purposes; doesn't actually affect anything. */

MEMORY
{
    #if TARGET_MCU_IS_STM32H7S3L8H6 /* @/pg 151/fig 3/`H7S3rm`. */
        AXI_SRAM (rw) : ORIGIN = 0x24000000, LENGTH = 456K
        DTCM     (rw) : ORIGIN = 0x20000000, LENGTH = 64K /* TODO There's actually 192K worth of memory available to use, but we might have to program the option bytes? */
        FLASH    (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    #endif

    #if TARGET_MCU_IS_STM32H533RET6 /* @/pg 117/fig 3/`H533rm`. */
        SRAM     (rw) : ORIGIN = 0x0A000000, LENGTH = 256K /* TODO Verify that we have this much memory. */
        FLASH    (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    #endif
}

SECTIONS
{


    /**************************************************************** FLASH ****************************************************************/



    .INTERRUPT_VECTOR_TABLE_output_section :
    {
        KEEP(*(.INTERRUPT_VECTOR_TABLE))
    } > FLASH

    .text_output_section : ALIGN(4)
    {
        *(.text*)
    } > FLASH

    .rodata_output_section : ALIGN(4)
    {
        *(.rodata*)
    } > FLASH



    /**************************************************************** RAM ****************************************************************/



    .stack_output_section : ALIGN(32) /* For MPU region alignment. TODO If we're going to use the MPU to detect stack overflow? */
    {
        .                     += MAIN_STACK_SIZE;
        INITIAL_STACK_ADDRESS  = .;

        #define STACK_LOCATION_STM32H7S3L8H6 DTCM
        #define STACK_LOCATION_STM32H533RET6 SRAM
    } > CONCAT(STACK_LOCATION_, TARGET_MCU)

    .data_output_section : ALIGN(4)
    {
        LINK_data_load_addr_start = LOADADDR(.data_output_section);
        LINK_data_virt_addr_start = .;
        *(.data*)
        .                       = ALIGN(4);
        LINK_data_virt_addr_end = .;

        #define DATA_LOCATION_STM32H7S3L8H6 AXI_SRAM
        #define DATA_LOCATION_STM32H533RET6 SRAM
    } > CONCAT(DATA_LOCATION_, TARGET_MCU) AT > FLASH

    .bss_output_section : ALIGN(4)
    {
        LINK_bss_addr_start = .;
        *(.bss*)
        .                 = ALIGN(4);
        LINK_bss_addr_end = .;

        #define BSS_LOCATION_STM32H7S3L8H6 AXI_SRAM
        #define BSS_LOCATION_STM32H533RET6 SRAM
    } > CONCAT(BSS_LOCATION_, TARGET_MCU)
}



/**************************************************************** Notes ****************************************************************/



/*
    This is the linker script to define how symbols and sections should be merged together with other object fields.

    If you don't know what that exactly means, or not even familar with the
    process of linking, then I highly suggest you the following 4-hour video:

        "Handmade Hero Chat 013 - Translation Units, Function Pointers, Compilation, Linking, and Execution"
        @/url:`https://www.youtube.com/watch?v=n4fI4eUTTKM`.

    Obviously you don't have to watch all that, but linking is a pretty important part of building programs.
    It's how things like DLLs and static libraries work, and I have to say, it can get pretty crazy!

    This file is the linker script, written in the scripting language by the GNU people.
    It's absolutely dogshit confusing.
    I'll admit, I'm not an expert on this at all.
    I'm very sure there's something in this file that's being done incorrectly,
    so take everything you read above with a handful of salt.

    However, what the linker script essentially does is define regions of the address space that certain sections should go to.
    For instance, constants like string literals typically get put into a read-only section like `.rodata`.
    In this event, we want to put it in the memory region we call FLASH.
    Other mutable data might be put into SRAM, but you can even go fine-tuner than that.

    For instance, there's something called Instruction-Tightly-Coupled-Memory (ITCM);
    this would be a region of memory on the microcontroller that can the CPU can fetch instructions from really quick,
    which can be pretty useful for low-latency critical appplications.
    Obviously, not all code can be put into ITCM since this region is typically much smaller than the inexpensive flash memory.
    So what you might then do is use the linker script to put certain hot functions (like interrupt handlers) into ITCM
    while leaving the rest the code in regular ol' flash memory.

    Anyways, this whole linker script thing is pretty archaic and opaque, so I can't speak much more on it than this.
    This file should pretty much be never changed unless we bring on another microcontroller or realized some
    critical issue with how something is defined in here.
*/
