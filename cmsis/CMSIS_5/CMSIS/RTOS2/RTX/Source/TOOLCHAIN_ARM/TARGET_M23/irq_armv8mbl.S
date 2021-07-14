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
; * Title:       ARMv8-M Baseline Exception handlers
; *
; * -----------------------------------------------------------------------------
; */


                IF       :LNOT::DEF:RTX_STACK_CHECK
RTX_STACK_CHECK EQU      0
                ENDIF

                IF       :LNOT::DEF:DOMAIN_NS
DOMAIN_NS       EQU      0
                ENDIF

I_T_RUN_OFS     EQU      20                     ; osRtxInfo.thread.run offset
TCB_SM_OFS      EQU      48                     ; TCB.stack_mem offset
TCB_SP_OFS      EQU      56                     ; TCB.SP offset
TCB_SF_OFS      EQU      34                     ; TCB.stack_frame offset
TCB_TZM_OFS     EQU      64                     ; TCB.tz_memory offset

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

                MOV      R0,LR
                LSRS     R0,R0,#3               ; Determine return stack from EXC_RETURN bit 2
                BCC      SVC_MSP                ; Branch if return stack is MSP
                MRS      R0,PSP                 ; Get PSP

SVC_Number
                LDR      R1,[R0,#24]            ; Load saved PC from stack
                SUBS     R1,R1,#2               ; Point to SVC instruction
                LDRB     R1,[R1]                ; Load SVC number
                CMP      R1,#0                  ; Check SVC number
                BNE      SVC_User               ; Branch if not SVC 0

                PUSH     {R0,LR}                ; Save SP and EXC_RETURN
                LDMIA    R0,{R0-R3}             ; Load function parameters from stack
                BLX      R7                     ; Call service function
                POP      {R2,R3}                ; Restore SP and EXC_RETURN
                STMIA    R2!,{R0-R1}            ; Store function return values
                MOV      LR,R3                  ; Set EXC_RETURN

SVC_Context
                LDR      R3,=osRtxInfo+I_T_RUN_OFS; Load address of osRtxInfo.thread.run
                LDMIA    R3!,{R1,R2}            ; Load osRtxInfo.thread.run: curr & next
                CMP      R1,R2                  ; Check if thread switch is required
                BEQ      SVC_Exit               ; Branch when threads are the same

                SUBS     R3,R3,#8               ; Adjust address
                STR      R2,[R3]                ; osRtxInfo.thread.run: curr = next
                CBZ      R1,SVC_ContextRestore  ; Branch if running thread is deleted

SVC_ContextSave
            IF DOMAIN_NS != 0
                LDR      R0,[R1,#TCB_TZM_OFS]   ; Load TrustZone memory identifier
                CBZ      R0,SVC_ContextSave_NS  ; Branch if there is no secure context
                PUSH     {R1,R2,R3,R7}          ; Save registers
                MOV      R7,LR                  ; Get EXC_RETURN
                BL       TZ_StoreContext_S      ; Store secure context
                MOV      LR,R7                  ; Set EXC_RETURN
                POP      {R1,R2,R3,R7}          ; Restore registers
            ENDIF

SVC_ContextSave_NS
                MRS      R0,PSP                 ; Get PSP
            IF DOMAIN_NS != 0
                MOV      R3,LR                  ; Get EXC_RETURN
                LSLS     R3,R3,#25              ; Check domain of interrupted thread
                BMI      SVC_ContextSaveSP      ; Branch if secure
            ENDIF

            IF RTX_STACK_CHECK != 0
                SUBS     R0,R0,#32              ; Calculate SP: space for R4..R11

SVC_ContextSaveSP
                STR      R0,[R1,#TCB_SP_OFS]    ; Store SP
                MOV      R3,LR                  ; Get EXC_RETURN
                MOV      R0,R1                  ; osRtxInfo.thread.run.curr
                ADDS     R0,R0,#TCB_SF_OFS      ; Adjust address
                STRB     R3,[R0]                ; Store stack frame information

                PUSH     {R1,R2}                ; Save osRtxInfo.thread.run: curr & next
                MOV      R0,R1                  ; Parameter: osRtxInfo.thread.run.curr
                BL       osRtxThreadStackCheck  ; Check if thread stack is overrun
                POP      {R1,R2}                ; Restore osRtxInfo.thread.run: curr & next
                CMP      R0,#0
                BNE      SVC_ContextSaveRegs    ; Branch when stack check is ok

                MOVS     R0,#osRtxErrorStackOverflow ; Parameter: r0=code, r1=object_id
                BL       osRtxKernelErrorNotify      ; Call osRtxKernelErrorNotify
                LDR      R3,=osRtxInfo+I_T_RUN_OFS   ; Load address of osRtxInfo.thread.run
                LDR      R2,[R3,#4]             ; Load osRtxInfo.thread.run: next
                STR      R2,[R3]                ; osRtxInfo.thread.run: curr = next
                B        SVC_ContextRestore     ; Branch to context restore handling

SVC_ContextSaveRegs
              IF DOMAIN_NS != 0
                MOV      R0,R1                  ; osRtxInfo.thread.run.curr
                ADDS     R0,R0,#TCB_SF_OFS      ; Adjust address
                LDRB     R3,[R0]                ; Load stack frame information
                LSLS     R3,R3,#25              ; Check domain of interrupted thread
                BMI      SVC_ContextRestore     ; Branch if secure
              ENDIF
                LDR      R0,[R1,#TCB_SP_OFS]    ; Load SP
                STMIA    R0!,{R4-R7}            ; Save R4..R7
                MOV      R4,R8
                MOV      R5,R9
                MOV      R6,R10
                MOV      R7,R11
                STMIA    R0!,{R4-R7}            ; Save R8..R11
            ELSE
                SUBS     R0,R0,#32              ; Calculate SP: space for R4..R11
                STMIA    R0!,{R4-R7}            ; Save R4..R7
                MOV      R4,R8
                MOV      R5,R9
                MOV      R6,R10
                MOV      R7,R11
                STMIA    R0!,{R4-R7}            ; Save R8..R11
                SUBS     R0,R0,#32              ; Adjust address
SVC_ContextSaveSP
                STR      R0,[R1,#TCB_SP_OFS]    ; Store SP
                MOV      R0,LR                  ; Get EXC_RETURN
                ADDS     R1,R1,#TCB_SF_OFS      ; Adjust address
                STRB     R0,[R1]                ; Store stack frame information
            ENDIF

SVC_ContextRestore
            IF DOMAIN_NS != 0
                LDR      R0,[R2,#TCB_TZM_OFS]   ; Load TrustZone memory identifier
                CBZ      R0,SVC_ContextRestore_NS ; Branch if there is no secure context
                PUSH     {R2,R3}                ; Save registers
                BL       TZ_LoadContext_S       ; Load secure context
                POP      {R2,R3}                ; Restore registers
            ENDIF

SVC_ContextRestore_NS
                LDR      R0,[R2,#TCB_SM_OFS]    ; Load stack memory base
                MSR      PSPLIM,R0              ; Set PSPLIM
                MOV      R0,R2                  ; osRtxInfo.thread.run.next
                ADDS     R0,R0,#TCB_SF_OFS      ; Adjust address
                LDRB     R3,[R0]                ; Load stack frame information
                MOVS     R0,#0xFF
                MVNS     R0,R0                  ; R0=0xFFFFFF00
                ORRS     R3,R3,R0
                MOV      LR,R3                  ; Set EXC_RETURN
                LDR      R0,[R2,#TCB_SP_OFS]    ; Load SP
            IF DOMAIN_NS != 0
                LSLS     R3,R3,#25              ; Check domain of interrupted thread
                BMI      SVC_ContextRestoreSP   ; Branch if secure
            ENDIF

                ADDS     R0,R0,#16              ; Adjust address
                LDMIA    R0!,{R4-R7}            ; Restore R8..R11
                MOV      R8,R4
                MOV      R9,R5
                MOV      R10,R6
                MOV      R11,R7
                SUBS     R0,R0,#32              ; Adjust address
                LDMIA    R0!,{R4-R7}            ; Restore R4..R7
                ADDS     R0,R0,#16              ; Adjust address

SVC_ContextRestoreSP
                MSR      PSP,R0                 ; Set PSP

SVC_Exit
                BX       LR                     ; Exit from handler

SVC_MSP
                MRS      R0,MSP                 ; Get MSP
                B        SVC_Number

SVC_User
                LDR      R2,=osRtxUserSVC       ; Load address of SVC table
                LDR      R3,[R2]                ; Load SVC maximum number
                CMP      R1,R3                  ; Check SVC number range
                BHI      SVC_Exit               ; Branch if out of range

                PUSH     {R0,LR}                ; Save SP and EXC_RETURN
                LSLS     R1,R1,#2
                LDR      R3,[R2,R1]             ; Load address of SVC function
                MOV      R12,R3
                LDMIA    R0,{R0-R3}             ; Load function parameters from stack
                BLX      R12                    ; Call service function
                POP      {R2,R3}                ; Restore SP and EXC_RETURN
                STR      R0,[R2]                ; Store function return value
                MOV      LR,R3                  ; Set EXC_RETURN

                BX       LR                     ; Return from handler

                ALIGN
                ENDP


PendSV_Handler  PROC
                EXPORT   PendSV_Handler
                IMPORT   osRtxPendSV_Handler

                PUSH     {R0,LR}                ; Save EXC_RETURN
                BL       osRtxPendSV_Handler    ; Call osRtxPendSV_Handler
                POP      {R0,R1}                ; Restore EXC_RETURN
                MOV      LR,R1                  ; Set EXC_RETURN
                B        SVC_Context            ; Branch to context handling

                ALIGN
                ENDP


SysTick_Handler PROC
                EXPORT   SysTick_Handler
                IMPORT   osRtxTick_Handler

                PUSH     {R0,LR}                ; Save EXC_RETURN
                BL       osRtxTick_Handler      ; Call osRtxTick_Handler
                POP      {R0,R1}                ; Restore EXC_RETURN
                MOV      LR,R1                  ; Set EXC_RETURN
                B        SVC_Context            ; Branch to context handling

                ALIGN
                ENDP


                END
