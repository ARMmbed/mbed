/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "can_api.h"

#if DEVICE_CAN

#include "cmsis.h"

#include "pinmap.h"
#include "error.h"

#include <math.h>
#include <string.h>

/* Acceptance filter mode in AFMR register */
#define ACCF_OFF                0x01
#define ACCF_BYPASS             0x02
#define ACCF_ON                 0x00
#define ACCF_FULLCAN            0x04

/* There are several bit timing calculators on the internet.
http://www.port.de/engl/canprod/sv_req_form.html
http://www.kvaser.com/can/index.htm
*/

static const PinMap PinMap_CAN_RD[] = {
    {P0_0 , CAN_1, 1},
    {P0_4 , CAN_2, 2},
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
    {P0_21, CAN_1, 3},
#elif defined(TARGET_LPC4088)
    {P0_21, CAN_1, 4},
#endif
    {P2_7 , CAN_2, 1},
    {NC   , NC   , 0}
};

static const PinMap PinMap_CAN_TD[] = {
    {P0_1 , CAN_1, 1},
    {P0_5 , CAN_2, 2},
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
    {P0_22, CAN_1, 3},
#elif defined(TARGET_LPC4088)
    {P0_22, CAN_1, 4},
#endif
    {P2_8 , CAN_2, 1},
    {NC   , NC   , 0}
};

// Type definition to hold a CAN message
struct CANMsg {
    unsigned int  reserved1 : 16;
    unsigned int  dlc       :  4; // Bits 16..19: DLC - Data Length Counter
    unsigned int  reserved0 : 10;
    unsigned int  rtr       :  1; // Bit 30: Set if this is a RTR message
    unsigned int  type      :  1; // Bit 31: Set if this is a 29-bit ID message
    unsigned int  id;             // CAN Message ID (11-bit or 29-bit)
    unsigned char data[8];        // CAN Message Data Bytes 0-7
};
typedef struct CANMsg CANMsg;

static uint32_t can_disable(can_t *obj) {
    uint32_t sm = obj->dev->MOD;
    obj->dev->MOD |= 1;
    return sm;
}

static inline void can_enable(can_t *obj) {
    if (obj->dev->MOD & 1) {
        obj->dev->MOD &= ~(1);
    }
}

#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
static int can_pclk(can_t *obj) {
    int value = 0;
    switch ((int)obj->dev) {
        case CAN_1: value = (LPC_SC->PCLKSEL0 & (0x3 << 26)) >> 26; break;
        case CAN_2: value = (LPC_SC->PCLKSEL0 & (0x3 << 28)) >> 28; break;
    }

    switch (value) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 6;
        default: return 4;
    }
}
#endif

// This table has the sampling points as close to 75% as possible. The first
// value is TSEG1, the second TSEG2.
static const int timing_pts[23][2] = {
    {0x0, 0x0},      // 2,  50%
    {0x1, 0x0},      // 3,  67%
    {0x2, 0x0},      // 4,  75%
    {0x3, 0x0},      // 5,  80%
    {0x3, 0x1},      // 6,  67%
    {0x4, 0x1},      // 7,  71%
    {0x5, 0x1},      // 8,  75%
    {0x6, 0x1},      // 9,  78%
    {0x6, 0x2},      // 10, 70%
    {0x7, 0x2},      // 11, 73%
    {0x8, 0x2},      // 12, 75%
    {0x9, 0x2},      // 13, 77%
    {0x9, 0x3},      // 14, 71%
    {0xA, 0x3},      // 15, 73%
    {0xB, 0x3},      // 16, 75%
    {0xC, 0x3},      // 17, 76%
    {0xD, 0x3},      // 18, 78%
    {0xD, 0x4},      // 19, 74%
    {0xE, 0x4},      // 20, 75%
    {0xF, 0x4},      // 21, 76%
    {0xF, 0x5},      // 22, 73%
    {0xF, 0x6},      // 23, 70%
    {0xF, 0x7},      // 24, 67%
};

static unsigned int can_speed(unsigned int sclk, unsigned int pclk, unsigned int cclk, unsigned char psjw) {
    uint32_t    btr;
    uint16_t    brp = 0;
    uint32_t    calcbit;
    uint32_t    bitwidth;
    int         hit = 0;
    int         bits;

    bitwidth = sclk / (pclk * cclk);

    brp = bitwidth / 0x18;
    while ((!hit) && (brp < bitwidth / 4)) {
        brp++;
        for (bits = 22; bits > 0; bits--) {
            calcbit = (bits + 3) * (brp + 1);
            if (calcbit == bitwidth) {
                hit = 1;
                break;
            }
        }
    }

    if (hit) {
        btr = ((timing_pts[bits][1] << 20) & 0x00700000)
            | ((timing_pts[bits][0] << 16) & 0x000F0000)
            | ((psjw                << 14) & 0x0000C000)
            | ((brp                 <<  0) & 0x000003FF);
    } else {
        btr = 0xFFFFFFFF;
    }

    return btr;

}

