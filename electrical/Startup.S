.syntax unified

.section .text
.thumb_func
.global INTERRUPT_Reset
INTERRUPT_Reset:



    // For global variables that are initially non-zero,
    // we copy it from flash and write it into RAM.

    ldr r0, =LINK_data_load_addr_start
    ldr r1, =LINK_data_virt_addr_start
    ldr r2, =LINK_data_virt_addr_end

    b is_copying_data_done
    copy_data:
        ldr r3, [r0]
        str r3, [r1]
        add r0, 4
        add r1, 4
    is_copying_data_done:
        cmp r1, r2
        bne copy_data



    // For any global variables that are initially zero,
    // we zero out that part of the RAM.

    mov r0, 0
    ldr r1, =LINK_bss_addr_start
    ldr r2, =LINK_bss_addr_end

    b is_zeroing_bss_done
    zero_bss:
        str r0, [r1]
        add r1, 4
    is_zeroing_bss_done:
        cmp r1, r2
        bne zero_bss



    // Enable the floating point coprocessor.
    // Must be done here since GCC can emit
    // vector push/pop instructions on entering main
    // that'd otherwise emit an illegal instruction fault.
    // @/pg 597/tbl B3-4/`Armv7-M`.
    // @/pg 1488/sec D1.2.14/`Armv8-M`.

    ldr r0, =0xE000ED88                     // CPACR register address.
    ldr r1, [r0]
    orr r1, r1, (0b11 << 22) | (0b11 << 20) // Full access to coprocessor 10 and 11.
    str r1, [r0]



    // Finished initializing, we begin main!

    b main
