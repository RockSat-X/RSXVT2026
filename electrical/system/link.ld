#define CONCAT_(X, Y) X##Y
#define CONCAT(X, Y)  CONCAT_(X, Y)

ENTRY(HANDLER_Reset) /* Just to indicate where program execution would begin for debugging purposes; doesn't actually affect anything. */

MEMORY
{
    #if TARGET_NAME_IS_SandboxNucleoH7S3L8 /* @/pg 151/fig 3/`H7S3rm`. */
        AXI_SRAM (rw) : ORIGIN = 0x24000000, LENGTH = 456K
        DTCM     (rw) : ORIGIN = 0x20000000, LENGTH = 64K /* TODO There's actually 192K worth of memory available to use, but we might have to program the option bytes? */
        FLASH    (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    #endif
}

SECTIONS
{
    /**************************************************************** FLASH ****************************************************************/

    .vector_table_output_section :
    {
        KEEP(*(.vector_table))
    } > FLASH

    .text_output_section : ALIGN(4)
    {
        *(.text*)
    } > FLASH

    .rodata_output_section : ALIGN(4)
    {
        *(.rodata*)
    } > FLASH

    .eh_frame_output_section  : { *(.eh_frame*)  } > FLASH
    .ARM_exidx_output_section : { *(.ARM.exidx*) } > FLASH

    /**************************************************************** RAM ****************************************************************/

    .stack_output_section : ALIGN(32) /* For MPU region alignment. TODO If we're going to use the MPU to detect stack overflow? */
    {
        .               += LINK_stack_size;
        LINK_stack_addr  = .;

        #define STACK_LOCATION_SandboxNucleoH7S3L8 DTCM
    } > CONCAT(STACK_LOCATION_, TARGET_NAME)

    .data_output_section : ALIGN(4)
    {
        LINK_data_load_addr_start = LOADADDR(.data_output_section);
        LINK_data_virt_addr_start = .;
        *(.data)
        .                       = ALIGN(4);
        LINK_data_virt_addr_end = .;

        #define DATA_LOCATION_SandboxNucleoH7S3L8 AXI_SRAM
    } > CONCAT(DATA_LOCATION_, TARGET_NAME) AT > FLASH

    .bss_output_section : ALIGN(4)
    {
        LINK_bss_addr_start = .;
        *(.bss)
        .                 = ALIGN(4);
        LINK_bss_addr_end = .;

        #define BSS_LOCATION_SandboxNucleoH7S3L8 AXI_SRAM
    } > CONCAT(BSS_LOCATION_, TARGET_NAME)
}
