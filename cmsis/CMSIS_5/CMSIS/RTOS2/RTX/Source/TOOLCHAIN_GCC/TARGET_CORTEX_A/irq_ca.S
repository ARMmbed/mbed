/*
 * Copyright (c) 2013-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       ARMv7-A Exception handlers
 *
 * -----------------------------------------------------------------------------
 */

                .syntax  unified

                .equ   MODE_FIQ,        0x11
                .equ   MODE_IRQ,        0x12
                .equ   MODE_SVC,        0x13
                .equ   MODE_ABT,        0x17
                .equ   MODE_UND,        0x1B

                .equ   CPSR_BIT_T,      0x20

                .equ   K_STATE_RUNNING, 2           // osKernelState_t::osKernelRunning
                .equ   I_K_STATE_OFS,   8           // osRtxInfo.kernel.state offset
                .equ   I_TICK_IRQN_OFS, 16          // osRtxInfo.tick_irqn offset
                .equ   I_T_RUN_OFS,     20          // osRtxInfo.thread.run offset
                .equ   TCB_SP_FRAME,    34          // osRtxThread_t.stack_frame offset
                .equ   TCB_SP_OFS,      56          // osRtxThread_t.sp offset


                .section ".rodata"
                .global  irqRtxLib                  // Non weak library reference
irqRtxLib:
                .byte    0

                .section ".data"
                .global  IRQ_PendSV
IRQ_NestLevel:
                .word    0                          // IRQ nesting level counter
IRQ_PendSV:
                .byte    0                          // Pending SVC flag

                .arm
                .section ".text"
                .align   4


                .type    Undef_Handler, %function
                .global  Undef_Handler
                .fnstart
                .cantunwind
Undef_Handler:

                srsfd   sp!, #MODE_UND
                push    {r0-r4, r12}                // Save APCS corruptible registers to UND mode stack

                mrs     r0, spsr
                tst     r0, #CPSR_BIT_T             // Check mode
                moveq   r1, #4                      // R1 = 4 ARM mode
                movne   r1, #2                      // R1 = 2 Thumb mode
                sub     r0, lr, r1
                ldreq   r0, [r0]                    // ARM mode - R0 points to offending instruction
                beq     Undef_Cont

                // Thumb instruction
                // Determine if it is a 32-bit Thumb instruction
                ldrh    r0, [r0]
                mov     r2, #0x1C
                cmp     r2, r0, lsr #11
                bhs     Undef_Cont                  // 16-bit Thumb instruction

                // 32-bit Thumb instruction. Unaligned - reconstruct the offending instruction
                ldrh    r2, [lr]
                orr     r0, r2, r0, lsl #16
