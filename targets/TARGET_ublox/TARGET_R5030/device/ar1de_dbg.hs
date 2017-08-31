; 
; Copyright (C) u-blox
; All rights reserved.
; This source file is the sole property of u-blox. Reproduction or utilization
; of this source in whole or part is forbidden without the written consent of
; u-blox.
; 
; FILE: ar1de_dbg.hs

AR1DE_NR_BP EQU 8
AR1DE_DBG_DEUID EQU 0x000
AR1DE_DBG_INSTID EQU 0x004
AR1DE_DBG_DBGCONF EQU 0x008
AR1DE_DBG_DBGCTRL EQU 0x080
AR1DE_DBG_ODECTRLSTAT EQU 0x084
AR1DE_DBG_ODEPC EQU 0x088
AR1DE_DBG_DBGBKSTATE EQU 0x08C
AR1DE_DBG_BKPSTAT EQU 0x090
AR1DE_DBG_BKPTENSET EQU 0x094
AR1DE_DBG_BKPTENCLR EQU 0x098
AR1DE_DBG_BREAK_ON_BREAKPOINT_DISABLE_SET EQU 0x0A0
AR1DE_DBG_BREAK_ON_BREAKPOINT_DISABLE_CLR EQU 0x0A4
AR1DE_DBG_BRKPTPC EQU 0x100
AR1DE_DBG_BRKPTPC_FIELDS EQU AR1DE_NR_BP
AR1DE_DBG_EXCPTRAW EQU 0x184
AR1DE_DBG_EXCPTSTAT EQU 0x188
AR1DE_DBG_EXCPTCLR EQU 0x18C
AR1DE_DBG_EXCPTENS EQU 0x190
AR1DE_DBG_EXCPTENC EQU 0x194
AR1DE_DBG_OBSADDR0 EQU 0x200
AR1DE_DBG_OBSDATA0 EQU 0x204
AR1DE_DBG_OBSCMD0 EQU 0x208
AR1DE_DBG_OBSSTAT0 EQU 0x20C
AR1DE_DBG_OBSCONF0 EQU 0x210
AR1DE_DBG_INTMCTRL EQU 0xF00
AR1DE_DBG_CLAIMSET EQU 0xFA0
AR1DE_DBG_CLAIMCLR EQU 0xFA4
AR1DE_DBG_LOCKACCESS EQU 0xFB0
AR1DE_DBG_LOCKSTATUS EQU 0xFB4
AR1DE_DBG_AUTHSTATUS EQU 0xFB8
AR1DE_DBG_DEVCONF EQU 0xFC8
AR1DE_DBG_DEVTYPE EQU 0xFCC
AR1DE_DBG_PERIPHERALID4 EQU 0xFD0
AR1DE_DBG_PERIPHERALID5 EQU 0xFD4
AR1DE_DBG_PERIPHERALID6 EQU 0xFD8
AR1DE_DBG_PERIPHERALID7 EQU 0xFDC
AR1DE_DBG_PERIPHERALID0 EQU 0xFE0
AR1DE_DBG_PERIPHERALID1 EQU 0xFE4
AR1DE_DBG_PERIPHERALID2 EQU 0xFE8
AR1DE_DBG_PERIPHERALID3 EQU 0xFEC
AR1DE_DBG_COMPONENTID0 EQU 0xFF0
AR1DE_DBG_COMPONENTID1 EQU 0xFF4
AR1DE_DBG_COMPONENTID2 EQU 0xFF8
AR1DE_DBG_COMPONENTID3 EQU 0xFFC
; EOF: ar1de_dbg.hs
   END
