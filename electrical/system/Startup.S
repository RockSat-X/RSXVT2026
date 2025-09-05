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



    // For global variables that are initially non-zero, we copy it from flash and write it into RAM.

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



    // For any global variables that are initially zero, we zero out that part of the RAM.

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
    // @/pg 597/tbl B3-4/`Armv7-M`.
    // @/pg 1488/sec D1.2.14/`Armv8-M`.

    ldr r0, =0xE000ED88                     // CPACR register address.
    ldr r1, [r0]
    orr r1, r1, (0b11 << 22) | (0b11 << 20) // Full access to coprocessor 10 and 11.
    str r1, [r0]



    // Finished initializing, we begin main!

    b main



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



.section .vector_table, "a"

    .word LINK_stack_addr
    .word __INTERRUPT_Reset

    #include "interrupt_vector_table.meta"
    /* #meta



        # Ensure all FreeRTOS interrupts has a place in the IVT.

        for mcu in MCUS:
            if missing := [
                interrupt
                for freertos_interrupt in MCUS[mcu].freertos_interrupts
                if freertos_interrupt not in INTERRUPTS[mcu]
            ]:
                raise RuntimeError(f'FreeRTOS interrupts missing in {mcu} interrupt vector table: {list(missing)}.')



        # Make the IVT.

        for target in PER_TARGET():

            for interrupt_i, interrupt_name in enumerate(INTERRUPTS[target.mcu]):



                # Nice header.

                Meta.line(f'''

                    // [{interrupt_i - 14}] {'Reserved' if interrupt_name is None else interrupt_name}.

                ''')



                # No interrupt handler here.

                if interrupt_name is None:

                    Meta.line('.word __INTERRUPT_Default')



                # This interrupt will be supplied by FreeRTOS.

                elif target.use_freertos and interrupt_name in MCUS[target.mcu].freertos_interrupts:

                    Meta.line(f'.word {MCUS[target.mcu].freertos_interrupts[interrupt_name]}')



                # This interrupt must be defined by the user.

                elif interrupt_name in (name for name, niceness in target.interrupt_priorities):

                    Meta.line(f'.word __INTERRUPT_{interrupt_name}')



                # These interrupt routines will be implemented by the user, if at all.

                else:

                    Meta.line(
                        f'.word __INTERRUPT_{interrupt_name}'                               , # Insert the address of the handler routine.
                        f'.weak __INTERRUPT_{interrupt_name}'                               , # If there exists an implementation of the handler, use it.
                        f'.set  __INTERRUPT_{interrupt_name}, __INTERRUPT_Default_indirect' , # Otherwise, use the default handler.
                    )

    */



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// The main purpose of this file is to define the interrupt
// vector table and what the CPU immediately does after a reset.
//
// An STM32 microcontroller can undergo a reset for many reasons:
//
//     - The MCU got powered on.
//     - The reset pin on the MCU got pulled low.
//     - The watchdog timer expired.
//     - etc.
//
// In any event, the microcontroller get set back to a known
// state, more-or-less. So a good question is then: what is
// the very first instruction that the CPU execute?
//
// All STM32 microcontrollers we're working with use Arm Cortex-M CPUs,
// and these CPUs are defined by the instruction set architecture (ISA).
// According to the ISA, the first thing the CPU does is:
//
//     1. Read the 32-bit value at address VTOR   and store it into the Stack-Pointer register.
//     2. Read the 32-bit value at address VTOR+4 and store it into the Instruction-Pointer register (also called Program-Counter).
//
// VTOR is the "Vector Table Offset Register", the value of which
// is implementation-defined upon MCU reset. That is, ST (the company
// that makes the STM32 MCUs) will decide what the VTOR will be.
// A common reset value for VTOR is 0x0000'0000, but it need not be.
// @/pg 525/sec B1.5.3/`Armv7-M`.
//
// Thus, if we were to program the MCU, then we ought to put the address
// of the first instruction of our firmware at address VTOR+4, so that when
// the MCU is reset, the instruction pointer is set to the beginning of our
// code and it'll chug on from there.
//
// When we program an MCU, what we're really doing is overwriting its flash
// memory. So if we want to properly initialize the instruction pointer, we
// must account for this.
//
// So if you go ahead and read the microcontroller's reference manual:
//
//     - @/pg 117/fig 3/`H533rm`.
//     - @/pg 151/fig 3/`H7S3rm`.
//
// ... then you'll see that the microcontroller's flash memory can be found at 0x0800'0000.
//
// And luckily, if consult the same reference manual again:
//
//    - @/pg 171/tbl 25/`H533rm`.
//    - @/pg 163/tbl 15/`H7S3rm`.
//
// The reset value of VTOR will be 0x0800'0000!
// Of course, for this to be the case, there must be some conditions met.
// One of them is that the BOOT0 pin must be zero (which on the physical
// pin of the MCU would mean pulled to ground), but in 99% of all cases,
// the VTOR value on reset will be 0x0800'0000 which is right where flash is.
//
// The initial SP and PC values at VTOR and VTOR+4 are actually part of
// a much larger structure, that structure being the interrupt vector table.
// The interrupt vector table is just a massive look-up table for whenever
// a particular event happens, like a timer counter overflowing, a SPI buffer
// TX-buffer empty, etc, the CPU knows what to immediately do next. To see the
// full list of interrupts, check out "build/interrupt_vector_table.meta". Most
// of the interrupts we'll never use, but we still have to put something there,
// so have a default interrupt handler to deal with that.
//
// At VTOR+4, we put the address of our "reset interrupt handler". It's not actually
// really an interrupt handler; it's just where code will start executing
// when a reset event happens, but it's the terminology that's used sometimes.
//
// Anyways, before we can go into `main`, we have to do some quick prep work.
// This pretty much involves initializing global variables and turning on the FPU;
// the reason why we turn on the FPU is because FPU instructions can pop/push a
// large amount of data all at once, so it's more efficient to use something like
// that than non-FPU instructions. This startup code is pretty straight-forward;
// in C++, it's much more complicated, because a bunch of constructors and destructors
// have to be invoked before `main` can start. When you get to application-space
// programs, like making a Windows program, even more complicated OS-specific stuff
// happens like loading DLLs and whatnot.
//
// In conclusion, this file makes a interrupt vector table of the addresses to
// interrupt handlers and some code to initialize stuff before we start `main`.
// This file is rarely ever changed, but it's cool to see how stuff works under the hood!