void can_init(can_t *obj, PinName rd, PinName td) {
    CANName can_rd = (CANName)pinmap_peripheral(rd, PinMap_CAN_RD);
    CANName can_td = (CANName)pinmap_peripheral(td, PinMap_CAN_TD);
    obj->dev = (LPC_CAN_TypeDef *)pinmap_merge(can_rd, can_td);
    if ((int)obj->dev == NC) {
        error("CAN pin mapping failed");
    }

    switch ((int)obj->dev) {
        case CAN_1: LPC_SC->PCONP |= 1 << 13; break;
        case CAN_2: LPC_SC->PCONP |= 1 << 14; break;
    }

    pinmap_pinout(rd, PinMap_CAN_RD);
    pinmap_pinout(td, PinMap_CAN_TD);

    can_reset(obj);
    obj->dev->IER = 0;             // Disable Interrupts
    can_frequency(obj, 100000);

    LPC_CANAF->AFMR = ACCF_BYPASS; // Bypass Filter
}

void can_free(can_t *obj) {
    switch ((int)obj->dev) {
        case CAN_1: LPC_SC->PCONP &= ~(1 << 13); break;
        case CAN_2: LPC_SC->PCONP &= ~(1 << 14); break;
    }
}

int can_frequency(can_t *obj, int f) {
#if defined(TARGET_LPC1768) || defined(TARGET_LPC2368)
    int pclk = can_pclk(obj);
#elif defined(TARGET_LPC4088)
    int pclk = PeripheralClock;
#endif
    int btr = can_speed(SystemCoreClock, pclk, (unsigned int)f, 1);

    if (btr > 0) {
        uint32_t modmask = can_disable(obj);
        obj->dev->BTR = btr;
        obj->dev->MOD = modmask;
        return 1;
    } else {
        return 0;
    }
}

int can_write(can_t *obj, CAN_Message msg, int cc) {
    unsigned int CANStatus;
    CANMsg m;

    can_enable(obj);

    m.id   = msg.id ;
    m.dlc  = msg.len & 0xF;
    m.rtr  = msg.type;
    m.type = msg.format;
    memcpy(m.data, msg.data, msg.len);
    const unsigned int *buf = (const unsigned int *)&m;

    CANStatus = obj->dev->SR;
    if (CANStatus & 0x00000004) {
        obj->dev->TFI1 = buf[0] & 0xC00F0000;
        obj->dev->TID1 = buf[1];
        obj->dev->TDA1 = buf[2];
        obj->dev->TDB1 = buf[3];
        if(cc) {
            obj->dev->CMR = 0x30;
        } else {
            obj->dev->CMR = 0x21;
        }
        return 1;

    } else if (CANStatus & 0x00000400) {
        obj->dev->TFI2 = buf[0] & 0xC00F0000;
        obj->dev->TID2 = buf[1];
        obj->dev->TDA2 = buf[2];
        obj->dev->TDB2 = buf[3];
        if (cc) {
            obj->dev->CMR = 0x50;
        } else {
            obj->dev->CMR = 0x41;
        }
        return 1;

    } else if (CANStatus & 0x00040000) {
        obj->dev->TFI3 = buf[0] & 0xC00F0000;
        obj->dev->TID3 = buf[1];
        obj->dev->TDA3 = buf[2];
        obj->dev->TDB3 = buf[3];
        if (cc) {
            obj->dev->CMR = 0x90;
        } else {
            obj->dev->CMR = 0x81;
        }
        return 1;
    }

    return 0;
}

int can_read(can_t *obj, CAN_Message *msg) {
    CANMsg x;
    unsigned int *i = (unsigned int *)&x;

    can_enable(obj);

    if (obj->dev->GSR & 0x1) {
        *i++ = obj->dev->RFS;  // Frame
        *i++ = obj->dev->RID;  // ID
        *i++ = obj->dev->RDA;  // Data A
        *i++ = obj->dev->RDB;  // Data B
        obj->dev->CMR = 0x04;  // release receive buffer

        msg->id     = x.id;
        msg->len    = x.dlc;
        msg->format = (x.type)? CANExtended : CANStandard;
        msg->type   = (x.rtr)?  CANRemote:    CANData;
        memcpy(msg->data,x.data,x.dlc);
        return 1;
    }

    return 0;
}

void can_reset(can_t *obj) {
    can_disable(obj);
    obj->dev->GSR = 0; // Reset error counter when CAN1MOD is in reset
}

unsigned char can_rderror(can_t *obj) {
    return (obj->dev->GSR >> 16) & 0xFF;
}

unsigned char can_tderror(can_t *obj) {
    return (obj->dev->GSR >> 24) & 0xFF;
}

void can_monitor(can_t *obj, int silent) {
    uint32_t mod_mask = can_disable(obj);
    if (silent) {
        obj->dev->MOD |= (1 << 1);
    } else {
        obj->dev->MOD &= ~(1 << 1);
    }
    if (!(mod_mask & 1)) {
        can_enable(obj);
    }
}

#endif

