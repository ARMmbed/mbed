/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/*
 *                 SSP interface Support
 *                 =====================
 */

#ifndef MBED_SPI_DEF_H
#define MBED_SPI_DEF_H

#include <stdint.h>                      /* standard types definitions                      */

#define Module_ID 0x00090108

typedef struct beetle_spi {
    volatile unsigned int CONFIG;            /* 0x00 RW Configuration Register */
    volatile unsigned int CLK_CFG;           /* 0x04 */
    volatile unsigned int MODE_CONFIG;       /* 0x08 RW SPI FIFO control Register */
    volatile unsigned int CS_REG;            /* 0x0C RW Slave Selection Register */
    volatile unsigned int IRQ_ENABLE;        /* 0x10 RO Interrupt Enable Register*/
    volatile unsigned int SPI_STATUS;        /* 0x14 RW SPI Status Register */
    volatile unsigned int SPI_TX_DATA;       /* 0x18 WO Transmit Data Register */
    volatile unsigned int SPI_RX_DATA;       /* 0x1C RO Receive Data Register */
    volatile unsigned int IRQ_DISABLE;       /* 0x WO Interrupt Disable Register */
    volatile unsigned int SPI_ENABLE;        /* 0x14 RW SPI Enable Register */
    volatile unsigned int TX_THRESHOLD;      /* 0x28 RW TX Threshold Register */
    volatile unsigned int RX_THRESHOLD;      /* 0x2C RW RX Threshold Register */
} SPI_TypeDef;

typedef struct _SSP_SFR {
    uint32_t volatile S5JS100_SSP_CR0_reg;  /* 0x0000 Control Register 0 */
    uint32_t volatile S5JS100_SSP_CR1_reg;  /* 0x0004 Control Register 1 */
    uint32_t volatile S5JS100_SSP_DR_reg;   /* 0x0008 Data Register */
    uint32_t volatile S5JS100_SSP_SR_reg;   /* 0x000c Status Register */
    uint32_t volatile S5JS100_SSP_CPSR_reg; /* 0x0010 Clock Prescale Register */
    uint32_t volatile S5JS100_SSP_IMSC_reg; /* 0x0014 Interrupt Mask Set and Clear Register */
    uint32_t volatile S5JS100_SSP_RIS_reg;  /* 0x0018 Raw Interrupt Status Register */
    uint32_t volatile S5JS100_SSP_MIS_reg;  /* 0x001c Masked Interrupt Status Register */
    uint32_t volatile S5JS100_SSP_ICR_reg;  /* 0x0020 Interrupt Clear Register */
    uint32_t volatile S5JS100_SSP_DMACR_reg;/* 0x0024 DMA Control Register */
} SSP_Typedef;

#define SPI0_BASE   (0x83016000)       /* Shield Header SPI Base Address */
#define SPI1_BASE   (0x83015000)       /* ADC SPI Base Address */

#define SHIELD_SPI        ((SPI_TypeDef      *) SPI0_BASE )
#define ADC_SPI           ((SPI_TypeDef      *) SPI1_BASE )

/* Configuration Register Bit Masks */
#define CONFIG_MSEL     0x00001     // Bit [00] MSEL Mode Select
#define CONFIG_CPOL     0x00002     // Bit [01] CPOL External Clock Edge
#define CONFIG_CPHA     0x00004     // Bit [02] CPHA Clock Phase
#define CONFIG_MBRD     0x00038     // Bits [05:03] MBRD Master Baud Rate Divisor (2 to 256)
#define CONFIG_MBRD_0   0x00008
#define CONFIG_MBRD_1   0x00010
#define CONFIG_MBRD_2   0x00020
#define CONFIG_MBRD_SHIFT 3
#define CONFIG_TWS      0x000C0     // Bits [07:06] TWS Transfer Word Size
#define CONFIG_TWS_0    0x00000
#define CONFIG_TWS_1    0x00040
#define CONFIG_MRCS     0x00100     // Bit [08] MRCS Reference Clock Select
#define CONFIG_PSD      0x00200     // Bit [09] PSD Peripheral Select Decode
#define CONFIG_PCSL     0x03C00     // Bits [13:10] PCSL Peripheral Chip Select Lines (master mode only)
#define CONFIG_MCSE     0x04000     // Bit [14] MCSE Manual Chip Select Enable
#define CONFIG_MSE      0x08000     // Bit [15] MSE Manual Start Enable
#define CONFIG_MSC      0x10000     // Bit [16] MSC Manual Start Command
#define CONFIG_MFGE     0x20000     // Bit [17] MFGE Mode Fail Generation Enable
#define CONFIG_SPSE     0x40000     // Bit [18] SPSE Sample Point Shift Enable

