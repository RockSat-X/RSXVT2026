.syntax unified


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// @/on 2025-july-25/by:`Phuc Doan`.
// The "HANDLER_Default_indirect" here just simply calls "HANDLER_Default" that's defined in a different object file.
// This is due to the fact that every interrupt routine is a weak symbol which the user can overwrite by defining the symbol;
// if the user doesn't do so, then the weak symbol defaults to "HANDLER_Default_indirect".
// The reason why it's not "HANDLER_Default" itself is because "HANDLER_Default" is a symbol that's also defined in a separate
// object file, and the limitation of weak symbols restrict it so that its default value can only be to something that's within
// the same object file the weak symbol is defined in.
// Annoying!
//

.section .text
.thumb_func
HANDLER_Default_indirect:
    b HANDLER_Default



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



.section .text
.thumb_func
.global HANDLER_Reset
HANDLER_Reset:



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

    ldr r0, =0xE000ED88                     // CPACR register address. @/pg 597/tbl B3-4/`ARMv7-M`.
    ldr r1, [r0]
    orr r1, r1, (0b11 << 22) | (0b11 << 20) // Full access to coprocessor 10 and 11. @/pg 614/sec B3.2.20/`ARMv7-M`.
    str r1, [r0]



    // Finished initializing, we begin main!

    b main



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



.section .vector_table, "a"

    .word LINK_stack_addr
    .word HANDLER_Reset

    #include "interrupt_vector_table.meta"
    /* #meta

        @Meta.ifs(TARGETS.mcus, style = '#if')
        def _(mcu):
            yield f'TARGET_MCU_IS_{mcu}'



            # Load the file which details the list of all interrupt routines supported by the MCU.

            interrupt_vector_table_file = root(f'./electrical/system/interrupt_vector_table_{mcu}.txt')

            if not interrupt_vector_table_file.is_file():
                with log(ansi = 'fg_red'):
                    log(f'[ERROR] Couldn\'t find "{interrupt_vector_table_file}"!')
                    log(f'        This is needed so I know what the interrupt vector table for {mcu} looks like.')
                    log(f'        See the reference manual for {mcu}.')
                raise MetaError



            # Parse the entries.

            entries = [entry.strip() for entry in interrupt_vector_table_file.read_text().splitlines()]
            entries = [None if entry == 'Reserved' else entry for entry in entries if not entry.startswith('#')]



            # Generate the assembly to define the interrupt vector table.

            for entryi, entry in enumerate(entries):

                Meta.line(f'''

                    // [{entryi - 14}] {'Reserved' if entry is None else entry}.

                ''')

                if entry is None:

                    # No interrupt handler here.
                    Meta.line('.word 0')

                else:

                    Meta.line(
                        f'.word HANDLER_{entry}'                           , # Insert the address of the handler routine.
                        f'.weak HANDLER_{entry}'                           , # If there exists an implementation of the handler, use it.
                        f'.set  HANDLER_{entry}, HANDLER_Default_indirect' , # Otherwise, use the default handler.
                    )
    */
