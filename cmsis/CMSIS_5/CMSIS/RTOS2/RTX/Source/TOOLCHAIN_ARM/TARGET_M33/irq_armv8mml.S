;/*
; * Copyright (c) 2016-2021 Arm Limited. All rights reserved.
; *
; * SPDX-License-Identifier: Apache-2.0
; *
; * Licensed under the Apache License, Version 2.0 (the License); you may
; * not use this file except in compliance with the License.
; * You may obtain a copy of the License at
; *
; * www.apache.org/licenses/LICENSE-2.0
; *
; * Unless required by applicable law or agreed to in writing, software
; * distributed under the License is distributed on an AS IS BASIS, WITHOUT
; * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; * See the License for the specific language governing permissions and
; * limitations under the License.
; *
; * -----------------------------------------------------------------------------
; *
; * Project:     CMSIS-RTOS RTX
; * Title:       ARMv8-M Mainline Exception handlers
; *
; * -----------------------------------------------------------------------------
; */


                IF       :LNOT::DEF:RTX_STACK_CHECK
RTX_STACK_CHECK EQU      0
                ENDIF

                IF       :LNOT::DEF:DOMAIN_NS
DOMAIN_NS       EQU      0
                ENDIF

                IF       ({FPU}="FPv5-SP") || ({FPU}="FPv5_D16")
FPU_USED        EQU      1
                ELSE
FPU_USED        EQU      0
                ENDIF

I_T_RUN_OFS     EQU      20                     ; osRtxInfo.thread.run offset
TCB_SM_OFS      EQU      48                     ; TCB.stack_mem offset
TCB_SP_OFS      EQU      56                     ; TCB.SP offset
TCB_SF_OFS      EQU      34                     ; TCB.stack_frame offset
TCB_TZM_OFS     EQU      64                     ; TCB.tz_memory offset

FPCCR           EQU      0xE000EF34             ; FPCCR Address

osRtxErrorStackOverflow\
                EQU      1                      ; Stack overflow


                PRESERVE8
                THUMB


                AREA     |.constdata|, DATA, READONLY
                EXPORT   irqRtxLib
irqRtxLib       DCB      0                      ; Non weak library reference


                AREA     |.text|, CODE, READONLY