/* Interrupt Status Register Bit Masks */
#define IRQ_STATUS_ROF  0x01        // Bit [00] ROF RX FIFO Overflow
#define IRQ_STATUS_MF   0x02        // Bit [01] MF Mode Fail
#define IRQ_STATUS_TNF  0x04        // Bit [02] TNF TX FIFO Not Full (current FIFO status)
#define IRQ_STATUS_TF   0x08        // Bit [03] TF TX FIFO Full (current FIFO status)
#define IRQ_STATUS_RNE  0x10        // Bit [04] RNE RX FIFO Not Empty (current FIFO status)
#define IRQ_STATUS_RF   0x20        // Bit [05] RF RX FIFO Full (current FIFO status)
#define IRQ_STATUS_TUF  0x40        // Bit [06] TUF TX FIFO Underflow

/* Interrupt Enable Register Bit Masks */
#define IRQ_ENABLE_ROFE  0x01       // Bit [00] ROFE RX FIFO Overflow Enable
#define IRQ_ENABLE_MFE   0x02       // Bit [01] MFE Mode Fail Enable
#define IRQ_ENABLE_TNFE  0x04       // Bit [02] TNFE TX FIFO Not Full Enable
#define IRQ_ENABLE_TFE   0x08       // Bit [03] TFE TX FIFO Full Enable
#define IRQ_ENABLE_RNEE  0x10       // Bit [04] RNEE RX FIFO Not Empty Enable
#define IRQ_ENABLE_RFE   0x20       // Bit [05] RFE RX FIFO Full Enable
#define IRQ_ENABLE_TUFE  0x40       // Bit [06] TUFE TX FIFO Underflow Enable

/* Interrupt Disable Register Bit Masks */
#define IRQ_DISABLE_ROFD  0x01      // Bit [00] ROFD RX FIFO Overflow Disable
#define IRQ_DISABLE_MFD   0x02      // Bit [01] MFD Mode Fail Disable
#define IRQ_DISABLE_TNFD  0x04      // Bit [02] TNFD TX FIFO Not Full Disable
#define IRQ_DISABLE_TFD   0x08      // Bit [03] TFD TX FIFO Full Disable
#define IRQ_DISABLE_RNED  0x10      // Bit [04] RNED RX FIFO Not Empty Disable
#define IRQ_DISABLE_RFD   0x20      // Bit [05] RFD RX FIFO Full Disable
#define IRQ_DISABLE_TUFD  0x40      // Bit [06] TUFD TX FIFO Underflow Disable

/* Interrupt Mask Register Bit Masks */
#define IRQ_MASK_ROFM  0x01         // Bit [00] ROFM RX FIFO Overflow Mask
#define IRQ_MASK_MFM   0x02         // Bit [01] MFM Mode Fail Mask
#define IRQ_MASK_TNFM  0x04         // Bit [02] TNFM TX FIFO Not Full Mask
#define IRQ_MASK_TFM   0x08         // Bit [03] TFM TX FIFO Full Mask
#define IRQ_MASK_RNEM  0x10         // Bit [04] RNEM RX FIFO Not Empty Mask
#define IRQ_MASK_RFM   0x20         // Bit [05] RFM RX FIFO Full Mask
#define IRQ_MASK_TUFM  0x40         // Bit [06] TUFM TX FIFO Underflow Mask

/* SPI Enable Register Bit Masks */
#define SPI_ENABLE_SPIE 0x01        // Bit [00] SPIE SPI Enable

/* Delay Register Bit Masks */
#define DELAY_D_INIT    0x000000FF  // Bits [07:00] D_INIT Delay Init
#define DELAY_D_AFTER   0x0000FF00  // Bits [15:08] D_AFTER Delay After
#define DELAY_D_BTWN    0x00FF0000  // Bits [23:16] D_BTWN Delay Between
#define DELAY_D_NSS     0xFF000000  // Bits [31:24] D_NSS Delay NSS

/* Transmit Data Register Bit Masks */
#define TX_DATA_TDATA   0xFF

/* Receive Data Register Bit Masks */
#define RX_DATA_RDATA   0xFF

/* Slave Idle Count Register Bit Masks */
#define SLAVE_IDLE_COUNT_SICNT 0xFF // Bits [07:00] SICNT Slave Idle Count

/* TX Threshold Register Bit Masks */
#define TX_THRESHOLD_TTRSH  0x07    // Bits [N:00] TTRSH TX Threshold

/* RX Threshold Register Bit Masks */
#define RX_THRESHOLD_RTRSH  0x07    // Bits [N:00] RTRSH RX Threshold