Undef_Cont:
                mov     r2, lr                      // Set LR to third argument

                and     r12, sp, #4                 // Ensure stack is 8-byte aligned
                sub     sp, sp, r12                 // Adjust stack
                push    {r12, lr}                   // Store stack adjustment and dummy LR

                // R0 =Offending instruction, R1 =2(Thumb) or =4(ARM)
                bl      CUndefHandler

                pop     {r12, lr}                   // Get stack adjustment & discard dummy LR
                add     sp, sp, r12                 // Unadjust stack

                ldr     lr, [sp, #24]               // Restore stacked LR and possibly adjust for retry
                sub     lr, lr, r0
                ldr     r0, [sp, #28]               // Restore stacked SPSR
                msr     spsr_cxsf, r0
                clrex                               // Clear exclusive monitor
                pop     {r0-r4, r12}                // Restore stacked APCS registers
                add     sp, sp, #8                  // Adjust SP for already-restored banked registers
                movs    pc, lr

                .fnend
                .size    Undef_Handler, .-Undef_Handler


                .type    PAbt_Handler, %function
                .global  PAbt_Handler
                .fnstart
                .cantunwind
PAbt_Handler:

                sub     lr, lr, #4                  // Pre-adjust LR
                srsfd   sp!, #MODE_ABT              // Save LR and SPRS to ABT mode stack
                push    {r0-r4, r12}                // Save APCS corruptible registers to ABT mode stack
                mrc     p15, 0, r0, c5, c0, 1       // IFSR
                mrc     p15, 0, r1, c6, c0, 2       // IFAR

                mov     r2, lr                      // Set LR to third argument

                and     r12, sp, #4                 // Ensure stack is 8-byte aligned
                sub     sp, sp, r12                 // Adjust stack
                push    {r12, lr}                   // Store stack adjustment and dummy LR

                bl      CPAbtHandler

                pop     {r12, lr}                   // Get stack adjustment & discard dummy LR
                add     sp, sp, r12                 // Unadjust stack

                clrex                               // Clear exclusive monitor
                pop     {r0-r4, r12}                // Restore stack APCS registers
                rfefd   sp!                         // Return from exception

                .fnend
                .size    PAbt_Handler, .-PAbt_Handler


                .type    DAbt_Handler, %function
                .global  DAbt_Handler
                .fnstart
                .cantunwind
DAbt_Handler:
                sub     lr, lr, #8                  // Pre-adjust LR
                srsfd   sp!, #MODE_ABT              // Save LR and SPRS to ABT mode stack
                push    {r0-r4, r12}                // Save APCS corruptible registers to ABT mode stack
                mrc     p15, 0, r0, c5, c0, 0       // DFSR
                mrc     p15, 0, r1, c6, c0, 0       // DFAR

                mov     r2, lr                      // Set LR to third argument

                and     r12, sp, #4                 // Ensure stack is 8-byte aligned
                sub     sp, sp, r12                 // Adjust stack
                push    {r12, lr}                   // Store stack adjustment and dummy LR

                bl      CDAbtHandler

                pop     {r12, lr}                   // Get stack adjustment & discard dummy LR
                add     sp, sp, r12                 // Unadjust stack

                clrex                               // Clear exclusive monitor
                pop     {r0-r4, r12}                // Restore stacked APCS registers
                rfefd   sp!                         // Return from exception

                .fnend
                .size    DAbt_Handler, .-DAbt_Handler


                .type    IRQ_Handler, %function
                .global  IRQ_Handler
                .fnstart
                .cantunwind
IRQ_Handler:

                sub     lr, lr, #4                  // Pre-adjust LR
                srsfd   sp!, #MODE_SVC              // Save LR_irq and SPSR_irq on to the SVC stack
                cps     #MODE_SVC                   // Change to SVC mode
                push    {r0-r3, r12, lr}            // Save APCS corruptible registers

                ldr     r0, =IRQ_NestLevel
                ldr     r1, [r0]
                add     r1, r1, #1                  // Increment IRQ nesting level
                str     r1, [r0]

                mov     r3, sp                      // Move SP into R3
                and     r3, r3, #4                  // Get stack adjustment to ensure 8-byte alignment
                sub     sp, sp, r3                  // Adjust stack
                push    {r3, r4}                    // Store stack adjustment(R3) and user data(R4)

                blx     IRQ_GetActiveIRQ            // Retrieve interrupt ID into R0
                mov     r4, r0                      // Move interrupt ID to R4

                blx     IRQ_GetHandler              // Retrieve interrupt handler address for current ID
                cmp     r0, #0                      // Check if handler address is 0
                beq     IRQ_End                     // If 0, end interrupt and return

                cpsie   i                           // Re-enable interrupts
                blx     r0                          // Call IRQ handler
                cpsid   i                           // Disable interrupts

IRQ_End:
                mov     r0, r4                      // Move interrupt ID to R0
                blx     IRQ_EndOfInterrupt          // Signal end of interrupt

                pop     {r3, r4}                    // Restore stack adjustment(R3) and user data(R4)
                add     sp, sp, r3                  // Unadjust stack

                bl      osRtxContextSwitch          // Continue in context switcher

                ldr     r0, =IRQ_NestLevel
                ldr     r1, [r0]
                subs    r1, r1, #1                  // Decrement IRQ nesting level
                str     r1, [r0]

                clrex                               // Clear exclusive monitor for interrupted code
                pop     {r0-r3, r12, lr}            // Restore stacked APCS registers
                rfefd   sp!                         // Return from IRQ handler

                .fnend
                .size    IRQ_Handler, .-IRQ_Handler


                .type    SVC_Handler, %function
                .global  SVC_Handler
                .fnstart
                .cantunwind
SVC_Handler:

                srsfd   sp!, #MODE_SVC              // Store SPSR_svc and LR_svc onto SVC stack
                push    {r12, lr}

                mrs     r12, spsr                   // Load SPSR
                tst     r12, #CPSR_BIT_T            // Thumb bit set?
                ldrhne  r12, [lr,#-2]               // Thumb: load halfword
                bicne   r12, r12, #0xFF00           //        extract SVC number
                ldreq   r12, [lr,#-4]               // ARM:   load word
                biceq   r12, r12, #0xFF000000       //        extract SVC number
                cmp     r12, #0                     // Compare SVC number
                bne     SVC_User                    // Branch if User SVC

                push    {r0-r3}

                ldr     r0, =IRQ_NestLevel
                ldr     r1, [r0]
                add     r1, r1, #1                  // Increment IRQ nesting level
                str     r1, [r0]

                ldr     r0, =osRtxInfo
                ldr     r1, [r0, #I_K_STATE_OFS]    // Load RTX5 kernel state
                cmp     r1, #K_STATE_RUNNING        // Check osKernelRunning
                blt     SVC_FuncCall                // Continue if kernel is not running
                ldr     r0, [r0, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                blx     IRQ_Disable                 // Disable OS Tick interrupt
SVC_FuncCall:
                pop     {r0-r3}

                ldr     r12, [sp]                   // Reload R12 from stack

                cpsie   i                           // Re-enable interrupts
                blx     r12                         // Branch to SVC function
                cpsid   i                           // Disable interrupts

                sub     sp, sp, #4
                stm     sp, {sp}^                   // Store SP_usr onto stack
                pop     {r12}                       // Pop SP_usr into R12
                sub     r12, r12, #16               // Adjust pointer to SP_usr
                ldmdb   r12, {r2,r3}                // Load return values from SVC function
                push    {r0-r3}                     // Push return values to stack

                ldr     r0, =osRtxInfo
                ldr     r1, [r0, #I_K_STATE_OFS]    // Load RTX5 kernel state
                cmp     r1, #K_STATE_RUNNING        // Check osKernelRunning
                blt     SVC_ContextCheck            // Continue if kernel is not running
                ldr     r0, [r0, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                blx     IRQ_Enable                  // Enable OS Tick interrupt

SVC_ContextCheck:
                bl      osRtxContextSwitch          // Continue in context switcher

                ldr     r0, =IRQ_NestLevel
                ldr     r1, [r0]
                sub     r1, r1, #1                  // Decrement IRQ nesting level
                str     r1, [r0]

                clrex                               // Clear exclusive monitor
                pop     {r0-r3, r12, lr}            // Restore stacked APCS registers
                rfefd   sp!                         // Return from exception

SVC_User:
                push    {r4, r5}
                ldr     r5,=osRtxUserSVC            // Load address of SVC table
                ldr     r4,[r5]                     // Load SVC maximum number
                cmp     r12,r4                      // Check SVC number range
                bhi     SVC_Done                    // Branch if out of range

                ldr     r12,[r5,r12,lsl #2]         // Load SVC Function Address
                blx     r12                         // Call SVC Function

SVC_Done:
                clrex                               // Clear exclusive monitor
                pop     {r4, r5, r12, lr}
                rfefd   sp!                         // Return from exception

                .fnend
                .size    SVC_Handler, .-SVC_Handler


                .type    osRtxContextSwitch, %function
                .global  osRtxContextSwitch
                .fnstart
                .cantunwind
osRtxContextSwitch:

                push    {lr}

                // Check interrupt nesting level
                ldr     r0, =IRQ_NestLevel
                ldr     r1, [r0]                    // Load IRQ nest level
                cmp     r1, #1
                bne     osRtxContextExit            // Nesting interrupts, exit context switcher

                ldr     r12, =osRtxInfo+I_T_RUN_OFS // Load address of osRtxInfo.run
                ldm     r12, {r0, r1}               // Load osRtxInfo.thread.run: curr & next
                ldr     r2, =IRQ_PendSV             // Load address of IRQ_PendSV flag
                ldrb    r3, [r2]                    // Load PendSV flag

                cmp     r0, r1                      // Check if context switch is required
                bne     osRtxContextCheck           // Not equal, check if context save required
                cmp     r3, #1                      // Compare IRQ_PendSV value
                bne     osRtxContextExit            // No post processing (and no context switch requested)

osRtxContextCheck:
                str     r1, [r12]                   // Store run.next as run.curr
                // R0 = curr, R1 = next, R2 = &IRQ_PendSV, R3 = IRQ_PendSV, R12 = &osRtxInfo.thread.run
                push    {r1-r3, r12}

                cmp     r0, #0                      // Is osRtxInfo.thread.run.curr == 0
                beq     osRtxPostProcess            // Current deleted, skip context save

osRtxContextSave:
                mov     lr, r0                      // Move &osRtxInfo.thread.run.curr to LR
                mov     r0, sp                      // Move SP_svc into R0
                add     r0, r0, #20                 // Adjust SP_svc to R0 of the basic frame
                sub     sp, sp, #4
                stm     sp, {sp}^                   // Save SP_usr to current stack
                pop     {r1}                        // Pop SP_usr into R1

                sub     r1, r1, #64                 // Adjust SP_usr to R4 of the basic frame
                stmia   r1!, {r4-r11}               // Save R4-R11 to user stack
                ldmia   r0!, {r4-r8}                // Load stacked R0-R3,R12 into R4-R8
                stmia   r1!, {r4-r8}                // Store them to user stack
                stm     r1, {lr}^                   // Store LR_usr directly
                add     r1, r1, #4                  // Adjust user sp to PC
                ldmib   r0!, {r5-r6}                // Load current PC, CPSR
                stmia   r1!, {r5-r6}                // Restore user PC and CPSR

                sub     r1, r1, #64                 // Adjust SP_usr to stacked R4

                // Check if VFP state need to be saved
                mrc     p15, 0, r2, c1, c0, 2       // VFP/NEON access enabled? (CPACR)
                and     r2, r2, #0x00F00000
                cmp     r2, #0x00F00000
                bne     osRtxContextSaveSP          // Continue, no VFP

                vmrs    r2, fpscr
                stmdb   r1!, {r2,r12}               // Push FPSCR, maintain 8-byte alignment

                vstmdb  r1!, {d0-d15}               // Save D0-D15
              #if defined(__ARM_NEON) && (__ARM_NEON == 1)
                vstmdb  r1!, {d16-d31}              // Save D16-D31
              #endif

                ldrb    r2, [lr, #TCB_SP_FRAME]     // Load osRtxInfo.thread.run.curr frame info
              #if defined(__ARM_NEON) && (__ARM_NEON == 1)
                orr     r2, r2, #4                  // NEON state
              #else
                orr     r2, r2, #2                  // VFP state
              #endif
                strb    r2, [lr, #TCB_SP_FRAME]     // Store VFP/NEON state

osRtxContextSaveSP:
                str     r1, [lr, #TCB_SP_OFS]       // Store user sp to osRtxInfo.thread.run.curr

osRtxPostProcess:
                // RTX IRQ post processing check
                pop     {r8-r11}                    // Pop R8 = run.next, R9 = &IRQ_PendSV, R10 = IRQ_PendSV, R11 = &osRtxInfo.thread.run
                cmp     r10, #1                     // Compare PendSV value
                bne     osRtxContextRestore         // Skip post processing if not pending

                mov     r4, sp                      // Move SP_svc into R4
                and     r4, r4, #4                  // Get stack adjustment to ensure 8-byte alignment
                sub     sp, sp, r4                  // Adjust stack

                // Disable OS Tick
                ldr     r5, =osRtxInfo              // Load address of osRtxInfo
                ldr     r5, [r5, #I_TICK_IRQN_OFS]  // Load OS Tick irqn
                mov     r0, r5                      // Set it as function parameter
                blx     IRQ_Disable                 // Disable OS Tick interrupt
                mov     r6, #0                      // Set PendSV clear value
                b       osRtxPendCheck
osRtxPendExec:
                strb    r6, [r9]                    // Clear PendSV flag
                cpsie   i                           // Re-enable interrupts
                blx     osRtxPendSV_Handler         // Post process pending objects
                cpsid   i                           // Disable interrupts
osRtxPendCheck:
                ldr     r8, [r11, #4]               // Load osRtxInfo.thread.run.next
                str     r8, [r11]                   // Store run.next as run.curr
                ldrb    r0, [r9]                    // Load PendSV flag
                cmp     r0, #1                      // Compare PendSV value
                beq     osRtxPendExec               // Branch to PendExec if PendSV is set

                // Re-enable OS Tick
                mov     r0, r5                      // Restore irqn as function parameter
                blx     IRQ_Enable                  // Enable OS Tick interrupt

                add     sp, sp, r4                  // Restore stack adjustment

osRtxContextRestore:
                ldr     lr, [r8, #TCB_SP_OFS]       // Load next osRtxThread_t.sp
                ldrb    r2, [r8, #TCB_SP_FRAME]     // Load next osRtxThread_t.stack_frame

                ands    r2, r2, #0x6                // Check stack frame for VFP context
                mrc     p15, 0, r2, c1, c0, 2       // Read CPACR
                andeq   r2, r2, #0xFF0FFFFF         // VFP/NEON state not stacked, disable VFP/NEON
                orrne   r2, r2, #0x00F00000         // VFP/NEON state is stacked, enable VFP/NEON
                mcr     p15, 0, r2, c1, c0, 2       // Write CPACR
                beq     osRtxContextRestoreRegs     // No VFP
                isb                                 // Sync if VFP was enabled
              #if defined(__ARM_NEON) && (__ARM_NEON == 1)
                vldmia  lr!, {d16-d31}              // Restore D16-D31
              #endif
                vldmia  lr!, {d0-d15}               // Restore D0-D15
                ldr     r2, [lr]
                vmsr    fpscr, r2                   // Restore FPSCR
                add     lr, lr, #8                  // Adjust sp pointer to R4

osRtxContextRestoreRegs:
                ldmia   lr!, {r4-r11}               // Restore R4-R11
                add     r12, lr, #32                // Adjust sp and save it into R12
                push    {r12}                       // Push sp onto stack
                ldm     sp, {sp}^                   // Restore SP_usr directly
                add     sp, sp, #4                  // Adjust SP_svc
                ldmia   lr!, {r0-r3, r12}           // Load user registers R0-R3,R12
                stmib   sp!, {r0-r3, r12}           // Store them to SP_svc
                ldm     lr, {lr}^                   // Restore LR_usr directly
                ldmib   lr!, {r0-r1}                // Load user registers PC,CPSR
                add     sp, sp, #4
                stmib   sp!, {r0-r1}                // Store them to SP_svc
                sub     sp, sp, #32                 // Adjust SP_svc to stacked LR

osRtxContextExit:
                pop     {pc}                        // Return

                .fnend
                .size    osRtxContextSwitch, .-osRtxContextSwitch

                .end