SVC_Handler     PROC
                EXPORT   SVC_Handler
                IMPORT   osRtxUserSVC
                IMPORT   osRtxInfo
            IF RTX_STACK_CHECK != 0
                IMPORT   osRtxThreadStackCheck
                IMPORT   osRtxKernelErrorNotify
            ENDIF
            IF DOMAIN_NS != 0
                IMPORT   TZ_LoadContext_S
                IMPORT   TZ_StoreContext_S
            ENDIF

                TST      LR,#0x04               ; Determine return stack from EXC_RETURN bit 2
                ITE      EQ
                MRSEQ    R0,MSP                 ; Get MSP if return stack is MSP
                MRSNE    R0,PSP                 ; Get PSP if return stack is PSP

                LDR      R1,[R0,#24]            ; Load saved PC from stack
                LDRB     R1,[R1,#-2]            ; Load SVC number
                CMP      R1,#0                  ; Check SVC number
                BNE      SVC_User               ; Branch if not SVC 0

                PUSH     {R0,LR}                ; Save SP and EXC_RETURN
                LDM      R0,{R0-R3,R12}         ; Load function parameters and address from stack
                BLX      R12                    ; Call service function
                POP      {R12,LR}               ; Restore SP and EXC_RETURN
                STM      R12,{R0-R1}            ; Store function return values

SVC_Context
                LDR      R3,=osRtxInfo+I_T_RUN_OFS; Load address of osRtxInfo.thread.run
                LDM      R3,{R1,R2}             ; Load osRtxInfo.thread.run: curr & next
                CMP      R1,R2                  ; Check if thread switch is required
                IT       EQ
                BXEQ     LR                     ; Exit when threads are the same

                STR      R2,[R3]                ; osRtxInfo.thread.run: curr = next

              IF FPU_USED != 0
                CBNZ     R1,SVC_ContextSave     ; Branch if running thread is not deleted
SVC_FP_LazyState
                TST      LR,#0x10               ; Determine stack frame from EXC_RETURN bit 4
                BNE      SVC_ContextRestore     ; Branch if not extended stack frame
                LDR      R3,=FPCCR              ; FPCCR Address
                LDR      R0,[R3]                ; Load FPCCR
                BIC      R0,R0,#1               ; Clear LSPACT (Lazy state preservation)
                STR      R0,[R3]                ; Store FPCCR
                B        SVC_ContextRestore     ; Branch to context restore handling
              ELSE
                CBZ      R1,SVC_ContextRestore  ; Branch if running thread is deleted
              ENDIF

SVC_ContextSave
            IF DOMAIN_NS != 0
                LDR      R0,[R1,#TCB_TZM_OFS]   ; Load TrustZone memory identifier
                CBZ      R0,SVC_ContextSave_NS  ; Branch if there is no secure context
                PUSH     {R1,R2,R12,LR}         ; Save registers and EXC_RETURN
                BL       TZ_StoreContext_S      ; Store secure context
                POP      {R1,R2,R12,LR}         ; Restore registers and EXC_RETURN
            ENDIF

SVC_ContextSave_NS
            IF DOMAIN_NS != 0
                TST      LR,#0x40               ; Check domain of interrupted thread
                BNE      SVC_ContextSaveSP      ; Branch if secure
            ENDIF

            IF RTX_STACK_CHECK != 0
                SUB      R12,R12,#32            ; Calculate SP: space for R4..R11
              IF FPU_USED != 0
                TST      LR,#0x10               ; Determine stack frame from EXC_RETURN bit 4
                IT       EQ                     ; If extended stack frame
                SUBEQ    R12,R12,#64            ;  Additional space for S16..S31
              ENDIF

SVC_ContextSaveSP
                STR      R12,[R1,#TCB_SP_OFS]   ; Store SP
                STRB     LR, [R1,#TCB_SF_OFS]   ; Store stack frame information

                PUSH     {R1,R2}                ; Save osRtxInfo.thread.run: curr & next
                MOV      R0,R1                  ; Parameter: osRtxInfo.thread.run.curr
                BL       osRtxThreadStackCheck  ; Check if thread stack is overrun
                POP      {R1,R2}                ; Restore osRtxInfo.thread.run: curr & next
                CBNZ     R0,SVC_ContextSaveRegs ; Branch when stack check is ok

              IF FPU_USED != 0
                MOV      R4,R1                  ; Save osRtxInfo.thread.run.curr
              ENDIF
                MOV      R0,#osRtxErrorStackOverflow ; Parameter: r0=code, r1=object_id
                BL       osRtxKernelErrorNotify      ; Call osRtxKernelErrorNotify
                LDR      R3,=osRtxInfo+I_T_RUN_OFS   ; Load address of osRtxInfo.thread.run
                LDR      R2,[R3,#4]             ; Load osRtxInfo.thread.run: next
                STR      R2,[R3]                ; osRtxInfo.thread.run: curr = next
              IF FPU_USED != 0
                LDRB     LR,[R4,#TCB_SF_OFS]    ; Load stack frame information
                B        SVC_FP_LazyState       ; Branch to FP lazy state handling
              ELSE
                B        SVC_ContextRestore     ; Branch to context restore handling
              ENDIF

SVC_ContextSaveRegs
                LDRB     LR,[R1,#TCB_SF_OFS]    ; Load stack frame information
              IF DOMAIN_NS != 0
                TST      LR,#0x40               ; Check domain of interrupted thread
                BNE      SVC_ContextRestore     ; Branch if secure
              ENDIF
                LDR      R12,[R1,#TCB_SP_OFS]   ; Load SP
              IF FPU_USED != 0
                TST      LR,#0x10               ; Determine stack frame from EXC_RETURN bit 4
                IT       EQ                     ; If extended stack frame
                VSTMIAEQ R12!,{S16-S31}         ;  Save VFP S16..S31
              ENDIF
                STM      R12,{R4-R11}           ; Save R4..R11
            ELSE
                STMDB    R12!,{R4-R11}          ; Save R4..R11
              IF FPU_USED != 0
                TST      LR,#0x10               ; Determine stack frame from EXC_RETURN bit 4
                IT       EQ                     ; If extended stack frame
                VSTMDBEQ R12!,{S16-S31}         ;  Save VFP S16.S31
              ENDIF
SVC_ContextSaveSP
                STR      R12,[R1,#TCB_SP_OFS]   ; Store SP
                STRB     LR, [R1,#TCB_SF_OFS]   ; Store stack frame information
            ENDIF

SVC_ContextRestore
            IF DOMAIN_NS != 0
                LDR      R0,[R2,#TCB_TZM_OFS]   ; Load TrustZone memory identifier
                CBZ      R0,SVC_ContextRestore_NS; Branch if there is no secure context
                PUSH     {R2,R3}                ; Save registers
                BL       TZ_LoadContext_S       ; Load secure context
                POP      {R2,R3}                ; Restore registers
            ENDIF

SVC_ContextRestore_NS
                LDR      R0,[R2,#TCB_SP_OFS]    ; Load SP
                LDR      R1,[R2,#TCB_SM_OFS]    ; Load stack memory base
                MSR      PSPLIM,R1              ; Set PSPLIM
                LDRB     R1,[R2,#TCB_SF_OFS]    ; Load stack frame information
                ORN      LR,R1,#0xFF            ; Set EXC_RETURN

            IF DOMAIN_NS != 0
                TST      LR,#0x40               ; Check domain of interrupted thread
                BNE      SVC_ContextRestoreSP   ; Branch if secure
            ENDIF

              IF FPU_USED != 0
                TST      LR,#0x10               ; Determine stack frame from EXC_RETURN bit 4
                IT       EQ                     ; If extended stack frame
                VLDMIAEQ R0!,{S16-S31}          ;  Restore VFP S16..S31
              ENDIF
                LDMIA    R0!,{R4-R11}           ; Restore R4..R11

SVC_ContextRestoreSP
                MSR      PSP,R0                 ; Set PSP

SVC_Exit
                BX       LR                     ; Exit from handler

SVC_User
                LDR      R2,=osRtxUserSVC       ; Load address of SVC table
                LDR      R3,[R2]                ; Load SVC maximum number
                CMP      R1,R3                  ; Check SVC number range
                BHI      SVC_Exit               ; Branch if out of range

                PUSH     {R0,LR}                ; Save SP and EXC_RETURN
                LDR      R12,[R2,R1,LSL #2]     ; Load address of SVC function
                LDM      R0,{R0-R3}             ; Load function parameters from stack
                BLX      R12                    ; Call service function
                POP      {R12,LR}               ; Restore SP and EXC_RETURN
                STR      R0,[R12]               ; Store function return value

                BX       LR                     ; Return from handler

                ALIGN
                ENDP


PendSV_Handler  PROC
                EXPORT   PendSV_Handler
                IMPORT   osRtxPendSV_Handler

                PUSH     {R0,LR}                ; Save EXC_RETURN
                BL       osRtxPendSV_Handler    ; Call osRtxPendSV_Handler
                POP      {R0,LR}                ; Restore EXC_RETURN
                MRS      R12,PSP                ; Save PSP to R12
                B        SVC_Context            ; Branch to context handling

                ALIGN
                ENDP


SysTick_Handler PROC
                EXPORT   SysTick_Handler
                IMPORT   osRtxTick_Handler

                PUSH     {R0,LR}                ; Save EXC_RETURN
                BL       osRtxTick_Handler      ; Call osRtxTick_Handler
                POP      {R0,LR}                ; Restore EXC_RETURN
                MRS      R12,PSP                ; Save PSP to R12
                B        SVC_Context            ; Branch to context handling

                ALIGN
                ENDP


                END