#define CH_CFG_TX_CH_ON         (1 << 0)
#define CH_CFG_TX_CH_OFF        (0 << 0)
#define CH_CFG_RX_CH_ON         (1 << 1)
#define CH_CFG_RX_CH_OFF        (0 << 1)
#define CH_CFG_MODE_MASK        (3 << 2)
#define CH_CFG_MODE(x)          ((x & 3) << 2)
#define CH_CFG_SLAVE            (1 << 4)
#define CH_CFG_MASTER           (0 << 4)
#define CH_CFG_FIFO_FLUSH       (1 << 5)
#define CH_CFG_FIFO_FLUSH_OFF   (0 << 5)
#define CH_CFG_HIGH_SPEED_EN    (1 << 6)
#define CH_CFG_HIGH_SPEED_DIS   (0 << 6)

#define MODE_CFG_DMA_SINGLE     (0 << 0)
#define MODE_CFG_DMA_4BURST     (1 << 0)
#define MODE_CFG_DMA_TX_ON      (1 << 1)
#define MODE_CFG_DMA_TX_OFF     (0 << 1)
#define MODE_CFG_DMA_RX_ON      (1 << 2)
#define MODE_CFG_DMA_RX_OFF     (0 << 2)
#define MODE_CFG_TX_RDY_LVL(x)  ((x & 0x3F) << 5)
#define MODE_CFG_RX_RDY_LVL(x)  ((x & 0x3F) << 11)
#define MODE_CFG_BUS_WDT_MASK   (3 << 17)
#define MODE_CFG_BUS_WIDTH_8    (0 << 17)
#define MODE_CFG_BUS_WIDTH_16   (1 << 17)
#define MODE_CFG_BUS_WIDTH_32   (2 << 17)
#define GET_MODE_CFG_BUSW(x)    (1 << ((x & MODE_CFG_BUS_WDT_MASK) >> 17))
#define MODE_CFG_TRLNG_CNT(x)   ((x & 0x3FF) << 19)
#define MODE_CFG_CH_WDT_MASK    (3 << 29)
#define MODE_CFG_CH_WIDTH_8     (0 << 29)
#define MODE_CFG_CH_WIDTH_16    (1 << 29)
#define MODE_CFG_CH_WIDTH_32    (2 << 29)

#define CS_REG_nSS_ACTIVE       (0 << 0)
#define CS_REG_nSS_INACTIVE     (1 << 0)
#define CS_REG_nSS_AUTO         (1 << 1)
#define CS_REG_nSS_MANUAL       (0 << 1)
#define CS_REG_nSS_TIME_CNT(x)  ((x & 0x3F) << 1)

#define INT_MASK_TRAILING       (1 << 6)
#define INT_MASK_RX_OVERRUN     (1 << 5)
#define INT_MASK_RX_UNDERRUN    (1 << 4)
#define INT_MASK_TX_OVERRUN     (1 << 3)
#define INT_MASK_TX_UNDERRUN    (1 << 2)
#define INT_MASK_RX_FIFO_RDY    (1 << 1)
#define INT_MASK_TX_FIFO_RDY    (1 << 0)

#define SPI_STAT_TX_FIFO_RDY(x)     ((x >> 0) & 1)
#define SPI_STAT_RX_FIFO_RDY(x)     ((x >> 1) & 1)
#define SPI_STAT_TX_UNDERRUN(x)     ((x >> 2) & 1)
#define SPI_STAT_TX_OVERRUN(x)      ((x >> 3) & 1)
#define SPI_STAT_RX_UNDERRUN(x)     ((x >> 4) & 1)
#define SPI_STAT_RX_OVERRUN(x)      ((x >> 5) & 1)
#define SPI_STAT_TX_FIFO_LVL(x)     ((x >> 6) & 0x1FF)
#define SPI_STAT_RX_FIFO_LVL(x)     ((x >> 15) & 0x1FF)
#define SPI_STAT_TRAILING_BYTE(x)   ((x >> 24) & 1)
#define SPI_STAT_TX_DONE(x)         ((x >> 25) & 1)

enum spi_mode_e {
    SPIDEV_MODE0 = 0,           /**< CPOL=0 CHPHA=0 */
    SPIDEV_MODE1,               /**< CPOL=0 CHPHA=1 */
    SPIDEV_MODE2,               /**< CPOL=1 CHPHA=0 */
    SPIDEV_MODE3                /**< CPOL=1 CHPHA=1 */
};


/* SSP *******************************************************************************************/

#define S5JS100_SSP_FIFOSZ         (8)
#define SSP_CLOCK   (200000000) /* SSP input frequency */

/* Register offsets *****************************************************************/

