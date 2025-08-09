.syntax unified



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// @/by:`Phuc Doan`.
// The "__INTERRUPT_Default_indirect" here just simply calls "__INTERRUPT_Default" that's defined in a different object file.
// This is due to the fact that every interrupt routine is a weak symbol which the user can overwrite by defining the symbol;
// if the user doesn't do so, then the weak symbol defaults to "__INTERRUPT_Default_indirect".
// The reason why it's not "__INTERRUPT_Default" itself is because "__INTERRUPT_Default" is a symbol that's also defined in a separate
// object file, and the limitation of weak symbols restrict it so that its default value can only be to something that's within
// the same object file the weak symbol is defined in.
// Annoying!
//

.section .text
.thumb_func
__INTERRUPT_Default_indirect:
    b __INTERRUPT_Default



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



.section .text
.thumb_func
.global __INTERRUPT_Reset
__INTERRUPT_Reset:



    // Move bytes from LMA to VMA for the data section.

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



    // Zero-initialize BSS section.

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
    // Must be done here since GCC can emit vector push/pop instructions on entering main
    // that'd otherwise emit an illegal instruction fault.

    ldr r0, =0xE000ED88                     // CPACR register address. @/pg 597/tbl B3-4/`Armv7-M`.
    ldr r1, [r0]
    orr r1, r1, (0b11 << 22) | (0b11 << 20) // Full access to coprocessor 10 and 11. @/pg 614/sec B3.2.20/`Armv7-M`.
    str r1, [r0]



    // Finished initializing, we begin main!

    b main



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



.section .vector_table, "a"

    .word LINK_stack_addr
    .word __INTERRUPT_Reset

    #include "interrupt_vector_table.meta"
    /* #meta

        @Meta.ifs(TARGETS.mcus, '#if')
        def _(mcu):

            yield f'TARGET_MCU_IS_{mcu}'

            for interrupt_i, interrupt in enumerate(INTERRUPTS[mcu]):



                # Nice header.

                Meta.line(f'''

                    // [{interrupt_i - 14}] {interrupt}.

                ''')



                # No interrupt handler here.

                if interrupt == 'Reserved':
                    Meta.line('.word 0')



                # These interrupt routines are implemented by FreeRTOS.

                elif interrupt in INTERRUPTS_FOR_FREERTOS:
                    Meta.line(f'.word {INTERRUPTS_FOR_FREERTOS[interrupt]}')



                # These interrupt routines will be implemented by the user, if at all.

                else:
                    Meta.line(
                        f'.word __INTERRUPT_{interrupt}'                               , # Insert the address of the handler routine.
                        f'.weak __INTERRUPT_{interrupt}'                               , # If there exists an implementation of the handler, use it.
                        f'.set  __INTERRUPT_{interrupt}, __INTERRUPT_Default_indirect' , # Otherwise, use the default handler.
                    )

    */