#define S5JS100_SSP_CR0_OFFSET     (0x0000) /* Control Register 0 */
#define S5JS100_SSP_CR1_OFFSET     (0x0004) /* Control Register 1 */
#define S5JS100_SSP_DR_OFFSET      (0x0008) /* Data Register */
#define S5JS100_SSP_SR_OFFSET      (0x000c) /* Status Register */
#define S5JS100_SSP_CPSR_OFFSET    (0x0010) /* Clock Prescale Register */
#define S5JS100_SSP_IMSC_OFFSET    (0x0014) /* Interrupt Mask Set and Clear Register */
#define S5JS100_SSP_RIS_OFFSET     (0x0018) /* Raw Interrupt Status Register */
#define S5JS100_SSP_MIS_OFFSET     (0x001c) /* Masked Interrupt Status Register */
#define S5JS100_SSP_ICR_OFFSET     (0x0020) /* Interrupt Clear Register */
#define S5JS100_SSP_DMACR_OFFSET   (0x0024) /* DMA Control Register */

/* Register bit definitions *********************************************************/
/* Control Register 0 */

#define SSP_CR0_DSS_SHIFT     (0)   /* Bits 0-3: DSS Data Size Select */
#define SSP_CR0_DSS_MASK      (15 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_4BIT      (3 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_5BIT      (4 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_6BIT      (5 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_7BIT      (6 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_8BIT      (7 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_9BIT      (8 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_10BIT     (9 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_11BIT     (10 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_12BIT     (11 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_13BIT     (12 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_14BIT     (13 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_15BIT     (14 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_DSS_16BIT     (15 << SSP_CR0_DSS_SHIFT)
#define SSP_CR0_FRF_SHIFT     (4)   /* Bits 4-5: FRF Frame Format */
#define SSP_CR0_FRF_MASK      (3 << SSP_CR0_FRF_SHIFT)
#define SSP_CR0_FRF_SPI       (0 << SSP_CR0_FRF_SHIFT)
#define SSP_CR0_FRF_TI        (1 << SSP_CR0_FRF_SHIFT)
#define SSP_CR0_FRF_UWIRE     (2 << SSP_CR0_FRF_SHIFT)
#define SSP_CR0_CPOL          (1 << 6)  /* Bit 6:  Clock Out Polarity */
#define SSP_CR0_CPHA          (1 << 7)  /* Bit 7:  Clock Out Phase */
#define SSP_CR0_SCR_SHIFT     (8)   /* Bits 8-15: Serial Clock Rate */
#define SSP_CR0_SCR_MASK      (0xff << SSP_CR0_SCR_SHIFT)
/* Bits 8-31: Reserved */

#define SSP_CR1_LBM (1 << 0)    /* Bit 0:  Loop Back Mode */
#define SSP_CR1_SSE (1 << 1)    /* Bit 1:  SSP Enable */
#define SSP_CR1_MS  (1 << 2)    /* Bit 2:  Master/Slave Mode */
#define SSP_CR1_SOD (1 << 3)    /* Bit 3:  Slave Output Disable */
#define SSP_CR1_CSE (1 << 4)    /* Bit 4:  User CS enable */
#define SSP_CR1_CS  (1 << 5)    /* Bit 5:  User CS control */
/* Bits 6-31: Reserved */
/* Data Register */

#define SSP_DR_MASK (0xffff)    /* Bits 0-15: Data */
/* Bits 16-31: Reserved */
/* Status Register */

#define SSP_SR_TFE  (1 << 0)    /* Bit 0:  Transmit FIFO Empty */
#define SSP_SR_TNF  (1 << 1)    /* Bit 1:  Transmit FIFO Not Full */
#define SSP_SR_RNE  (1 << 2)    /* Bit 2:  Receive FIFO Not Empty */
#define SSP_SR_RFF  (1 << 3)    /* Bit 3:  Receive FIFO Full */
#define SSP_SR_BSY  (1 << 4)    /* Bit 4:  Busy */

#define SSP_SR_TFE_CHECK(x) ((x) & SSP_SR_TFE)  /* Bit 0:  Transmit FIFO Empty */
#define SSP_SR_TNF_CHECK(x) ((x) & SSP_SR_TNF)  /* Bit 1:  Transmit FIFO Not Full */
#define SSP_SR_RNE_CHECK(x) ((x) & SSP_SR_RNE)  /* Bit 2:  Receive FIFO Not Empty */
#define SSP_SR_RFF_CHECK(x) ((x) & SSP_SR_RFF)  /* Bit 3:  Receive FIFO Full */
#define SSP_SR_BSY_CHECK(x) ((x) & SSP_SR_BSY)  /* Bit 4:  Busy */
/* Bits 5-31: Reserved */
/* Clock Prescale Register */

#define SSP_MAX_FREQUENCE           (SSP_CLOCK / 26)
#define SPI_MAX_FREQUENCY           (SSP_CLOCK / 6)
#define SSP_MIN_FREQUENCE           (SSP_CLOCK / (254 * 256))

#endif
