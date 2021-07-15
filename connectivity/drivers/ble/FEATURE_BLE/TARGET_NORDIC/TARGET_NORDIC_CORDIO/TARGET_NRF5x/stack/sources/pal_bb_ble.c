/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Baseband driver interface file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

/*
 *
 *  NOTES:
 *
 *    While BB is enabled certain resources are consumed and are off limits during that time.
 *      TIMER0 should not be touched while BB is enabled. It's available apart from that.
 *      PPI Channels 14 & 15 are used while BB is enabled. They are available apart from that.
 *
 *    TIMER0's compare/capture registers are assigned specific uses:
 *      CC[0] - triggers radio TXEN or RXEN task, PPI triggered
 *      CC[1] - RX timeout
 *      CC[2] - captures *every* radio PAYLOAD event time, PPI triggered
 *      CC[3] - manual capture of current time
 *      CC[4] - captures *every* radio ADDRESS event time, PPI triggered
 *
 *    PPI Channel 14:
 *      Used to trigger TXEN or RXEN task on timer event COMPARE[0].
 *
 *    PPI Channel 15:
 *      Used to trigger timer capture to CC[2] on every radio PAYLOAD event.
 *
 *    PPI Channel 16:
 *      Used to trigger timer capture to CC[4] on every radio ADDRESS event.
 *
 *    If  BB_CLK_RATE_HZ == 32768 (low power mode):
 *
 *    Additional two channels of NRF RTC1 are controlled here.
 *      CC[1] - Compare value for NRF TIMER0 start task.
 *      CC[2] - Compare value for NRF HFCLK start task.
 */

#include "pal_types.h"
#include "pal_bb.h"
#include "pal_rtc.h"
#include "pal_led.h"
#include "pal_bb_ble.h"
#include "pal_radio.h"
#include "pal_timer.h"
#include "ll_defs.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

#include <string.h>

#if (LL_ENABLE_TESTER)
#include "ll_tester_api.h"
#endif

#include "mbed_nrf5x_adaptation.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

#define PDU_HEADER_LEN              2
#define MAX_PAYLOAD_LEN             255
#define BB_DATA_PDU_LEN_OFFSET      1
#define BB_TRL_MAX_LEN              LL_DATA_MIC_LEN
#define BB_DATA_HDR_LEN             PDU_HEADER_LEN

#define MAX_TIFS_DEVIATION_USECS    (2 + 1) /* from spec, section 4.2.1 Active Clock Accuracy, 50ppm = 2usec */
                                            /*   +1 usec as a margin of uncertainty */

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
#define NRF5x_tTXEN_BLE_USECS       39      /* usec, Time between TXEN task and READY event, fast ramp mode (datasheet value of 40 is wrong). */
#define NRF5x_tRXEN_BLE_USECS       42      /* usec, Time between RXEN task and READY event, fast ramp mode (datasheet value of 40 is wrong). */
#else
/* from Nordic's Product Specification, section "Radio timing parameters" */
#define NRF5x_tTXEN_BLE_USECS       140     /* usec, Time between TXEN task and READY event  */
#define NRF5x_tRXEN_BLE_USECS       140     /* usec, Time between RXEN task and READY event  */
#endif

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
#define NRF5x_PROP_DELAY_TX_USECS   1       /* usec, (precise value is 0.6 usec) transmit propagation delay, START to first bit at antenna */
#define NRF5x_PROP_DELAY_RX_USECS   9       /* usec, (precise value is 9.4 usec), receive propagation delay, last bit at antenna to END */
#else
/* internal propagation delays obtained from Nordic support */
#define NRF5x_PROP_DELAY_TX_USECS   3       /* usec, transmit propagation delay, START to first bit at antenna */
#define NRF5x_PROP_DELAY_RX_USECS   3       /* usec, (precise value is 3.2 usec), receive propagation delay, last bit at antenna to END */
#endif

/* Nordic chip uses a one-byte prefix plus a three-byte base address to form the address */
#define NORDIC_PREFIX_LEN           1       /* prefix is a Nordic concept */
#define NORDIC_BASE_ADDR_LEN        3       /* base address is a Nordic concept */

/* scratch area for Nordic encryption engine */
#define ENC_CCM_DATA_STRUCT_LEN     33      /* length of CCM data structure used for encryption/decryption */
#define ENC_MAXPACKETSIZE           251     /* length used in MAXPACKETSIZE */
#define ENC_SCRATCH_BUF_LEN         ( 16 + ENC_MAXPACKETSIZE ) /* size of scratch area */
#define ENC_H_FIELD_LEN             1       /* length of "H" field */
#define ENC_LPLUS4_LEN              1       /* length of the "L+4" field */
#define ENC_RFU_LEN                 1       /* length of the "RFU" field */
#define ENC_MIC_LEN                 4       /* length of the MIC field */
#define ENC_PLAIN_BUF_LEN           ( ENC_H_FIELD_LEN + ENC_LPLUS4_LEN + ENC_RFU_LEN + ENC_MAXPACKETSIZE )
#define ENC_ENCRYPTED_BUF_LEN       ( ENC_PLAIN_BUF_LEN  + ENC_MIC_LEN )
#define ENC_RFU_OFFSET              2       /* offset of the "RFU" field */

/* +/- range for TIFS adjustment */
#define TIFS_ADJ_RANGE_USECS        8       /* the +/- maximums for TIFS adjustment */

/* adjust prescaler for the clock rate */
#if   (BB_CLK_RATE_HZ == 1000000)
  #define TIMER_PRESCALER   4
  #define TICKS_PER_USEC    1
#elif (BB_CLK_RATE_HZ == 2000000)
  #define TIMER_PRESCALER   3
  #define TICKS_PER_USEC    2
#elif (BB_CLK_RATE_HZ == 4000000)
  #define TIMER_PRESCALER   2
  #define TICKS_PER_USEC    4
#elif (BB_CLK_RATE_HZ == 8000000)
  #define TIMER_PRESCALER   1
  #define TICKS_PER_USEC    8
#elif (USE_RTC_BB_CLK)
  #define TIMER_PRESCALER   4       /* Use 1MHz for HFCLK */
  #define TICKS_PER_USEC    1
#else
  #error "Unsupported clock rate."
#endif

#if (USE_RTC_BB_CLK)
  #define TICKS_TO_USEC(ticks)      (ticks)  /* Use 1MHz for HFCLK */
#endif

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
/* for nRF52, stall until write buffer empties */
#define WAIT_FOR_WR_BUF_EMPTY(r)    ((void)r)
#else
/* for nRF51, no action required */
#define WAIT_FOR_WR_BUF_EMPTY(r)
#endif

#ifndef BB_ENABLE_INLINE_ENC_TX
#define BB_ENABLE_INLINE_ENC_TX     FALSE
#endif

#ifndef BB_ENABLE_INLINE_DEC_RX
#define BB_ENABLE_INLINE_DEC_RX     FALSE
#endif

/* LED definitions */
#ifndef BB_LED_ENA
#define BB_LED_ENA             0
#endif

#if (BB_LED_ENA == 1)
#define BB_LED_1M_ON()        PalLedOn(0)
#define BB_LED_2M_ON()        PalLedOn(1)
#define BB_LED_CODED_ON()     PalLedOn(0); PalLedOn(1)
#define BB_LED_OFF()          PalLedOff(0); PalLedOff(1);
#else
#define BB_LED_1M_ON()
#define BB_LED_2M_ON()
#define BB_LED_CODED_ON()
#define BB_LED_OFF()
#endif

#if (AUDIO_CAPE == 1)
#define BB_LED_TX_ON()        PalLedOn(6)
#define BB_LED_TX_OFF()       PalLedOff(6)
#define BB_LED_RX_ON()        PalLedOn(7)
#define BB_LED_RX_OFF()       PalLedOff(7)
#else
#define BB_LED_TX_ON()
#define BB_LED_TX_OFF()
#define BB_LED_RX_ON()
#define BB_LED_RX_OFF()
#endif

/*! \brief convert little endian byte buffer to uint16_t. */
#define BYTES_TO_UINT16(n, p)     {n = ((uint16_t)(p)[0] + ((uint16_t)(p)[1] << 8));}

/**************************************************************************************************
  Typedefs
**************************************************************************************************/

/* driver states */
typedef enum
{
  NULL_STATE,
  SLEEP_STATE,
  IDLE_STATE,
  TX_STATE,
  RX_STATE,
  PRBS15_STATE
} bbDriverState_t;

/* TIFS states */
typedef enum
{
  TIFS_NULL,
  TIFS_TX_RAMPUP,
  TIFS_RX_RAMPUP
} bbTifsState_t;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/
static void palBbTxHwRadioConfig(uint8_t * pTxBuf);
static void palBbRxHwRadioConfig(uint8_t * pRxBuf, uint16_t len);
static void palBbRadioHardStop(void);
static void palBbRestoreTrl(void);
static uint8_t *palBbGetTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt);

/* IRQ callback declarations. */
static void BbBleDrvTimerIRQHandler(void);
static void BbBleDrvRadioIRQHandler(void);

extern int8_t palRadioGetTxRfPathComp(void);

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

volatile bbDriverState_t driverState = NULL_STATE;
volatile bbTifsState_t tifsState;
uint8_t bbRadioPcnf1WhiteEn;
PalBbBleTxIsr_t bbTxCallback = NULL;
PalBbBleRxIsr_t bbRxCallback = NULL;
uint32_t bbRxTimeoutUsec;
uint8_t * bbpRxBuf;
uint32_t bbAntennaDueTimeUsec;
uint32_t bbEventStartTime;
uint16_t bbDueOffsetUsec;
int8_t bbTxTifsAdj;
uint8_t bbChanIndex;
uint8_t bbTxPhy;
uint8_t bbRxPhy;
uint8_t bbTxPhyOptions;
uint8_t bbRxPhyOptions;
uint8_t tifsTxPhyOptions;
uint16_t bbTxLen;
bool_t bbEncryptTxFlag;
bool_t bbDecryptRxFlag;
bool_t bbEncryptTxSuppressMic;
uint8_t bbEncryptType;
uint8_t bbEncryptDirection;
uint64_t bbEncryptPacketCounter;
uint64_t bbDecryptPacketCounter;
uint8_t bbEncryptPlainBuf[ENC_PLAIN_BUF_LEN+2];         /* add two for overrun protection */
uint8_t bbEncryptScratchBuf[ENC_SCRATCH_BUF_LEN+2];     /* add two for overrun protection */
uint8_t bbEncryptEncryptedBuf[ENC_ENCRYPTED_BUF_LEN+2]; /* add two for overrun protection */
uint8_t bbEncryptCcmData[ENC_CCM_DATA_STRUCT_LEN];
uint8_t bbTrlSave[BB_TRL_MAX_LEN];
uint8_t *bbTrlSavedPtr = NULL;
uint8_t bbTrlSavedLen;
PalBbBleOpParam_t bbOpParam;

#if BB_ENABLE_ASSERT == TRUE
bool_t keySetFlag = 0;
bool_t ivSetFlag = 0;
bool_t encryptTypeSetFlag = 0;
bool_t directionSetFlag = 0;
bool_t txPacketCountSetFlag = 0;
bool_t rxPacketCountSetFlag = 0;
#endif

#if (LL_ENABLE_TESTER == TRUE)
uint32_t bbRxAccAddr;
uint32_t bbTxAccAddr;
uint32_t bbRxCrcInit;
uint32_t bbTxCrcInit;
uint16_t bbModifyTxHdrMask = 0;
uint16_t bbModifyTxHdrValue = 0;
uint8_t bbTxCrcInitInvalidStep = 0;
uint8_t bbTxAccAddrInvalidStep = 0;
uint8_t bbRxCrcInitInvalidStep = 0;
uint8_t bbRxAccAddrInvalidStep = 0;
uint32_t bbTxCrcInitInvalidAdjMask = 0;
uint32_t bbTxAccAddrInvalidAdjMask = 0;
uint32_t bbRxCrcInitInvalidAdjMask = 0;
uint32_t bbRxAccAddrInvalidAdjMask = 0;
uint64_t bbTxCrcInitInvalidChanMask = 0;
uint64_t bbTxAccAddrInvalidChanMask = 0;
uint64_t bbRxCrcInitInvalidChanMask = 0;
uint64_t bbRxAccAddrInvalidChanMask = 0;
bool_t bbTxAccAddrShiftMask = FALSE;
bool_t bbRxAccAddrShiftMask = FALSE;
bool_t bbTxAccAddrShiftInc = FALSE;
bool_t invalidateAccAddrOnceRx = FALSE;
bool_t invalidateAccAddrOnceTx = FALSE;
#endif

/* enable BB assertions */
#ifdef BB_ENABLE_ASSERT
#define BB_ASSERT(x)       { if (!(x)) {  return; } }
#else
#define BB_ASSERT(x)
#endif

#ifdef BB_REQ_ALIGN
#define BB_ALIGN_ASSERT(pBuf)  { if (!((((uint32_t)(pBuf)) & 0x3) == 0)) {  return; } }
#else
#define BB_ALIGN_ASSERT(pBuf)
#endif

/* enable trace here or with conditional compilation */
#define xBB_TRACE

/////////////////////////////////////////////////////////////////////////////////
/* enable diagnostic here or via project level define */
#define xDIAGNOSTICS

/////////////////////////////////////////////////////////////////////////////////
#ifdef DIAGNOSTICS
typedef enum
{
  DIAG_NULL,
  DIAG_TX,
  DIAG_RX,
  DIAG_TX_TIFS,
  DIAG_RX_TIFS
} diag_op_t;

static void DiagFauxTxCallback(uint8_t status);
static void DiagFauxRxCallback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp);

PalBbBleTxIsr_t diagTxCallback;
PalBbBleRxIsr_t diagRxCallback;

diag_op_t diagOp                   = DIAG_NULL;
diag_op_t diagOpLast               = DIAG_NULL;
uint8_t   diagLastStatus           = 0xFF;

/* transmit diagnostics */
uint16_t diagTxTotalCalls          = 0;
uint16_t diagTxCalls               = 0;
uint16_t diagTxTifsCalls           = 0;

uint16_t diagTxSuccess             = 0;
uint16_t diagTxFail                = 0;

uint16_t diagTxTifsSuccess         = 0;
uint16_t diagTxTifsFail            = 0;

uint16_t diagTxFailBadDueTime      = 0;
uint16_t diagTxFailMissedDueTime   = 0;
uint16_t diagTxBlueMoonWin         = 0;

uint16_t diagTxTifsFailMissed      = 0;
uint16_t diagTxTifsFailNotPossible = 0;
uint16_t diagTxTifsBlueMoonWin     = 0;

/* receive diagnostics */
uint16_t diagRxTotalCalls          = 0;
uint16_t diagRxCalls               = 0;
uint16_t diagRxTifsCalls           = 0;

uint16_t diagRxSuccess             = 0;
uint16_t diagRxFail                = 0;
uint16_t diagRxTimeout             = 0;
uint16_t diagRxCrcFail             = 0;

uint16_t diagRxTifsSuccess         = 0;
uint16_t diagRxTifsFail            = 0;
uint16_t diagRxTifsTimeout         = 0;
uint16_t diagRxTifsCrcFail         = 0;

uint16_t diagRxFailBadDueTime      = 0;
uint16_t diagRxFailMissedDueTime   = 0;
uint16_t diagRxBlueMoonWin         = 0;

uint16_t diagRxTifsFailMissed      = 0;
uint16_t diagRxTifsFailNotPossible = 0;
uint16_t diagRxTifsBlueMoonWin     = 0;

/* misc */
uint16_t diagCancels               = 0;
#endif
/////////////////////////////////////////////////////////////////////////////////

/*************************************************************************************************/
/*!
 *  \brief      Configure GPIO as input or output.
 *
 *  \param      pin           Pin number.
 */
/*************************************************************************************************/
void PalBbGpioCfgOutput(uint32_t pin)
{
  nrf_gpio_cfg_output(pin);
}

/*************************************************************************************************/
/*!
 *  \brief      Set GPIO pin high.
 *
 *  \param      pin           Pin number.
 */
/*************************************************************************************************/
void PalBbGpioSet(uint32_t pin)
{
  nrf_gpio_pin_write(pin, 1);
}

/*************************************************************************************************/
/*!
 *  \brief      Set GPIO pin low.
 *
 *  \param      pin           Pin number.
 */
/*************************************************************************************************/
void PalBbGpioClear(uint32_t pin)
{
  nrf_gpio_pin_write(pin, 0);
}

/*************************************************************************************************/
/*!
 *  \brief      Toggle GPIO.
 *
 *  \param      pin           Pin number.
 *  \param      times         GPIO toggle times.
 */
/*************************************************************************************************/
void PalBbGpioToggle(uint32_t pin, uint32_t times)
{
  for (uint32_t i = 0; i < times; i++)
  {
    nrf_gpio_pin_write(pin, 1);
    nrf_gpio_pin_write(pin, 0);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set radio mode.
 *
 *  \param      phy     PHY.
 *  \param      option  PHY option.
 */
/*************************************************************************************************/
static inline void palBbSetRadioMode(uint8_t phy, uint8_t option)
{
  switch(phy)
  {
    case BB_PHY_BLE_1M:
      NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit;
      break;

#if defined(NRF52832_XXAA) || defined(NRF52840_XXAA)
    case BB_PHY_BLE_2M:
      NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_2Mbit;
      break;
#endif

#if defined(NRF52840_XXAA)
    case BB_PHY_BLE_CODED:
      if (option == BB_PHY_OPTIONS_BLE_S2)
      {
        NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_LR500Kbit;
      }
      else
      {
        NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_LR125Kbit;
      }
    break;
#endif

    default:
      NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit;
      break;
  }

  /* select LED according to client selection not actual PHY used */
  switch (bbRxPhy)
  {
    case BB_PHY_BLE_1M:
      BB_LED_1M_ON();
      break;
    case BB_PHY_BLE_2M:
      BB_LED_2M_ON();
      break;
    case BB_PHY_BLE_CODED:
      BB_LED_CODED_ON();
      break;
    default:
      break;
  }

#if defined(NRF52832_XXAA)
  *(volatile uint32_t*)0x40001777 = 0UL;   /* Disable fault tolerant AA correlator. */
#endif
}

#if (USE_RTC_BB_CLK)
/*************************************************************************************************/
/*!
 *  \brief      Set the time for the PAL_BB_TIMER to start.
 *
 *  \param      startTime     PAL_BB_TIMER start time.
 *
 */
/*************************************************************************************************/
static void palBbStartTimer(uint32_t startTime)
{
  bbEventStartTime = PAL_BB_RTC->CC[1] = startTime;
  PAL_BB_RTC->EVENTS_COMPARE[1] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_RTC->EVENTS_COMPARE[1]);
}
#endif

/*************************************************************************************************/
/*!
 *  \brief      Low power operation.
 *
 *  \note       Called by upper baseband code.
 */
/*************************************************************************************************/
void PalBbBleLowPower(void)
{
  /* Stop high frequency timer between BLE events if low lower mode. */
#if (USE_RTC_BB_CLK)
  PAL_BB_TIMER->TASKS_STOP = 1;
  PAL_BB_TIMER->TASKS_CLEAR = 1;
#endif
}

#if (BB_ENABLE_INLINE_ENC_TX)
/*************************************************************************************************/
/*!
 *  \brief      Enable or disable inline encryption on transmit.
 *
 *  \param      enable  Boolean flag to enable or disable TX encryption
 */
/*************************************************************************************************/
static void palBbBleInlineEncryptTxEnable(bool_t enable)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  /* copy encryption enable to global variable */
  bbEncryptTxFlag = enable;
}
#endif

#if (BB_ENABLE_INLINE_DEC_RX)
/*************************************************************************************************/
/*!
 *  \brief      Enable or disable inline decryption on reception.
 *
 *  \param      enable  Boolean flag to enable or disable RX decryption
 */
/*************************************************************************************************/
static void palBbBleInlineDecryptRxEnable(bool_t enable)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  /* copy decryption enable to global variable */
  bbDecryptRxFlag = enable;
}
#endif

#if (BB_ENABLE_INLINE_ENC_TX || BB_ENABLE_INLINE_DEC_RX)
/*************************************************************************************************/
/*!
 *  \brief      Set inline encryption/decryption MIC suppression.
 *
 *  \param      enable  Boolean flag to indicate if MIC is suppressed
 */
/*************************************************************************************************/
static void palBbBleInlineEncryptDecryptSuppressMic(bool_t enable)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  /* copy to global variable */
  bbEncryptTxSuppressMic = enable;
}

/*************************************************************************************************/
/*!
 *  \brief      Set inline encryption/decryption key.
 *
 *  \param      key     Pointer to 16-byte key value
 */
/*************************************************************************************************/
static void palBbBleInlineEncryptDecryptSetKey(uint8_t *pKey)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  #if BB_ENABLE_ASSERT == TRUE
  keySetFlag = 1;
  #endif

  /* populate encryption structure with reversed stored key */
  for (unsigned int i=0; i<16; i++)
  {
    bbEncryptCcmData[i] = pKey[15-i];
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set inline encryption/decryption IV (initialization vector).
 *
 *  \param      key     Pointer to 8-byte value for IV
 */
/*************************************************************************************************/
static void palBbBleDrvInlineEncryptDecryptSetIv(uint8_t *pIV)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  #if BB_ENABLE_ASSERT == TRUE
  ivSetFlag = 1;
  #endif

  /* populate encryption structure with reversed stored IV */
  memcpy(&bbEncryptCcmData[25], pIV, 8);
}

/*************************************************************************************************/
/*!
 *  \brief      Set inline encryption/decryption type.
 *
 *  \param      encType     0=PAL_BB_TYPE_ACL, CIS, BIS
 */
/*************************************************************************************************/
static void palBbBleInlineEncryptSetType(uint8_t encType)
{
  #if BB_ENABLE_ASSERT == TRUE
  encryptTypeSetFlag = 1;
  #endif

  bbEncryptType = encType;
}

/*************************************************************************************************/
/*!
 *  \brief      Set inline encryption/decryption direction bit.
 *
 *  \param      dir     0=slave, non-zero=master
 */
/*************************************************************************************************/
void palBbBleInlineEncryptDecryptSetDirection(uint8_t dir)
{
  #if BB_ENABLE_ASSERT == TRUE
  directionSetFlag = 1;
  #endif

  bbEncryptDirection = dir;
}

/*************************************************************************************************/
/*!
 *  \brief      Set the inline encryption packet count for transmit.
 *
 *  \param      pktCtr  Packet counter value, a 39-bit value (upper bits are ignored).
 */
/*************************************************************************************************/
void PalBbBleSetInlineEncryptPacketCount(uint64_t pktCtr)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  #if BB_ENABLE_ASSERT == TRUE
  txPacketCountSetFlag = 1;
  #endif

  /* populate packet counter */
  bbEncryptPacketCounter = pktCtr;
}

/*************************************************************************************************/
/*!
 *  \brief      Set the inline decryption packet count for receive.
 *
 *  \param      pktCtr  Packet counter value, a 39-bit value (upper bits are ignored).
 */
/*************************************************************************************************/
void PalBbBleSetInlineDecryptPacketCount(uint64_t pktCtr)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  #if BB_ENABLE_ASSERT == TRUE
  rxPacketCountSetFlag = 1;
  #endif

  /* populate packet counter */
  bbDecryptPacketCounter = pktCtr;
}
#endif

/*************************************************************************************************/
/*!
 *  \brief      Initialize the BLE baseband driver.
 *
 *  One-time initialization of BLE baseband driver.
 */
/*************************************************************************************************/
void PalBbBleInit(void)
{
  BB_ASSERT( LL_AA_LEN == (NORDIC_PREFIX_LEN + NORDIC_BASE_ADDR_LEN) ); /* internal integrity check */
  BB_ASSERT(driverState == NULL_STATE); /* driver should only be initialized once */

  /* initialize callback routines to NULL */
  bbTxCallback = NULL;
  bbRxCallback = NULL;

  /* give timeout an initial value, used by assert */
  bbRxTimeoutUsec = 0;

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
  NRF_RADIO->MODECNF0 = RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos;
#endif

#if defined(NRF51)
  /* if available, copy in the factory radio calibration values  */
  if ((NRF_FICR->OVERRIDEEN & FICR_OVERRIDEEN_BLE_1MBIT_Msk) == 0)
  {
    NRF_RADIO->OVERRIDE0 =  NRF_FICR->BLE_1MBIT[0];
    NRF_RADIO->OVERRIDE1 =  NRF_FICR->BLE_1MBIT[1];
    NRF_RADIO->OVERRIDE2 =  NRF_FICR->BLE_1MBIT[2];
    NRF_RADIO->OVERRIDE3 =  NRF_FICR->BLE_1MBIT[3];
    NRF_RADIO->OVERRIDE4 =  NRF_FICR->BLE_1MBIT[4] | RADIO_OVERRIDE4_ENABLE_Msk;
  }
#endif

  /* CRC configuration */
  NRF_RADIO->CRCCNF  = LL_CRC_LEN | RADIO_CRCCNF_SKIP_ADDR_Msk;  /* set length, skip address as part of CRC */
  NRF_RADIO->CRCPOLY = 0x0000065B;  /* crc polynomial, value from bluetooth spec */

  /* set logical address 0 for transmit (uses PREFIX0.AP0/BASE0 pair) */
  NRF_RADIO->TXADDRESS = 0;

  /* enable receive address on logical address 0 (uses PREFIX0.AP0/BASE0 pair) */
  NRF_RADIO->RXADDRESSES = 1;  /* NOTE: this is a bitmask, a '1' enables logical address 0 */

  /* set default direction in CCM structure */
  bbEncryptCcmData[24] = 0;  /* 0=slave, 1=master */

  /* set default value for data whitening */
  bbRadioPcnf1WhiteEn = RADIO_PCNF1_WHITEEN_Enabled;

  /* set initial value for TIFS adjustment */
  bbTxTifsAdj = 0;

  /* set default of no encryption */
  bbEncryptTxFlag = 0;
  bbDecryptRxFlag = 0;

#if (USE_RTC_BB_CLK)
  PAL_BB_RTC->EVTENCLR = RTC_EVTENCLR_COMPARE1_Msk;
  PAL_BB_RTC->EVTENCLR = RTC_EVTENCLR_COMPARE2_Msk;
#endif

  /* update driver state */
  driverState = SLEEP_STATE;

  /////////////////////////////////////////////////////////////////////////////////
  #if BB_ENABLE_ASSERT
  /* initialize buffers with 0xAA's, last two bytes used by assert to detect buffer overrun */
  memset(bbEncryptPlainBuf,     0xAA, sizeof(bbEncryptPlainBuf));
  memset(bbEncryptEncryptedBuf, 0xAA, sizeof(bbEncryptEncryptedBuf));
  memset(bbEncryptScratchBuf,   0xAA, sizeof(bbEncryptScratchBuf));
  #endif
  /////////////////////////////////////////////////////////////////////////////////

  bbTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  bbRxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  tifsTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;

  PalBbRegisterProtIrq(BB_PROT_BLE, BbBleDrvTimerIRQHandler, BbBleDrvRadioIRQHandler);
  PalBbRegisterProtIrq(BB_PROT_BLE_DTM, BbBleDrvTimerIRQHandler, BbBleDrvRadioIRQHandler);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable the BB hardware.
 *
 *  Wake the BB hardware out of sleep and enable for operation. All BB functionality is
 *  available when this routine completes. BB clock is set to zero and started.
 */
/*************************************************************************************************/
void PalBbBleEnable(void)
{
  /* Give the BB the highest priority. */
  NVIC_SetPriority(RADIO_IRQn, 1);
  NVIC_SetPriority(PAL_BB_IRQn, 1);

  BB_ASSERT(NVIC_GetPriority(RADIO_IRQn) == NVIC_GetPriority(PAL_BB_IRQn)); /* BB driver related interrupts must have same priority */
  BB_ASSERT(driverState == SLEEP_STATE); /* the BB driver should never be re-enabled */
  BB_ASSERT(NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_STATE_Msk); /* HF clock must be running */
#if (USE_RTC_BB_CLK)
  if (!(NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_SRC_Msk))
  {
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
  }

#else
  BB_ASSERT(NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_SRC_Msk);   /* HF clock source must be the crystal */
#endif

  /* stop timer if it was somehow running (timer must be stopped for configuration) */
  PAL_BB_TIMER->TASKS_STOP  = 1;

  /* clear timer to zero count */
  PAL_BB_TIMER->TASKS_CLEAR = 1;

  /* configure timer */
  PAL_BB_TIMER->MODE      = TIMER_MODE_MODE_Timer;
  PAL_BB_TIMER->BITMODE   = TIMER_BITMODE_BITMODE_32Bit;
  PAL_BB_TIMER->PRESCALER = TIMER_PRESCALER;  /* f = 16MHz / (2 ^ TIMER_PRESCALER),  for TIMER_PRESCALER of 4, clock has 1us resolution
                                                                                       TIMER_PRESCALER of 3, clock is 0.5us resolution
                                                                                       TIMER_PRESCALER of 2, clock is 0.25us resolution
                                                                                       TIMER_PRESCALER of 1, clock is 0.125us resolution  */

  /*
   *   Configure the PPI Channel 13 -  "Chan 13: RTC.COMPARE[0] -> TIMER[3].START"
   *
   *   This channel is used to trigger starting TIMER[3] for radio operations.
   */
#if (USE_RTC_BB_CLK)
  /* TIMER0 starts when RTC0.COMPARE[1] event is triggered */
  NRF_PPI->CH[13].EEP = (uint32_t) &PAL_BB_RTC->EVENTS_COMPARE[1];
  NRF_PPI->CH[13].TEP = (uint32_t) &PAL_BB_TIMER->TASKS_START;
  NRF_PPI->CHENSET = PPI_CHENSET_CH13_Msk;                                        /* enable channel */

  /* HFCLK starts when RTC0.COMPARE[2] event is triggered */
  NRF_PPI->CH[10].EEP = (uint32_t) &PAL_BB_RTC->EVENTS_COMPARE[2];
  NRF_PPI->CH[10].TEP = (uint32_t) &NRF_CLOCK->TASKS_HFCLKSTART;
  NRF_PPI->CHENSET = PPI_CHENSET_CH10_Msk;                                        /* enable channel */
#else
  /* TIMER0 is a free running clock */
  PAL_BB_TIMER->TASKS_START = 1;
#endif

  /*
   *   Configure the PPI Channel 14 -  "Chan 14: TIMER[3].COMPARE[0] -> TXEN/RXEN"
   *
   *   This channel is used to trigger radio task TXEN, or RXEN, on timer event COMPARE[0].
   *   It is only enabled when needed. The PPI task parameter is meant to be configured just
   *   before the channel is enabled. This task will be either TXEN or RXEN depending on radio
   *   operation. The event is static and never changes. It is configured below.
   */
  NRF_PPI->CH[14].EEP = (uint32_t) &PAL_BB_TIMER->EVENTS_COMPARE[0];   /* configure event */

  /*
   *   Configure and enable PPI Channel 15 -  "Chan 15: PAYLOAD -> CAPTURE[2]"
   *
   *   This channel is used to capture the timer value on every radio PAYLOAD event.
   *   It is configured to trigger timer task CAPTURE[2] which copies the timer to CC[2].
   *
   *   Once this channel is set up and enabled, it requires no further action.
   *   It will stay active until the BB driver is disabled.
   */
  NRF_PPI->CH[15].EEP = (uint32_t) &NRF_RADIO->EVENTS_PAYLOAD;        /* configure event */
  NRF_PPI->CH[15].TEP = (uint32_t) &PAL_BB_TIMER->TASKS_CAPTURE[2];     /* configure task */
  NRF_PPI->CHENSET = PPI_CHENSET_CH15_Msk;                            /* enable channel */

  /*
   *   Configure and enable PPI Channel 16 -  "Chan 16: ADDRESS -> CAPTURE[4]"
   *
   *   This channel is used to capture the timer value on every radio ADDRESS event.
   *   It is configured to trigger timer task CAPTURE[4] which copies the timer to CC[4].
   *
   *   Once this channel is set up and enabled, it requires no further action.
   *   It will stay active until the BB driver is disabled.
   */
  NRF_PPI->CH[16].EEP = (uint32_t) &NRF_RADIO->EVENTS_ADDRESS;        /* configure event */
  NRF_PPI->CH[16].TEP = (uint32_t) &PAL_BB_TIMER->TASKS_CAPTURE[4];     /* configure task */
  NRF_PPI->CHENSET = PPI_CHENSET_CH16_Msk;                            /* enable channel */

  /*
   *   NOTE: Per Nordic support team response, PPI channels do not require a reset.
   *         The PPI configured event will trigger the configured task every time it happens,
   *         regardless if the EVENT register is cleared or not.  (This is not clear from the
   *         reference manual.)
   */

  /* clear out and enable BB scheduler interrupts at system level */
  PAL_BB_TIMER->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(PAL_BB_IRQn);
  NVIC_EnableIRQ(PAL_BB_IRQn);

  /* clear out and enable RADIO interrupts at system level */
  NRF_RADIO->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(RADIO_IRQn);
  NVIC_EnableIRQ(RADIO_IRQn);

  /* clear out and enable CCM/AAR interrupts at system level */
  NRF_CCM->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(CCM_AAR_IRQn);
  NVIC_EnableIRQ(CCM_AAR_IRQn);

   /*
    *  Enable all the radio interrupts that are used. Per design, these are to
    *  be left on as long as the BB driver is enabled.
    */

#if (USE_RTC_BB_CLK)
  PAL_BB_RTC->EVTENSET = RTC_EVTENSET_COMPARE1_Msk;
  PAL_BB_RTC->EVTENSET = RTC_EVTENSET_COMPARE2_Msk;
#endif

  /* enable the radio "TIFS Expired" interrupt (triggers on radio PAYLOAD event) */
  NRF_RADIO->EVENTS_READY = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_READY);
  NRF_RADIO->INTENSET = RADIO_INTENSET_READY_Msk;

  /* enable the radio "Timestamp/TIFS Setup" interrupt (triggers on radio PAYLOAD event) */
  NRF_RADIO->EVENTS_PAYLOAD = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_PAYLOAD);
  NRF_RADIO->INTENSET = RADIO_INTENSET_PAYLOAD_Msk;

  /* enable the radio "TX-RX Complete" interrupt (triggers on radio END event) */
  NRF_RADIO->EVENTS_DISABLED = 0;
  NRF_RADIO->EVENTS_END = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_END);
  NRF_RADIO->INTENSET = RADIO_INTENSET_END_Msk;

  /* put the radio is a definite know state */
  palBbRadioHardStop();
}

/*************************************************************************************************/
/*!
 *  \brief      Disable the BB hardware.
 *
 *  Disable the baseband and put radio hardware to sleep. Must be called from an idle state.
 *  A radio operation cannot be in progress.
 */
/*************************************************************************************************/
void PalBbBleDisable(void)
{
  BB_ASSERT(driverState == IDLE_STATE); /* can only disable if radio is idle */

  /* slight chance that a TIFS operation is ramping up, this will stop it */
  palBbRadioHardStop();

  /* stop timer */
  PalBbBleLowPower();

  /* disable PPI channels */
  NRF_PPI->CHENCLR = PPI_CHENCLR_CH14_Msk;   /* Chan 14: COMPARE[0] -> TXEN/RXEN */
  NRF_PPI->CHENCLR = PPI_CHENCLR_CH15_Msk;   /* Chan 15: PAYLOAD -> CAPTURE[2] */
  NRF_PPI->CHENCLR = PPI_CHENCLR_CH16_Msk;   /* Chan 16: ADDRESS -> CAPTURE[4] */

#if (USE_RTC_BB_CLK)
  PAL_BB_RTC->EVTENCLR = RTC_EVTENCLR_COMPARE1_Msk;
  PAL_BB_RTC->EVTENCLR = RTC_EVTENCLR_COMPARE2_Msk;
#endif

  /* disable and clean up TIMER0 interrupts */
  NVIC_DisableIRQ(PAL_BB_IRQn);
  PAL_BB_TIMER->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(PAL_BB_IRQn);

  /* disable and clean up RADIO interrupts */
  NVIC_DisableIRQ(RADIO_IRQn);
  NRF_RADIO->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(RADIO_IRQn);

  /* disable and clean up CCM/AAR interrupts */
  NVIC_DisableIRQ(CCM_AAR_IRQn);
  NRF_CCM->INTENCLR = 0xFFFFFFFF;
  NVIC_ClearPendingIRQ(CCM_AAR_IRQn);

  /* update driver state */
  driverState = SLEEP_STATE;
}

/*************************************************************************************************/
/*!
 *  \brief      Set the packet transmit completion handler.
 *
 *  \param      pParam     Callback called upon the completion of a transmit operation.
 *
 *  The given \a txCback routine is called once and only once in response to \a BbDrvTx(). It
 *  should be called in the ISR context due to the transmit completion interrupt from the BB.
 *  If \a BbDrvTxCancel() is called with a return value of TRUE, \a txCback must not be executed.
 */
/*************************************************************************************************/
void PalBbBleSetDataParams(const PalBbBleDataParam_t *pParam)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */
  BB_ASSERT(pParam->txCback != NULL); /* must be set to something */
  BB_ASSERT(pParam->rxCback != NULL); /* must be set to something */

  /* store the callback function addresses to local memory */
  bbTxCallback = pParam->txCback;
  bbRxCallback = pParam->rxCback;

  /* store due time for future use */
  bbAntennaDueTimeUsec = pParam->dueUsec;

  /* store timeout value for future use */
  bbRxTimeoutUsec = pParam->rxTimeoutUsec;

  /////////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  bbTxCallback = DiagFauxTxCallback;
  diagTxCallback = pParam->txCback;
  bbRxCallback = DiagFauxRxCallback;
  diagRxCallback = pParam->rxCback;
  #endif
  /* for diagnostic purposes, an override scheme where all callbacks pass through an internal path. */
  /////////////////////////////////////////////////////////////////////////////////
}

/*************************************************************************************************/
/*!
 *  \brief      Set the operation parameters.
 *
 *  \param      pOpParam    Operations parameters.
 *
 *  Calling this routine will set parameters for the next transmit or receive operations.
 */
/*************************************************************************************************/
void PalBbBleSetOpParams(const PalBbBleOpParam_t *pOpParam)
{
  bbOpParam = *pOpParam;
}

/*************************************************************************************************/
/*!
 *  \brief      Set channelization parameters.
 *
 *  \param      pChan      Channelization parameters.
 */
/*************************************************************************************************/
static void palBbBleSetChannelParam(PalBbBleChan_t *pChan)
{
  uint8_t rfChan;
  int8_t  txPower;

  /* get the channel index into a local variable, for efficiency */
  bbChanIndex = pChan->chanIdx;

  /* save the PHYs */
  bbTxPhy = pChan->txPhy;
  bbRxPhy = pChan->rxPhy;
  bbTxPhyOptions = pChan->initTxPhyOptions;
  tifsTxPhyOptions = pChan->tifsTxPhyOptions;

  BB_ASSERT((pChan->txPhy == BB_PHY_BLE_1M) ||
            (pChan->txPhy == BB_PHY_BLE_2M) ||
            (pChan->txPhy == BB_PHY_BLE_CODED));
  BB_ASSERT((pChan->rxPhy == BB_PHY_BLE_1M) ||
            (pChan->rxPhy == BB_PHY_BLE_2M) ||
            (pChan->rxPhy == BB_PHY_BLE_CODED));

  /* convert channel index to RF channel, per bluetooth specification */
  rfChan = 0; /* initialize to zero to suppress compiler 'uninitialized' warning */
  if (bbChanIndex <= 10)        {  rfChan = bbChanIndex + 1;  }
  else if (bbChanIndex <= 36)   {  rfChan = bbChanIndex + 2;  }
  else if (bbChanIndex == 37)   {  rfChan = 0;   }
  else if (bbChanIndex == 38)   {  rfChan = 12;  }
  else if (bbChanIndex == 39)   {  rfChan = 39;  }
  else
  {
    BB_ASSERT(0); /* illegal channel index */
  }

  /* set the radio frequency */
  NRF_RADIO->FREQUENCY = 2 + (rfChan * 2);  /* over-the-air freq = 2400Hz + value in FREQUENCY */

  /* set the data whitening initial value, based on channel index per bluetooth spec */
  NRF_RADIO->DATAWHITEIV = bbChanIndex | 0x40;  /* position zero is always '1', it gets "or'ed" in here with 0x40 */

  /* set access addresses for logical address 0, which uses the PREFIX0.AP0/BASE0 pair */
  NRF_RADIO->PREFIX0 = (pChan->accAddr & 0xFF000000) >> 24;
  NRF_RADIO->BASE0   = (pChan->accAddr & 0x00FFFFFF) <<  8;

#if (LL_ENABLE_TESTER == TRUE)
  /* store access address overrides */
  bbRxAccAddr = pChan->accAddrRx;
  bbTxAccAddr = pChan->accAddrTx;
#endif

  /* set initial CRC */
  NRF_RADIO->CRCINIT = pChan->crcInit;

#if (LL_ENABLE_TESTER == TRUE)
  /* store CRC init overrides */
  bbRxCrcInit = pChan->crcInitRx;
  bbTxCrcInit = pChan->crcInitTx;
#endif

  /* get the transmit power into a local variable, for efficiency */
  txPower = pChan->txPower;

#if defined(NRF52840_XXAA)
  if      (txPower >=   8)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos8dBm;   txPower =   8;  }
  else if (txPower >=   7)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos7dBm;   txPower =   7;  }
  else if (txPower >=   6)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos6dBm;   txPower =   6;  }
  else if (txPower >=   5)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos5dBm;   txPower =   5;  }
  else if (txPower >=   4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos4dBm;   txPower =   4;  }
  else if (txPower >=   3)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos3dBm;   txPower =   3;  }
  else if (txPower >=   2)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos2dBm;   txPower =   2;  }
  else if (txPower >=   0)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;      txPower =   0;  }
  else if (txPower >=  -4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg4dBm;   txPower =  -4;  }
  else if (txPower >=  -8)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg8dBm;   txPower =  -8;  }
  else if (txPower >= -12)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg12dBm;  txPower = -12;  }
  else if (txPower >= -16)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg16dBm;  txPower = -16;  }
  else if (txPower >= -20)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg20dBm;  txPower = -20;  }
  else                      {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg40dBm;  txPower = -40;  }
#elif defined(NRF52832_XXAA)
  if      (txPower >=   4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos4dBm;   txPower =   4;  }
  else if (txPower >=   3)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos3dBm;   txPower =   3;  }
  else if (txPower >=   0)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;      txPower =   0;  }
  else if (txPower >=  -4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg4dBm;   txPower =  -4;  }
  else if (txPower >=  -8)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg8dBm;   txPower =  -8;  }
  else if (txPower >= -12)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg12dBm;  txPower = -12;  }
  else if (txPower >= -16)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg16dBm;  txPower = -16;  }
  else if (txPower >= -20)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg20dBm;  txPower = -20;  }
  else                      {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg40dBm;  txPower = -40;  }
#else
  if      (txPower >=   4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos4dBm;   txPower =   4;  }
  else if (txPower >=   0)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;      txPower =   0;  }
  else if (txPower >=  -4)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg4dBm;   txPower =  -4;  }
  else if (txPower >=  -8)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg8dBm;   txPower =  -8;  }
  else if (txPower >= -12)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg12dBm;  txPower = -12;  }
  else if (txPower >= -16)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg16dBm;  txPower = -16;  }
  else if (txPower >= -20)  {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg20dBm;  txPower = -20;  }
  else                      {  NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Neg30dBm;  txPower = -30;  }
#endif

  /* set the power parameter to the value that was actually set in hardware */
  pChan->txPower = txPower;

#if BB_ENABLE_INLINE_ENC_TX
  /* enable inline encryption */
  palBbBleInlineEncryptTxEnable(pChan->enc.enaEncrypt);
#endif

#if BB_ENABLE_INLINE_DEC_RX
  /* enable inline decryption */
  palBbBleInlineDecryptRxEnable(pChan->enc.enaDecrypt);
#endif

#if (BB_ENABLE_INLINE_ENC_TX || BB_ENABLE_INLINE_DEC_RX)
  /* set encryption parameters */
  if (pChan->enc.enaEncrypt || pChan->enc.enaDecrypt)
  {
    palBbBleInlineEncryptDecryptSuppressMic(FALSE); /* TODO add MIC suppression */
    palBbBleInlineEncryptDecryptSetKey(pChan->enc.sk);
    palBbBleDrvInlineEncryptDecryptSetIv(pChan->enc.iv);
    palBbBleInlineEncryptSetType(pChan->enc.type);
    palBbBleInlineEncryptDecryptSetDirection(pChan->enc.dir);
  }
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Set channelization parameters.
 *
 *  \param      pChan      Channelization parameters.
 *
 *  Calling this routine will set these parameters for all future transmit and receive operations
 *  until this routine is called again providing new parameters.
 *
 *  The setting of channelization parameters influence the operations of the following listed
 *  routines. Therefore, this routine is called to set the channel characteristics before
 *  the use of these listed packet routines.
 *
 *  - \a BbDrvTx()
 *  - \a BbDrvRx()
 *  - \a BbDrvTxTifs()
 *  - \a BbDrvRxTifs()
 *
 *  \note       The \a pParam contents are not guaranteed to be static and is only valid in the
 *              context of the call to this routine. Therefore parameters requiring persistence
 *              should be copied.
 */
/*************************************************************************************************/
void PalBbBleSetChannelParam(PalBbBleChan_t *pChan)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  /* stop any TIFS operation that might be ramping up */
  palBbRadioHardStop();

  palBbBleSetChannelParam(pChan);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable data whitening.
 *
 *  \param      enable       flag to indicate data whitening
 *
 *  Sets an internal variable that indicates if data whitening is enabled or not.
 *  The value is used later when setting PCNF1 at beginning of TX or RX.
 */
/*************************************************************************************************/
void PalBbBleEnableDataWhitening(bool_t enable)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */

  /* assume disabled */
  bbRadioPcnf1WhiteEn = RADIO_PCNF1_WHITEEN_Disabled;

  /* if enabled set accordingly */
  if (enable)
  {
    bbRadioPcnf1WhiteEn = RADIO_PCNF1_WHITEEN_Enabled;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Enable or disable PRBS15.
 *
 *  \param      enable       flag to indicate PRBS15
 *
 *  Immediately enable or disable continuous PRBS15 bitstream.
 */
/*************************************************************************************************/
void PalBbBleEnablePrbs15(bool_t enable)
{
  if (enable)
  {
    BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle to enter this mode */

    /* stop the radio in case a TIFS is ramping up */
    palBbRadioHardStop();

    /* set test mode */
#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
    palBbSetRadioMode(BB_PHY_BLE_1M, BB_PHY_OPTIONS_DEFAULT);
    NRF_RADIO->MODECNF0 = RADIO_MODECNF0_DTX_Center << RADIO_MODECNF0_DTX_Pos;
#else
    palBbSetRadioMode(BB_PHY_BLE_1M, BB_PHY_OPTIONS_DEFAULT);
    NRF_RADIO->TEST = (RADIO_TEST_CONST_CARRIER_Enabled << RADIO_TEST_CONST_CARRIER_Pos) |
                      (RADIO_TEST_PLL_LOCK_Enabled      << RADIO_TEST_PLL_LOCK_Pos);
#endif

    /* start transmitting constant carrier */
    NRF_RADIO->TASKS_TXEN = 1;

    /* update the driver state */
    driverState = PRBS15_STATE;
  }
  else
  {
    BB_ASSERT(driverState == PRBS15_STATE); /* not in test mode, cannot disable */

#if defined(NRF52840_XXAA) || defined(NRF52832_XXAA)
    NRF_RADIO->MODECNF0 = 0;
#else
    /* disable test mode */
    NRF_RADIO->TEST = 0;

    /* stop the radio */
    palBbRadioHardStop();
#endif

    /* update the driver state */
    driverState = IDLE_STATE;
    BB_LED_OFF();
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Transmit a packet.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 */
/*************************************************************************************************/
void PalBbBleTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt)
{
  uint8_t *pBuf = palBbGetTxData(descs, cnt);

#if defined(BOARD_PCA10056)
  /*
   *   Observed frequent failures with consecutive coded phy TX's.
   *   In the case, radio stays in TXDISABLE(temporary transition state) for too long.
   *   Calling palBbRadioHardStop multiple times seems to settle radio state into idle(Disabled) state.
   */
  if (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled)
  {
    uint8_t num_cleanup = 0;
    uint8_t busycount;

    while (num_cleanup < 3)
    {
      palBbRadioHardStop();

      busycount = 0;
      while ((NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) && (busycount < 10))
      {
        busycount++;
      }

      if (busycount < 10)
      {
        /* Radio state is cleared now, continue TxData operation. */
        break;
      }

      if (++num_cleanup == 3)
      {
        palBbRestoreTrl();
        bbTxCallback(BB_STATUS_FAILED);
        return;
      }
    }
  }
#else
  palBbRadioHardStop();
#endif

  BB_ASSERT(driverState == IDLE_STATE);                       /* driver must be idle */
  BB_ASSERT(bbTxCallback != NULL);                            /* callback must be configured */
  BB_ALIGN_ASSERT(pBuf);

  /////////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  BB_ASSERT(diagOp == DIAG_NULL); /* apparently previous operation did not complete */
  diagOp = DIAG_TX;
  diagTxTotalCalls++;
  diagTxCalls++;
  #endif
  /////////////////////////////////////////////////////////////////////////////////

#if (LL_ENABLE_TESTER == TRUE)
  /* set Tx access addresses */
  NRF_RADIO->PREFIX0 = (bbTxAccAddr & 0xFF000000) >> 24;
  NRF_RADIO->BASE0   = (bbTxAccAddr & 0x00FFFFFF) <<  8;
  if (invalidateAccAddrOnceTx)
  {
    invalidateAccAddrOnceTx = FALSE;

    /* invalidate */
    NRF_RADIO->PREFIX0 = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
    NRF_RADIO->BASE0   = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
  }

  uint16_t hdr;
  BYTES_TO_UINT16(hdr, pBuf);

  if ((hdr & bbModifyTxHdrMask) == bbModifyTxHdrValue)
  {
    if (bbTxAccAddrInvalidChanMask & (UINT64_C(1) << bbChanIndex))
    {
      if ((bbTxAccAddrInvalidAdjMask & (1 << bbTxAccAddrInvalidStep)) || bbTxAccAddrShiftMask)
      {
        if (bbTxAccAddrShiftMask)
        {
          uint32_t invalidAA = (1 << bbTxAccAddrInvalidStep) ^ bbTxAccAddr;

          NRF_RADIO->PREFIX0 = (invalidAA & 0xFF000000) >> 24;
          NRF_RADIO->BASE0   = (invalidAA & 0x00FFFFFF) <<  8;
        }
        else
        {
          /* invalidate */
          NRF_RADIO->PREFIX0 = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
          NRF_RADIO->BASE0   = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
        }
      }

      if (!bbTxAccAddrShiftMask)
      {
        if (++bbTxAccAddrInvalidStep > 31)
        {
          bbTxAccAddrInvalidStep = 0;
        }
      }
      else
      {
        if (bbTxAccAddrShiftInc)
        {
          ++bbTxAccAddrInvalidStep;
        }
        else
        {
          --bbTxAccAddrInvalidStep;
        }

        if (bbTxAccAddrInvalidStep > 20 && bbTxAccAddrShiftInc)
        {
          bbTxAccAddrInvalidStep = 31;
          bbTxAccAddrShiftInc = FALSE;
        }
        else if (bbTxAccAddrInvalidStep < 12 && !bbTxAccAddrShiftInc)
        {
          bbTxAccAddrInvalidStep = 1;
          bbTxAccAddrShiftInc = TRUE;
        }
      }
    }
  }

  /* set Tx CRC init */
  NRF_RADIO->CRCINIT = bbTxCrcInit;
  if ((hdr & bbModifyTxHdrMask) == bbModifyTxHdrValue)
  {
    if (bbTxCrcInitInvalidChanMask & (UINT64_C(1) << bbChanIndex))
    {
      if (bbTxCrcInitInvalidAdjMask & (1 << bbTxCrcInitInvalidStep))
      {
        /* invalidate */
        NRF_RADIO->CRCINIT = bbTxCrcInit ^ 0xFFFFFF;
      }

      if (++bbTxCrcInitInvalidStep > 31)
      {
        bbTxCrcInitInvalidStep = 0;
      }
    }
  }
#endif

  /* calculate start time */
  uint32_t txStart = BB_US_TO_BB_TICKS(bbAntennaDueTimeUsec - (NRF5x_tTXEN_BLE_USECS + NRF5x_PROP_DELAY_TX_USECS));

  /* set timer compare CC[0] for time to trigger a transmit */
#if (USE_RTC_BB_CLK)
  uint32_t txStartUsec = bbAntennaDueTimeUsec - (NRF5x_tTXEN_BLE_USECS + NRF5x_PROP_DELAY_TX_USECS);

  if (bbAntennaDueTimeUsec < NRF5x_tTXEN_BLE_USECS + NRF5x_PROP_DELAY_TX_USECS)
  {
    txStartUsec = bbAntennaDueTimeUsec - NRF5x_tTXEN_BLE_USECS - NRF5x_PROP_DELAY_TX_USECS + BB_RTC_MAX_VALUE_US + 1;
  }

  txStart = BB_US_TO_BB_TICKS(txStartUsec);

  int16_t dueOffsetUsec = txStartUsec - BB_TICKS_TO_US(txStart);

  bbDueOffsetUsec = dueOffsetUsec > 0 ? dueOffsetUsec : 0;

  if (bbDueOffsetUsec > 31)
  {
    bbDueOffsetUsec = 31;
  }

  if (bbDueOffsetUsec == 0)
  {
    bbDueOffsetUsec = 1;                /* CC[0] can't trigger on 0 */
  }

  PAL_BB_TIMER->CC[0] = bbDueOffsetUsec;
  PAL_BB_TIMER->EVENTS_COMPARE[0] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[0]);
  palBbStartTimer(txStart);
#else
  BB_ASSERT(bbDueOffsetUsec == 0);     /* Always 0 with HFCLK. */
  PAL_BB_TIMER->CC[0] = txStart;
  PAL_BB_TIMER->EVENTS_COMPARE[0] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[0]);
#endif

  /* configure radio hardware registers for transmit */
  palBbTxHwRadioConfig(pBuf);

  /* set shortcuts to transmit as soon as radio warms up */
  NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk   |
                      RADIO_SHORTS_END_DISABLE_Msk;

  uint32_t currentBbTick = PalBbGetCurrentTime();

  /* see if the due time is in the past (or a very long way, away) */
#if (USE_RTC_BB_CLK)
  if ((txStart - currentBbTick) & 0x00800000) /* Check MSB of 24 bit RTC counter. */
#else
  if ((txStart - currentBbTick) & 0x80000000)
#endif
  {
    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagTxFailBadDueTime++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////

    /* run the callback function */
    palBbRestoreTrl();
    bbTxCallback(BB_STATUS_FAILED);
    return;
  }

  /* configure and enable PPI Chan 14, COMPARE[0] -> TXEN */
  NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_TXEN;  /* configure task */
  NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                  /* enable channel */

  /* see if the compare point was already reached */
  if (PAL_BB_TIMER->EVENTS_COMPARE[0])
  {
    /* it's 99.99999% certain did not make it, but... maybe, just maybe... */
    if (NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled)
    {
      /////////////////////////////////////////////////////////////////////////////////
      #ifdef DIAGNOSTICS
      diagTxFailMissedDueTime++;
      #endif
      /////////////////////////////////////////////////////////////////////////////////

      /* run the callback function */
      palBbRestoreTrl();
      bbTxCallback(BB_STATUS_FAILED);
      return;
    }

    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagTxBlueMoonWin++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////
  }

  /* update the driver state */
  driverState = TX_STATE;
}

/*************************************************************************************************/
/*!
 *  \brief      Transmit packet on TIFS timing.
 *
 *  \param      descs       Transmit data buffer descriptor.
 *  \param      cnt         Transmit data count.
 *
 *  If possible, the transmit will occur at the TIFS timing. If not possible, the callback status
 *  will indicate this.
 */
/*************************************************************************************************/
void PalBbBleTxTifsData(PalBbBleTxBufDesc_t descs[], uint8_t cnt)
{
  uint8_t *pBuf = palBbGetTxData(descs, cnt);

  BB_ASSERT(driverState == IDLE_STATE);                       /* driver must be idle */
  BB_ASSERT(bbTxCallback != NULL);                            /* callback must be configured */
  BB_ALIGN_ASSERT(pBuf);

  /////////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  BB_ASSERT(diagOp == DIAG_NULL);
  diagOp = DIAG_TX_TIFS;
  diagTxTotalCalls++;
  diagTxTifsCalls++;
  #endif
  /////////////////////////////////////////////////////////////////////////////////

#if (LL_ENABLE_TESTER == TRUE)
  /* set Tx access addresses */
  NRF_RADIO->PREFIX0 = (bbTxAccAddr & 0xFF000000) >> 24;
  NRF_RADIO->BASE0   = (bbTxAccAddr & 0x00FFFFFF) <<  8;
  if (invalidateAccAddrOnceTx)
  {
    invalidateAccAddrOnceTx = FALSE;

    /* invalidate */
    NRF_RADIO->PREFIX0 = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
    NRF_RADIO->BASE0   = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
  }

  uint16_t hdr;
  BYTES_TO_UINT16(hdr, pBuf);

  if ((hdr & bbModifyTxHdrMask) == bbModifyTxHdrValue)
  {
    if (bbTxAccAddrInvalidChanMask & (UINT64_C(1) << bbChanIndex))
    {
      if ((bbTxAccAddrInvalidAdjMask & (1 << bbTxAccAddrInvalidStep)) || bbTxAccAddrShiftMask)
      {
        if (bbTxAccAddrShiftMask)
        {
          uint32_t invalidAA = (1 << bbTxAccAddrInvalidStep) ^ bbTxAccAddr;

          NRF_RADIO->PREFIX0 = (invalidAA & 0xFF000000) >> 24;
          NRF_RADIO->BASE0   = (invalidAA & 0x00FFFFFF) <<  8;
        }
        else
        {
          /* invalidate */
          NRF_RADIO->PREFIX0 = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
          NRF_RADIO->BASE0   = ((bbTxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
        }
      }

      if (!bbTxAccAddrShiftMask)
      {
        if (++bbTxAccAddrInvalidStep > 31)
        {
          bbTxAccAddrInvalidStep = 0;
        }
      }
      else
      {
        if (bbTxAccAddrShiftInc)
        {
          ++bbTxAccAddrInvalidStep;
        }
        else
        {
          --bbTxAccAddrInvalidStep;
        }

        if (bbTxAccAddrInvalidStep > 20 && bbTxAccAddrShiftInc)
        {
          bbTxAccAddrInvalidStep = 31;
          bbTxAccAddrShiftInc = FALSE;
        }
        else if (bbTxAccAddrInvalidStep < 12 && !bbTxAccAddrShiftInc)
        {
          bbTxAccAddrInvalidStep = 1;
          bbTxAccAddrShiftInc = TRUE;
        }
      }
    }
  }

  /* set Tx CRC init */
  NRF_RADIO->CRCINIT = bbTxCrcInit;
  if ((hdr & bbModifyTxHdrMask) == bbModifyTxHdrValue)
  {
    if (bbTxCrcInitInvalidChanMask & (UINT64_C(1) << bbChanIndex))
    {
      if (bbTxCrcInitInvalidAdjMask & (1 << bbTxCrcInitInvalidStep))
      {
        /* invalidate */
        NRF_RADIO->CRCINIT = bbTxCrcInit ^ 0xFFFFFF;
      }

      if (++bbTxCrcInitInvalidStep > 31)
      {
        bbTxCrcInitInvalidStep = 0;
      }
    }
  }
#endif

  /* configure radio hardware registers for transmit */
  palBbTxHwRadioConfig(pBuf);

  /* -----------------------------------------------------------------------------*
   *                             TIFS "Ramping Up"
   * -----------------------------------------------------------------------------*/
  if (tifsState == TIFS_TX_RAMPUP)
  {
    /* assert that the READY-to-START shortcut is not set */
    BB_ASSERT((NRF_RADIO->SHORTS & RADIO_SHORTS_READY_START_Msk) == 0);

    /* radio should be ramping up, set shortcuts (previously READY-to-START shortcut was not set) */
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk   |
                        RADIO_SHORTS_END_DISABLE_Msk;

    /* if the READY event has already happened... TIFS almost certainly did not go out */
    if (NRF_RADIO->EVENTS_READY)
    {
      /* but maybe, just maybe, TIFS did go out... */
      if (NRF_RADIO->STATE != RADIO_STATE_STATE_Tx)
      {
        /////////////////////////////////////////////////////////////////////////////////
        #ifdef DIAGNOSTICS
        diagTxTifsFailMissed++;
        #endif
        /////////////////////////////////////////////////////////////////////////////////

        /* no, TIFS was missed, indicate via callback */
        palBbRestoreTrl();
        bbTxCallback(BB_STATUS_FAILED);
        return;  /* exit from here... on purpose, alternative coding is ugly */
      }

      /////////////////////////////////////////////////////////////////////////////////
      #ifdef DIAGNOSTICS
      diagTxTifsBlueMoonWin++;
      #endif
      /////////////////////////////////////////////////////////////////////////////////
    }

    /* update the driver state */
    driverState = TX_STATE;
  }

  /* -----------------------------------------------------------------------------*
   *                            TIFS "Not Possible"
   * -----------------------------------------------------------------------------*/
  else
  {
    /*
     *  The TIFS operation is not possible. Some reasons this could happen:
     *     - the TIFS "window of opportunity" has expired, it ramped up and then shut down
     *     - the previous operation timed out, or was cancelled
     *     - this is the wrong type of TIFS for the window (e.g. TX when ramping up for RX)
     *     - attempting this TIFS as the first operation
     *
     *  NOTE: The radio is not stopped. If an opposite type of TIFS is ramping up,
     *        the radio "TIFS Expired" interrupt will handle it.
     */

    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagTxTifsFailNotPossible++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////

    /* indicate that this TIFS operation is not possible */
    palBbRestoreTrl();
    bbTxCallback(BB_STATUS_FAILED);
  }
}

/*===============================================================================================*/
/*!
 *  \brief      Configure radio for transmit.
 *
 *  \param      pTxBuf - pointer to buffer to transmit
 *
 *  Configuration common to TIFS and non-TIFS transmit.
 *
 */
/*===============================================================================================*/
static void palBbTxHwRadioConfig(uint8_t * pTxBuf)
{
#if defined(NRF52840_XXAA)
  if (bbTxPhy == BB_PHY_BLE_CODED)
  {
    /* Improve Coded sensitivity for nRF52840 chip. */
    *(volatile uint32_t *)0x4000173C |= 0x80000000;
    *(volatile uint32_t *)0x4000173C = ((*(volatile uint32_t *)0x4000173C & 0xFFFFFF00) | 0x5C);
  }

  uint32_t plen = ((bbTxPhy != BB_PHY_BLE_1M) ? ((bbTxPhy != BB_PHY_BLE_2M) ? RADIO_PCNF0_PLEN_LongRange : RADIO_PCNF0_PLEN_16bit) \
                  : RADIO_PCNF0_PLEN_8bit) << RADIO_PCNF0_PLEN_Pos;
#elif defined(NRF52832_XXAA)
  uint32_t plen = ((bbTxPhy == BB_PHY_BLE_2M) ? RADIO_PCNF0_PLEN_16bit : RADIO_PCNF0_PLEN_8bit) << RADIO_PCNF0_PLEN_Pos;
#else
  uint32_t plen = 0;
#endif

  /* set the mode */
  palBbSetRadioMode(bbTxPhy, bbTxPhyOptions);

  if (!bbEncryptTxFlag)
  {
    /*------------------------------------------------------------------------------
     *   No encryption.
     *
     *   When encryption is not needed, the Nordic hardware can be configured to
     *   enable extended packets. This configuration bypasses Nordic's specific
     *   format. This also allows for zero copy as well.
     *
     */

    /* configure PCNF0 */
    NRF_RADIO->PCNF0 = (((uint32_t)(  1   /* S0 field in bytes      */ ))  << RADIO_PCNF0_S0LEN_Pos)                              |
                       (((uint32_t)(  8   /* length field in bits   */ ))  << RADIO_PCNF0_LFLEN_Pos)                              |
                       (((uint32_t)(  0   /* S1 field in bits       */ ))  << RADIO_PCNF0_S1LEN_Pos)                              |
#if defined(NRF52840_XXAA)
                       (((uint32_t)((bbTxPhy == BB_PHY_BLE_CODED) ? 3 : 0 /* TERM field in bits*/ ))  << RADIO_PCNF0_TERMLEN_Pos) |
                       (((uint32_t)((bbTxPhy == BB_PHY_BLE_CODED) ? 2 : 0 /* CI field in bits  */ ))  << RADIO_PCNF0_CILEN_Pos)   |
                       (((uint32_t)(  0   /* CRCINC field           */ ))  << RADIO_PCNF0_CRCINC_Pos)                             |
                       (((uint32_t)(  0   /* S1INCL filed           */ ))  << RADIO_PCNF0_S1INCL_Pos)                             |
#endif
                       ((uint32_t)( plen /* preamble field in bits */ ));

    /* configure PCNF1 */
    NRF_RADIO->PCNF1 = (((uint32_t)(  0   /* maximum packet length  */ ))  << RADIO_PCNF1_MAXLEN_Pos  )  |
                       (((uint32_t)(  0   /* static packet length   */ ))  << RADIO_PCNF1_STATLEN_Pos )  |
                       (((uint32_t)( NORDIC_BASE_ADDR_LEN              ))  << RADIO_PCNF1_BALEN_Pos   )  |
                       (((uint32_t)( RADIO_PCNF1_ENDIAN_Little         ))  << RADIO_PCNF1_ENDIAN_Pos  )  |
                       (((uint32_t)( bbRadioPcnf1WhiteEn               ))  << RADIO_PCNF1_WHITEEN_Pos );

    /* set packet pointer to supplied buffer */
    NRF_RADIO->PACKETPTR = (uint32_t)pTxBuf;
  }
  else
  {
    /*------------------------------------------------------------------------------
     *   Encryption.
     *
     *   Nordic's encryption engine requires a precise format to function. The header
     *   is held in three bytes, instead of two. Each byte represents a field in the
     *   actual Bluetooth header. The Nordic hardware converts these three bytes into
     *   the two byte BLE header before transmitting.
     *
     *   To accommodate this hardware quirk, the buffer must be copied to adjust for
     *   the Nordic "extra byte." Zero copy is not possible. Also, extended packets
     *   are not possible.
     *
     *   NOTE:  Most of the CCM hardware is configured just once, in a different
     *          function, when encryption is first enabled.
     */

    BB_ASSERT( keySetFlag ); /* encryption key has not been set */
    BB_ASSERT( ivSetFlag ); /* iv was not set */
    BB_ASSERT( txPacketCountSetFlag ); /* packet count was not set */
    BB_ASSERT( directionSetFlag ); /* direction was not set */

    /* configure PCNF0 */
    NRF_RADIO->PCNF0 = (((uint32_t)(  1   /* S0 field in bytes      */ ))  << RADIO_PCNF0_S0LEN_Pos)                              |
                       (((uint32_t)(  8   /* length field in bits   */ ))  << RADIO_PCNF0_LFLEN_Pos)                              |
                       (((uint32_t)(  0   /* S1 field in bits       */ ))  << RADIO_PCNF0_S1LEN_Pos)                              |
#if defined(NRF52840_XXAA)
                       (((uint32_t)((bbTxPhy == BB_PHY_BLE_CODED) ? 3 : 0 /* TERM field in bits*/ ))  << RADIO_PCNF0_TERMLEN_Pos) |
                       (((uint32_t)((bbTxPhy == BB_PHY_BLE_CODED) ? 2 : 0 /* CI field in bits  */ ))  << RADIO_PCNF0_CILEN_Pos)   |
                       (((uint32_t)(  0   /* CRCINC field           */ ))  << RADIO_PCNF0_CRCINC_Pos)                             |
                       (((uint32_t)(  1   /* S1INCL field           */ ))  << RADIO_PCNF0_S1INCL_Pos)                             |
#endif
                       ((uint32_t)( plen /* preamble field in bits */ ));

    /* configure PCNF1 */
    NRF_RADIO->PCNF1 = (((uint32_t)(  0   /* maximum packet length  */ ))  << RADIO_PCNF1_MAXLEN_Pos  )  |
                       (((uint32_t)(  0   /* static packet length   */ ))  << RADIO_PCNF1_STATLEN_Pos )  |
                       (((uint32_t)( NORDIC_BASE_ADDR_LEN              ))  << RADIO_PCNF1_BALEN_Pos   )  |
                       (((uint32_t)( RADIO_PCNF1_ENDIAN_Little         ))  << RADIO_PCNF1_ENDIAN_Pos  )  |
                       (((uint32_t)( bbRadioPcnf1WhiteEn               ))  << RADIO_PCNF1_WHITEEN_Pos );

    /* set packet pointer to the soon-to-be encrypted buffer */
    NRF_RADIO->PACKETPTR = (uint32_t)&bbEncryptEncryptedBuf[0];

    /* set encryption direction */
    bbEncryptCcmData[24] = bbEncryptDirection;

    /* set encrypt packet counter */
    bbEncryptCcmData[16] = (bbEncryptPacketCounter >>  0) & 0xFF;
    bbEncryptCcmData[17] = (bbEncryptPacketCounter >>  8) & 0xFF;
    bbEncryptCcmData[18] = (bbEncryptPacketCounter >> 16) & 0xFF;
    bbEncryptCcmData[19] = (bbEncryptPacketCounter >> 24) & 0xFF;
    bbEncryptCcmData[20] = (bbEncryptPacketCounter >> 32) & 0x7F; /* only 7-bits of MSB are used (packet count is 39 bits) */

    /* get length from packet to transmit */
    bbTxLen = pTxBuf[1];
    BB_ASSERT(bbTxLen <= ENC_MAXPACKETSIZE); /* payload exceeds Nordic's maximum possible for encryption */

    /* construct header in Nordic format */
    bbEncryptPlainBuf[0] = pTxBuf[0];  /* field "H" */
    bbEncryptPlainBuf[1] = bbTxLen;    /* field "L" */
    bbEncryptPlainBuf[2] = 0;          /* field "RFU" */

    /* copy the payload into Nordic format transmit buffer */
    memcpy(&bbEncryptPlainBuf[3], &pTxBuf[2], bbTxLen);
    BB_ASSERT(bbEncryptPlainBuf[sizeof(bbEncryptPlainBuf)-1] == 0xAA);  /* buffer overrun from memcpy */
    BB_ASSERT(bbEncryptPlainBuf[sizeof(bbEncryptPlainBuf)-2] == 0xAA);  /* buffer overrun from memcpy */

    NRF_CCM->ENABLE = (CCM_ENABLE_ENABLE_Disabled << CCM_ENABLE_ENABLE_Pos);
    NRF_CCM->ENABLE = (CCM_ENABLE_ENABLE_Enabled  << CCM_ENABLE_ENABLE_Pos);

    NRF_CCM->CNFPTR     = (uint32_t)bbEncryptCcmData;
    NRF_CCM->SCRATCHPTR = (uint32_t)bbEncryptScratchBuf;

    NRF_CCM->INPTR  = (uint32_t)bbEncryptPlainBuf;
    NRF_CCM->OUTPTR = (uint32_t)bbEncryptEncryptedBuf;

    uint32_t ccmModeDatarate;
    switch (bbTxPhy)
    {
      default:
      case BB_PHY_BLE_1M:
        ccmModeDatarate = CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos;
        break;
      case BB_PHY_BLE_2M:
        ccmModeDatarate = CCM_MODE_DATARATE_2Mbit << CCM_MODE_DATARATE_Pos;
        break;
#if defined(NRF52840_XXAA)
      case BB_PHY_BLE_CODED:
        ccmModeDatarate = ((bbTxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? CCM_MODE_DATARATE_500Kbps : CCM_MODE_DATARATE_125Kbps)
                          << CCM_MODE_DATARATE_Pos;
        break;
#endif
    }

    NRF_CCM->MODE = (CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos) |
                    (ccmModeDatarate) |
                    (CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos);

    NRF_CCM->EVENTS_ENDCRYPT = 0;
    NRF_CCM->EVENTS_ENDKSGEN = 0;
    NRF_CCM->EVENTS_ERROR    = 0;

    /* ---------- handle MIC suppression ---------- */
    if (bbEncryptTxSuppressMic)
    {
      /* set the length field in output buffer to 0xFF, used to detect when it has been populated via DMA */
      bbEncryptEncryptedBuf[1] = 0xFF;

      /* clear the ENDKSGEN event, aka "key-stream generation complete" event */
      WAIT_FOR_WR_BUF_EMPTY(NRF_CCM->EVENTS_ENDKSGEN);

      /* enable the ENDKSGEN interrupt, this interrupt will revert length to original "MIC-less" length */
      NRF_CCM->INTENSET = CCM_INTENSET_ENDKSGEN_Msk;
    }
    /* -------------------------------------------- */

#if (LL_ENABLE_TESTER)
    if ((llTesterCb.pktMic) || (llTesterCb.pktLlId))
    {
      /* clear the ENDCRYPT event */
      WAIT_FOR_WR_BUF_EMPTY(NRF_CCM->EVENTS_ENDCRYPT);

      /* enable the ENDCRYPT interrupt, this interrupt will modify the MIC/LLID */
      NRF_CCM->INTENSET = CCM_INTENSET_ENDCRYPT_Msk;
    }
#endif

    /* set shortcut to start encryption immediately after key generation */
    NRF_CCM->SHORTS = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;

    /* start encryption */
    NRF_CCM->TASKS_KSGEN = 1;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      CCM interrupt handler "Key-stream Generation Complete"
 *
 *  \param      None.
 *
 *  This interrupt undoes Nordic's automatic +4 to the length field. This allows transmitting
 *  without the MIC.
 */
/*************************************************************************************************/
void CCM_AAR_IRQHandler(void)
{
  if (bbEncryptTxSuppressMic)
  {
    BB_ASSERT(NRF_CCM->INTENSET == CCM_INTENSET_ENDKSGEN_Msk); /* only keygen complete should be enabled */

    /* -----------------------------------------------------------------------------*
    *                         ENDKSGEN - Key Generation Complete
    * -----------------------------------------------------------------------------*/
    BB_ASSERT(NRF_CCM->EVENTS_ENDKSGEN); /* this event should have been set */

    /* disable this interrupt */
    NRF_CCM->INTENCLR = CCM_INTENCLR_ENDKSGEN_Msk;

    /*
     *  The key-stream generation has just completed. A shortcut should immediately
     *  start encryption which populates the output buffer. The second byte of the
     *  output buffer will be the length. Nordic automatically adds four to account for
     *  the addition of the MIC. To avoid transmitting the MIC in the "MIC-less" mode,
     *  this interrupt readjusts the length before it gets transmitted.
     */

    /* wait for the length field to be populated by the Nordic hardware */
    while (bbEncryptEncryptedBuf[1] == 0xFF)
    {
      ///////////////////////////////////////////////////////////////////////////////
      #ifdef BB_ENABLE_ASSERT
      uint8_t busycount = 0;
      BB_ASSERT(busycount < 10); /* got stuck */
      busycount++;
      #endif
      ///////////////////////////////////////////////////////////////////////////////
    }

    /* overwrite with the length field with the original length, that does not include the MIC */
    BB_ASSERT(bbTxLen == (bbEncryptEncryptedBuf[1] - 4));  /* the populated length should be exactly +4 */
    bbEncryptEncryptedBuf[1] = bbTxLen;
  }

#if (LL_ENABLE_TESTER)
  if ((llTesterCb.pktMic) || (llTesterCb.pktLlId))
  {
    BB_ASSERT(NRF_CCM->EVENTS_ENDCRYPT); /* this event should have been set */

    /* disable this interrupt */
    NRF_CCM->INTENCLR = CCM_INTENCLR_ENDCRYPT_Msk;

    bbEncryptEncryptedBuf[0] ^= llTesterCb.pktLlId & 0x03;

    bbEncryptEncryptedBuf[bbTxLen] ^= (llTesterCb.pktMic >>  0) & 0xFF;
    bbEncryptEncryptedBuf[bbTxLen+1] ^= (llTesterCb.pktMic >>  8) & 0xFF;
    bbEncryptEncryptedBuf[bbTxLen+2] ^= (llTesterCb.pktMic >> 16) & 0xFF;
    bbEncryptEncryptedBuf[bbTxLen+3] ^= (llTesterCb.pktMic >> 24) & 0xFF;
  }
#endif
}

/*************************************************************************************************/
/*!
 *  \brief      Receive packet at the given due time.
 *
 *  \param      pBuf        Transmit data buffer.
 *  \param      len         Length of data buffer.
 *
 *  The receiver is kept on for the amount of time previously configured by function call.
 */
/*************************************************************************************************/
void PalBbBleRxData(uint8_t *pBuf, uint16_t len)
{
  BB_ASSERT(driverState == IDLE_STATE);                   /* driver must be idle */
  BB_ASSERT(bbRxCallback != NULL);                        /* callback must be configured */
  BB_ASSERT(len >= PDU_HEADER_LEN);                       /* buffer must include room for a two byte header */
  BB_ASSERT(len <= (PDU_HEADER_LEN + MAX_PAYLOAD_LEN));   /* buffer should not exceed header + max payload size */
  BB_ALIGN_ASSERT(pBuf);

  /////////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  BB_ASSERT(diagOp == DIAG_NULL);
  diagOp = DIAG_RX;
  diagRxTotalCalls++;
  diagRxCalls++;
  #endif
  /////////////////////////////////////////////////////////////////////////////////

#if (LL_ENABLE_TESTER == TRUE)
  /* set Rx access addresses */
  NRF_RADIO->PREFIX0 = (bbRxAccAddr & 0xFF000000) >> 24;
  NRF_RADIO->BASE0   = (bbRxAccAddr & 0x00FFFFFF) <<  8;
  if (invalidateAccAddrOnceRx)
  {
    invalidateAccAddrOnceRx = FALSE;

    /* invalidate */
    NRF_RADIO->PREFIX0 = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
    NRF_RADIO->BASE0   = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
  }

  if (bbRxAccAddrInvalidChanMask & (UINT64_C(1) << bbChanIndex))
  {
    if (bbRxAccAddrInvalidAdjMask & (1 << bbRxAccAddrInvalidStep))
    {
      /* invalidate */
      NRF_RADIO->PREFIX0 = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
      NRF_RADIO->BASE0   = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
    }

    if (++bbRxAccAddrInvalidStep > 31)
    {
      bbRxAccAddrInvalidStep = 0;
    }
  }

  /* set Rx CRC init */
  NRF_RADIO->CRCINIT = bbRxCrcInit;
  if (bbRxCrcInitInvalidChanMask & (UINT64_C(1) << bbChanIndex))
  {
    if (bbRxCrcInitInvalidAdjMask & (1 << bbRxCrcInitInvalidStep))
    {
      /* invalidate */
      bbRxCrcInitInvalidStep--;
      NRF_RADIO->CRCINIT = bbRxCrcInit ^ 0xFFFFFF;
    }

    if (++bbRxCrcInitInvalidStep > 31)
    {
      bbRxCrcInitInvalidStep = 0;
    }
  }
#endif

  /*
   *   In case there is a TIFS operation ramping up, kill it.
   *   This non-TIFS operation is coming through.
   */
  palBbRadioHardStop();

  /* calculate start time */
  uint32_t rxStart = BB_US_TO_BB_TICKS(bbAntennaDueTimeUsec - (NRF5x_tRXEN_BLE_USECS + NRF5x_PROP_DELAY_RX_USECS));

  /* set timer compare CC[0] for time to trigger a transmit */
#if (USE_RTC_BB_CLK)
  uint32_t rxStartUsec = bbAntennaDueTimeUsec - (NRF5x_tRXEN_BLE_USECS + NRF5x_PROP_DELAY_RX_USECS);

  if (bbAntennaDueTimeUsec < NRF5x_tRXEN_BLE_USECS + NRF5x_PROP_DELAY_RX_USECS)
  {
    rxStartUsec = bbAntennaDueTimeUsec - NRF5x_tRXEN_BLE_USECS - NRF5x_PROP_DELAY_RX_USECS + BB_RTC_MAX_VALUE_US + 1;
  }

  rxStart = BB_US_TO_BB_TICKS(rxStartUsec);

  int16_t dueOffsetUsec = rxStartUsec - BB_TICKS_TO_US(rxStart);

  bbDueOffsetUsec = dueOffsetUsec > 0 ? dueOffsetUsec : 0;

  if (bbDueOffsetUsec > 31)
  {
    bbDueOffsetUsec = 31;
  }

  if (bbDueOffsetUsec == 0)
  {
    bbDueOffsetUsec = 1;                /* CC[0] can't trigger on 0 */
  }

  PAL_BB_TIMER->CC[0] = bbDueOffsetUsec;
  PAL_BB_TIMER->EVENTS_COMPARE[0] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[0]);
  palBbStartTimer(rxStart - 1);        /* Subtract 1 for receive uncertainty due to rounding. */
#else
  BB_ASSERT(bbDueOffsetUsec == 0);     /* Always 0 with HFCLK. */
  PAL_BB_TIMER->CC[0] = rxStart;
  PAL_BB_TIMER->EVENTS_COMPARE[0] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[0]);
#endif

#if (USE_RTC_BB_CLK)
  /* Since receive timestamp rounds down to RTC clock, extend timeout up to 31us
   * to makeup for rounding error. */
  bbRxTimeoutUsec += 31;
#endif

  uint32_t preambleTime;
  uint32_t accessAddrTime;
  int32_t correction;

  switch (bbRxPhy)
  {
    case BB_PHY_BLE_1M:
      preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
      accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
      correction = 0;
      break;
    case BB_PHY_BLE_2M:
      preambleTime = LL_BLE_US_PER_BYTE_2M * LL_PREAMBLE_LEN_2M;
      accessAddrTime = LL_BLE_US_PER_BYTE_2M * LL_AA_LEN;
      correction = 0;
      break;
#if defined(NRF52840_XXAA)
    case BB_PHY_BLE_CODED:
      preambleTime = LL_BLE_US_PER_BIT_CODED_S8 * LL_PREAMBLE_LEN_CODED_BITS;
      accessAddrTime = LL_BLE_US_PER_BYTE_CODED_S8 * LL_AA_LEN;
      correction = 100;
      break;
#endif
    default:
      preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
      accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
      correction = 0;
    }
  /* set timer compare CC[1] for RX timeout (allow time to recognize address, and account for propagation delay) */
  PAL_BB_TIMER->CC[1]  = PAL_BB_TIMER->CC[0]
                       + TICKS_PER_USEC * (bbRxTimeoutUsec
                                           + preambleTime
                                           + accessAddrTime
                                           + NRF5x_PROP_DELAY_RX_USECS
                                           + NRF5x_tRXEN_BLE_USECS
                                           + correction);

  /* clear RX timeout compare event, but do not enable interrupt, that happens in radio isr */
  PAL_BB_TIMER->EVENTS_COMPARE[1] = 0;
  WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[1]);

  /* configure radio hardware registers for receive */
  palBbRxHwRadioConfig(pBuf, len);

  /* configure shortcuts */
  NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk        |
                      RADIO_SHORTS_END_DISABLE_Msk        |
                      RADIO_SHORTS_ADDRESS_RSSISTART_Msk  |
                      RADIO_SHORTS_DISABLED_RSSISTOP_Msk;

  uint32_t currentBbTick = PalBbGetCurrentTime();

  /* see if the due time is in the past (or a very long way, away) */
#if (USE_RTC_BB_CLK)
  if ((rxStart - currentBbTick) & 0x00800000) /* Check MSB of 24 bit RTC counter. */
#else
  if ((rxStart - currentBbTick) & 0x80000000)
#endif
  {
    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagRxFailBadDueTime++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////

    /* run the callback function */
    bbRxCallback(BB_STATUS_FAILED, 0 ,0, 0, 0);
    return;
  }

  /* configure and enable PPI Chan 14, COMPARE[0] -> RXEN */
  NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_RXEN;  /* configure task */
  NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                  /* enable channel */

  /* see if the compare point was already reached */
  if (PAL_BB_TIMER->EVENTS_COMPARE[0])
  {
    /* it's 99.99999% certain did not make it, but... maybe, just maybe... */
    if (NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled)
    {
      /////////////////////////////////////////////////////////////////////////////////
      #ifdef DIAGNOSTICS
      diagRxFailMissedDueTime++;
      #endif
      /////////////////////////////////////////////////////////////////////////////////

      /* run the callback function */
      bbRxCallback(BB_STATUS_FAILED, 0, 0, 0, 0);
      return;
    }

    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagRxBlueMoonWin++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////
  }

  /* update the driver state */
  driverState = RX_STATE;
}

/*************************************************************************************************/
/*!
 *  \brief      Receive packet on TIFS timing.
 *
 *  \param      pBuf        Receive data buffer.
 *  \param      len         Length of data buffer.
 *
 *  The receiver is left on for the minimum amount of time to recognize a receive.
 *
 *  If possible, the receive will occur on the TIFS timing. If not possible, the callback status
 *  will indicate this.
 */
/*************************************************************************************************/
void PalBbBleRxTifsData(uint8_t *pBuf, uint16_t len)
{
  BB_ASSERT(driverState == IDLE_STATE); /* driver must be idle */
  BB_ASSERT(bbRxCallback != NULL);      /* callback must be configured */
  BB_ASSERT(len >= PDU_HEADER_LEN);                       /* buffer must include room for a two byte header */
  BB_ASSERT(len <= (PDU_HEADER_LEN + MAX_PAYLOAD_LEN));   /* buffer should not exceed header + max payload size */
  BB_ALIGN_ASSERT(pBuf);

  /////////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  BB_ASSERT(diagOp == DIAG_NULL);
  diagOp = DIAG_RX_TIFS;
  diagRxTotalCalls++;
  diagRxTifsCalls++;
  #endif
  /////////////////////////////////////////////////////////////////////////////////

#if (LL_ENABLE_TESTER == TRUE)
  /* set Rx access addresses */
  NRF_RADIO->PREFIX0 = (bbRxAccAddr & 0xFF000000) >> 24;
  NRF_RADIO->BASE0   = (bbRxAccAddr & 0x00FFFFFF) <<  8;
  if (invalidateAccAddrOnceRx)
  {
    invalidateAccAddrOnceRx = FALSE;

    /* invalidate */
    NRF_RADIO->PREFIX0 = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
    NRF_RADIO->BASE0   = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
  }

  if (bbRxAccAddrInvalidChanMask & (UINT64_C(1) << bbChanIndex))
  {
    if (bbRxAccAddrInvalidAdjMask & (1 << bbRxAccAddrInvalidStep))
    {
      /* invalidate */
      NRF_RADIO->PREFIX0 = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0xFF000000) >> 24;
      NRF_RADIO->BASE0   = ((bbRxAccAddr ^ 0xFFFFFFFF) & 0x00FFFFFF) <<  8;
    }

    if (++bbRxAccAddrInvalidStep > 31)
    {
      bbRxAccAddrInvalidStep = 0;
    }
  }

  /* set Rx CRC init */
  NRF_RADIO->CRCINIT = bbRxCrcInit;
  if (bbRxCrcInitInvalidChanMask & (UINT64_C(1) << bbChanIndex))
  {
    if (bbRxCrcInitInvalidAdjMask & (1 << bbRxCrcInitInvalidStep))
    {
      /* invalidate */
      bbRxCrcInitInvalidStep--;
      NRF_RADIO->CRCINIT = bbRxCrcInit ^ 0xFFFFFF;
    }

    if (++bbRxCrcInitInvalidStep > 31)
    {
      bbRxCrcInitInvalidStep = 0;
    }
  }
#endif

  /*
   *  NOTE: To make the TIFS timing requirements, timer CC[0] and PPI Chan 14 were previously
   *        configured at the end of the last transmit. The RXEN task will trigger on the next
   *        timer COMPARE0 event.
   */

  /* configure radio hardware registers for receive */
  palBbRxHwRadioConfig(pBuf, len);

  /* -----------------------------------------------------------------------------*
   *                             TIFS "Ramping Up"
   * -----------------------------------------------------------------------------*/
  if (tifsState == TIFS_RX_RAMPUP)
  {
    /* assert that the READY-to-START shortcut is not set */
    BB_ASSERT((NRF_RADIO->SHORTS & RADIO_SHORTS_READY_START_Msk) == 0);

    /* radio should be ramping up, set shortcuts (previously READY-to-START shortcut was not set) */
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk        |
                        RADIO_SHORTS_END_DISABLE_Msk        |
                        RADIO_SHORTS_ADDRESS_RSSISTART_Msk  |
                        RADIO_SHORTS_DISABLED_RSSISTOP_Msk;

    /* if the READY event has already happened... TIFS almost certainly did not go out */
    if (NRF_RADIO->EVENTS_READY)
    {
      /* but maybe, just maybe, TIFS did go out... */
      if (NRF_RADIO->STATE != RADIO_STATE_STATE_Rx)
      {
        /////////////////////////////////////////////////////////////////////////////////
        #ifdef DIAGNOSTICS
        diagRxTifsFailMissed++;
        #endif
        /////////////////////////////////////////////////////////////////////////////////

        /* no, TIFS was missed, indicate via callback */
        bbRxCallback(BB_STATUS_FAILED, 0, 0, 0, 0);
        return; /* exit from here... on purpose, alternative coding is ugly */
      }

      /////////////////////////////////////////////////////////////////////////////////
      #ifdef DIAGNOSTICS
      diagRxTifsBlueMoonWin++;
      #endif
      /////////////////////////////////////////////////////////////////////////////////
    }

    /* update the driver state */
    driverState = RX_STATE;
  }

  /* -----------------------------------------------------------------------------*
   *                            TIFS "Not Possible"
   * -----------------------------------------------------------------------------*/
  else
  {
    /////////////////////////////////////////////////////////////////////////////////
    #ifdef DIAGNOSTICS
    diagRxTifsFailNotPossible++;
    #endif
    /////////////////////////////////////////////////////////////////////////////////

    /* indicate that this TIFS operation is not possible */
    bbRxCallback(BB_STATUS_FAILED, 0, 0, 0, 0);
  }
}

/*===============================================================================================*/
/*
 *  \brief      Configure radio for receive.
 *
 *  \param      pRxBuf - pointer where receive bytes get written
 *  \param      len    - maximum length for receive, i.e. size of the allocated receive buffer
 *
 *  Configuration common to TIFS and non-TIFS transmit.
 *
 */
/*===============================================================================================*/
static void palBbRxHwRadioConfig(uint8_t * pRxBuf, uint16_t len)
{
  BB_ASSERT(len >= PDU_HEADER_LEN); /* buffer must at least accommodate the header */

#if defined(NRF52840_XXAA)
    if (bbTxPhy == BB_PHY_BLE_CODED)
    {
      /* Improve Coded sensitivity for nRF52840 chip. */
      *(volatile uint32_t *)0x4000173C |= 0x80000000;
      *(volatile uint32_t *)0x4000173C = ((*(volatile uint32_t *)0x4000173C & 0xFFFFFF00) | 0x5C);
    }

    uint32_t plen = ((bbRxPhy != BB_PHY_BLE_1M) ? ((bbRxPhy != BB_PHY_BLE_2M) ? RADIO_PCNF0_PLEN_LongRange : RADIO_PCNF0_PLEN_16bit) \
    : RADIO_PCNF0_PLEN_8bit) << RADIO_PCNF0_PLEN_Pos;
#elif defined(NRF52832_XXAA)
    uint32_t plen = ((bbRxPhy == BB_PHY_BLE_2M) ? RADIO_PCNF0_PLEN_16bit : RADIO_PCNF0_PLEN_8bit) << RADIO_PCNF0_PLEN_Pos;
#else
    uint32_t plen = 0;
#endif

  /* set the mode */
  palBbSetRadioMode(bbRxPhy, bbTxPhyOptions);

  if (!bbDecryptRxFlag)
  {
    /* No decryption */
    /* configure PCNF0 */
    NRF_RADIO->PCNF0 = (((uint32_t)(  1   /* S0 field in bytes      */ ))  << RADIO_PCNF0_S0LEN_Pos)                              |
                     (((uint32_t)(  8   /* length field in bits   */ ))  << RADIO_PCNF0_LFLEN_Pos)                              |
                     (((uint32_t)(  0   /* S1 field in bits       */ ))  << RADIO_PCNF0_S1LEN_Pos)                              |
#if defined(NRF52840_XXAA)
                     (((uint32_t)((bbRxPhy == BB_PHY_BLE_CODED) ? 3 : 0 /* TERM field in bits*/ ))  << RADIO_PCNF0_TERMLEN_Pos) |
                     (((uint32_t)((bbRxPhy == BB_PHY_BLE_CODED) ? 2 : 0 /* CI field in bits  */ ))  << RADIO_PCNF0_CILEN_Pos)   |
                     (((uint32_t)(  0   /* CRCINC field           */ ))  << RADIO_PCNF0_CRCINC_Pos)                             |
                     (((uint32_t)(  0   /* S1INCL filed           */ ))  << RADIO_PCNF0_S1INCL_Pos)                             |
#endif
                     ((uint32_t)( plen /* preamble field in bits */ ));

    /* configure PCNF1 */
    NRF_RADIO->PCNF1  = (((uint32_t)( len - PDU_HEADER_LEN        /* max len    */ ))  << RADIO_PCNF1_MAXLEN_Pos  )  |
                      (((uint32_t)( 0                           /* static len */ ))  << RADIO_PCNF1_STATLEN_Pos )  |
                      (((uint32_t)( NORDIC_BASE_ADDR_LEN                         ))  << RADIO_PCNF1_BALEN_Pos   )  |
                      (((uint32_t)( RADIO_PCNF1_ENDIAN_Little                    ))  << RADIO_PCNF1_ENDIAN_Pos  )  |
                      (((uint32_t)( bbRadioPcnf1WhiteEn                          ))  << RADIO_PCNF1_WHITEEN_Pos );

    /* set up memory location where received packet gets stored */
    NRF_RADIO->PACKETPTR = (uint32_t)pRxBuf;

  }
  else
  {
    /* Decryption */
    BB_ASSERT( keySetFlag ); /* encryption key has not been set */
    BB_ASSERT( ivSetFlag ); /* iv was not set */
    BB_ASSERT( rxPacketCountSetFlag ); /* packet count was not set */
    BB_ASSERT( directionSetFlag ); /* direction was not set */
    BB_ASSERT( encryptTypeSetFlag ); /* decrypt type was not set */

    /* configure PCNF0 */
    NRF_RADIO->PCNF0 = (((uint32_t)(  1   /* S0 field in bytes      */ ))  << RADIO_PCNF0_S0LEN_Pos)                              |
                       (((uint32_t)(  8   /* length field in bits   */ ))  << RADIO_PCNF0_LFLEN_Pos)                              |
                       (((uint32_t)(  0   /* S1 field in bits       */ ))  << RADIO_PCNF0_S1LEN_Pos)                              |
#if defined(NRF52840_XXAA)
                       (((uint32_t)((bbRxPhy == BB_PHY_BLE_CODED) ? 3 : 0 /* TERM field in bits*/ ))  << RADIO_PCNF0_TERMLEN_Pos) |
                       (((uint32_t)((bbRxPhy == BB_PHY_BLE_CODED) ? 2 : 0 /* CI field in bits  */ ))  << RADIO_PCNF0_CILEN_Pos)   |
                       (((uint32_t)(  0   /* CRCINC field           */ ))  << RADIO_PCNF0_CRCINC_Pos)                             |
                       (((uint32_t)(  1   /* S1INCL field           */ ))  << RADIO_PCNF0_S1INCL_Pos)                             |
#endif
                       ((uint32_t)( plen /* preamble field in bits */ ));

    /* configure PCNF1 */
    NRF_RADIO->PCNF1 = (((uint32_t)(  len - PDU_HEADER_LEN   /* maximum packet length  */ ))  << RADIO_PCNF1_MAXLEN_Pos  )  |
                       (((uint32_t)(  0   /* static packet length   */ ))  << RADIO_PCNF1_STATLEN_Pos )  |
                       (((uint32_t)( NORDIC_BASE_ADDR_LEN              ))  << RADIO_PCNF1_BALEN_Pos   )  |
                       (((uint32_t)( RADIO_PCNF1_ENDIAN_Little         ))  << RADIO_PCNF1_ENDIAN_Pos  )  |
                       (((uint32_t)( bbRadioPcnf1WhiteEn               ))  << RADIO_PCNF1_WHITEEN_Pos );

    /* set packet pointer to the encrypted buffer */
    NRF_RADIO->PACKETPTR = (uint32_t)&bbEncryptEncryptedBuf[0];

    /* update decryption direction for ACL/CIS */
    if ((bbEncryptType == PAL_BB_TYPE_ACL) || (bbEncryptType == PAL_BB_TYPE_CIS))
    {
      bbEncryptCcmData[24] = !bbEncryptDirection;
    }
    else
    {
      bbEncryptCcmData[24] = bbEncryptDirection;
    }

    /* set decrypt packet counter */
    bbEncryptCcmData[16] = (bbDecryptPacketCounter >>  0) & 0xFF;
    bbEncryptCcmData[17] = (bbDecryptPacketCounter >>  8) & 0xFF;
    bbEncryptCcmData[18] = (bbDecryptPacketCounter >> 16) & 0xFF;
    bbEncryptCcmData[19] = (bbDecryptPacketCounter >> 24) & 0xFF;
    bbEncryptCcmData[20] = (bbDecryptPacketCounter >> 32) & 0x7F; /* only 7-bits of MSB are used (packet count is 39 bits) */

    NRF_CCM->ENABLE = (CCM_ENABLE_ENABLE_Disabled << CCM_ENABLE_ENABLE_Pos);
    NRF_CCM->ENABLE = (CCM_ENABLE_ENABLE_Enabled  << CCM_ENABLE_ENABLE_Pos);

    NRF_CCM->CNFPTR     = (uint32_t)bbEncryptCcmData;
    NRF_CCM->SCRATCHPTR = (uint32_t)bbEncryptScratchBuf;

    NRF_CCM->INPTR  = (uint32_t)bbEncryptEncryptedBuf;
    NRF_CCM->OUTPTR = (uint32_t)pRxBuf;

    uint32_t ccmModeDatarate;
    switch (bbRxPhy)
    {
      default:
      case BB_PHY_BLE_1M:
        ccmModeDatarate = CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos;
        break;
      case BB_PHY_BLE_2M:
        ccmModeDatarate = CCM_MODE_DATARATE_2Mbit << CCM_MODE_DATARATE_Pos;
        break;
#if defined(NRF52840_XXAA)
      case BB_PHY_BLE_CODED:
        ccmModeDatarate = ((bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? CCM_MODE_DATARATE_500Kbps : CCM_MODE_DATARATE_125Kbps)
                          << CCM_MODE_DATARATE_Pos;
        break;
#endif
    }

    NRF_CCM->MODE = (CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos) |
                    (ccmModeDatarate) |
                    (CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos);

    NRF_CCM->EVENTS_ENDCRYPT = 0;
    NRF_CCM->EVENTS_ENDKSGEN = 0;
    NRF_CCM->EVENTS_ERROR    = 0;

    /* disable shortcut between ENDKSGEN and CRYPT for on-the-fly decryption */
    NRF_CCM->SHORTS = 0;

    /* enable PPI between RADIO->EVENTS_ADDRESS and CCM->TASKS_CRYPT*/
    NRF_PPI->CHENSET = PPI_CHENSET_CH25_Msk;

    /* start key stream generation */
    NRF_CCM->TASKS_KSGEN = 1;
  }

  /* remember where the receive buffer is located, used later when processing the completed receive */
  bbpRxBuf = pRxBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      TIMER0 interrupt handler "RX Timeout"
 *
 *  \param      None.
 *
 *  This is the "RX Timeout" interrupt. No other functionality is shared on the TIMER0 interrupt.
 *  If the receive operation has not gotten started, it is cancelled.
 *
 */
/*************************************************************************************************/
void BbBleDrvTimerIRQHandler(void)
{
  /* -----------------------------------------------------------------------------*
   *                         COMPARE1 - "RX Timeout"
   * -----------------------------------------------------------------------------*/

  if (PAL_BB_TIMER->INTENSET & TIMER_INTENSET_COMPARE1_Msk)
  {
    BB_ASSERT(PAL_BB_TIMER->EVENTS_COMPARE[1]);
    BB_ASSERT(driverState == RX_STATE); /* an RX timeout should only occur for an RX */
    BB_ASSERT(NRF_RADIO->STATE == RADIO_STATE_STATE_Rx); /* should only be timeout if receiver is actually on */

    /* disable this interrupt */
    PAL_BB_TIMER->INTENCLR = TIMER_INTENCLR_COMPARE1_Msk;

    /*   Once the ADDRESS event has happened, the receive will not be timed out. This event happens
     *   just after the incoming preamble/address has been recognized.
     */
    if (NRF_RADIO->EVENTS_ADDRESS == 0)
    {
      /* stop the radio */
      palBbRadioHardStop();

      /* send notification of timeout */
      bbRxCallback(BB_STATUS_RX_TIMEOUT, 0, 0, 0, 0);
    }
  }

  /* -----------------------------------------------------------------------------*
   *                         COMPARE5 - "Scheduler"
   * -----------------------------------------------------------------------------*/

  if (PAL_BB_TIMER->INTENSET & PAL_BB_TIMER_EXPIRY_INTENSET_MASK)
  {
    PalTimerStop();

    /* TODO move to private PAL header. */
    extern void (*palTimerExpCback)(void);
    if (palTimerExpCback)
    {
      palTimerExpCback();
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Radio interrupt handler.
 *
 *  \param      None.
 *
 *  This the radio interrupt service routine.  It is at the heart of the baseband driver
 *  design. It handles the following interrupts:
 *
 *  1) READY or "TIFS Expired" Interrupt - This interrupt fires as soon as the radio
 *     ramps up and is 'ready'. It is used as timeout for any TIFS operation that may
 *     be coming up. Also, it enables the RX Timeout interrupt if a receive operation.
 *
 *  2) PAYLOAD or "Timestamp/TIFS Setup" Interrupt - This interrupt fires once the payload
 *     has completed, for either transmit or receive.  During this interrupt, the timestamp
 *     is collected and computed. The next potential TIFS operation is also set up.
 *
 *  3) DISABLED or "TX-RX Complete" Interrupt - This interrupt occurs at the completion of
 *     a receive or transmit. It runs the appropriate callback with the appropriate status.
 *     (NOTE: This interrupt is intentionally *not* the END interrupt, for bulletproof timing.)
 *
 */
/*************************************************************************************************/
void BbBleDrvRadioIRQHandler(void)
{
  /* -----------------------------------------------------------------------------*
   *                       READY  -  "TIFS Expired"
   * -----------------------------------------------------------------------------*/
  if ((NRF_RADIO->INTENSET & RADIO_INTENSET_READY_Msk) && NRF_RADIO->EVENTS_READY)
  {
    /* clear the event, leave the interrupt active */
    NRF_RADIO->EVENTS_READY = 0;
    WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_READY);

    /* disable the PPI channel of TX/RX operations */
    NRF_PPI->CHENCLR = PPI_CHENCLR_CH14_Msk;  /* COMPARE[0] -> TXEN/RXEN */

    /* see if a TIFS operation is ramping up, and if so, see if it failed */
    if ( ((tifsState == TIFS_TX_RAMPUP) && (NRF_RADIO->STATE == RADIO_STATE_STATE_TxIdle)) ||
         ((tifsState == TIFS_RX_RAMPUP) && (NRF_RADIO->STATE == RADIO_STATE_STATE_RxIdle)))
    {
      palBbRadioHardStop();  /* note: tifs state is reset in this function */
      return;
    }

    if ((driverState == TX_STATE) || (tifsState == TIFS_TX_RAMPUP))
    {
      BB_LED_TX_ON();
      BB_ASSERT(NRF_RADIO->STATE == RADIO_STATE_STATE_Tx);
    }

    /* if this is a receive, activate the pre-configured RX timeout */
    if ((driverState == RX_STATE) || (tifsState == TIFS_RX_RAMPUP))
    {
      BB_ASSERT(NRF_RADIO->STATE == RADIO_STATE_STATE_Rx);
      BB_LED_RX_ON();

      /* clear the ADDRESS event, it will indicate if an RX timeout is possible */
      NRF_RADIO->EVENTS_ADDRESS = 0;
      WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_ADDRESS);

      /* enable the RX interrupt */
      PAL_BB_TIMER->INTENSET = TIMER_INTENSET_COMPARE1_Msk;

      /*  if the compare event is set, the RX timeout already occurred */
      if (PAL_BB_TIMER->EVENTS_COMPARE[1])
      {
        /* force a compare event, which will trigger the interrupt (it was disabled when the actual event happened) */
        PAL_BB_TIMER->EVENTS_COMPARE[1] = 1;
      }
    }

    /* update tifs state */
    tifsState = TIFS_NULL;
  }

  /* -----------------------------------------------------------------------------*
   *                      PAYLOAD  -  "Timestamp/TIFS Setup"
   * -----------------------------------------------------------------------------*/
  else if ((NRF_RADIO->INTENSET & RADIO_INTENSET_PAYLOAD_Msk) && NRF_RADIO->EVENTS_PAYLOAD)
  {
    /* clear the event, leave the interrupt active */
    NRF_RADIO->EVENTS_PAYLOAD = 0;
    WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_PAYLOAD);

    /* disable the "RX Timeout" interrupt (timer COMPARE1 event) */
    PAL_BB_TIMER->INTENCLR = TIMER_INTENCLR_COMPARE1_Msk;
    NVIC_ClearPendingIRQ(PAL_BB_IRQn); /* necessary, if interrupt already "fired" this is the only way to clear it */

    /*
     *  Setup for potential TIFS operation.
     *
     *  Every transmit and every receive has a potential follow-up TIFS operation.
     *  To accommodate this, the radio must start ramping up before it knows
     *  if the operation is going to happen or not.
     *
     */
    BB_ASSERT((NRF_PPI->CHENSET & PPI_CHENSET_CH14_Msk) == 0); /* channel should be disabled by now */

    /* clear radio shortcuts except for END->DISABLE (must not have ENABLE->START shortcut active) */
    NRF_RADIO->SHORTS = RADIO_SHORTS_END_DISABLE_Msk;

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *            TX, set up for TIFS-RX
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    if ((bbOpParam.ifsMode == PAL_BB_IFS_MODE_TOGGLE_TIFS) &&
        (driverState == TX_STATE))
    {
      uint32_t antennaTimeTxPktEnd;
      uint32_t crcTime;
      uint32_t term2Time;
      uint32_t preambleTime;
      uint32_t accessAddrTime;
      int32_t correction;

      /* calculate time to transmit CRC and TERM2 (Long Range Only) */
      switch (bbTxPhy)
      {
        case BB_PHY_BLE_1M:
          crcTime = LL_BLE_US_PER_BYTE_1M * LL_CRC_LEN;
          term2Time = 0;
          break;
        case BB_PHY_BLE_2M:
          crcTime = LL_BLE_US_PER_BYTE_2M * LL_CRC_LEN;
          term2Time = 0;
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          crcTime = ((bbTxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? LL_BLE_US_PER_BYTE_CODED_S2 : LL_BLE_US_PER_BYTE_CODED_S8) * LL_CRC_LEN;
          term2Time = ((bbTxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? LL_BLE_US_PER_BIT_CODED_S2 : LL_BLE_US_PER_BIT_CODED_S8) * LL_TERM2_LEN_BITS;
          break;
#endif
        default:
          crcTime = LL_BLE_US_PER_BYTE_1M * LL_CRC_LEN;
          term2Time = 0;
      }

      /* calculate time to receive preamble and access address */
      switch(bbRxPhy)
      {
        case BB_PHY_BLE_1M:
          preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
          accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
          correction = 4;
          break;
        case BB_PHY_BLE_2M:
          preambleTime = LL_BLE_US_PER_BYTE_2M * LL_PREAMBLE_LEN_2M;
          accessAddrTime = LL_BLE_US_PER_BYTE_2M * LL_AA_LEN;
          correction = 20;
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          preambleTime = LL_BLE_US_PER_BIT_CODED_S8 * LL_PREAMBLE_LEN_CODED_BITS;
          accessAddrTime = LL_BLE_US_PER_BYTE_CODED_S8 * LL_AA_LEN;
          correction = 110;
          break;
#endif
        default:
          preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
          accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
          correction = 4;
      }

      /*
       *   Calculate "antenna time" when the packet ended.
       *
       *   The timer was captured into CC[2] triggered by the PAYLOAD event.
       *   The time for the CRC is added to get to the end of the packet.
       *   The transmit propagation delay from event to antenna is added to get the actual "antenna time" when the
       *   last transmitted bit finishes at the antenna.
       */
      antennaTimeTxPktEnd =  PAL_BB_TIMER->CC[2]                                        /* start with timer value when payload just finished */
                              + TICKS_PER_USEC * crcTime                              /* add CRC time to arrive at end of packet */
                              + TICKS_PER_USEC * term2Time                            /* add TERM2 time to arrive at end of packet */
                              + TICKS_PER_USEC * NRF5x_PROP_DELAY_TX_USECS;           /* add propagation delay to get "antenna time" */

      /* set timer trigger for upcoming TIFS receive operation */
      PAL_BB_TIMER->CC[0]   =  antennaTimeTxPktEnd                                      /* start with "antenna time" at end of packet just transmitted */
                              + TICKS_PER_USEC * LL_BLE_TIFS_US                       /* add the TIFS period */
                              - TICKS_PER_USEC * NRF5x_tRXEN_BLE_USECS                /* subtract time it takes to ramp-up for receive */
                              - TICKS_PER_USEC * MAX_TIFS_DEVIATION_USECS;            /* subtract allowed deviation */

      /* configure and enable PPI trigger for RXEN, happens on next timer COMPARE[0] just configured above */
      NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_RXEN;                        /* configure task */
      NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                                        /* enable channel */

      /* calculate and set RX timeout */
      PAL_BB_TIMER->CC[1] = antennaTimeTxPktEnd                                                             /* start with "antenna time" at end of packet just transmitted */
                            + TICKS_PER_USEC * LL_BLE_TIFS_US                                             /* add the TIFS period */
                            + TICKS_PER_USEC * preambleTime                                               /* allow time preamble to be received */
                            + TICKS_PER_USEC * accessAddrTime                                             /* allow time access address to be received */
                            + TICKS_PER_USEC * (2 * MAX_TIFS_DEVIATION_USECS)                             /* add allowed tolerance  +2/-2 usecs */
                            + TICKS_PER_USEC * correction;                                                         /* empirically derived trim factor (NOTE!! units are ticks, scaler change affects this) */
      /*
       *  NOTE:  In the case of an RX timeout, the above compare will trigger an interrupt.
       *         This interrupt requires time to execute. This delay extends the time the radio
       *         is active. The effective timeout period is longer than computed above.
       *
       *         This effective timeout period can be reduced using the trim value.
       *         This would be useful for power savings. But be warned!! Great care
       *         is required as this involves timing the speed of code execution.
       *         Any adjustment must be made with debug code disabled.
       */

      /* clear RX timeout compare event, but do not enable interrupt, that happens in radio isr */
      PAL_BB_TIMER->EVENTS_COMPARE[1] = 0;
      WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[1]);

      /* a TX is completing, the radio will soon be ramping up for possible TIFS RX */
      tifsState = TIFS_RX_RAMPUP;
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *              RX, set up for TIFS-TX
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    else if ((bbOpParam.ifsMode == PAL_BB_IFS_MODE_TOGGLE_TIFS) &&
             (driverState == RX_STATE))
    {
      uint32_t antennaTimeRxPktEnd;
      uint32_t crcTime;
      int32_t  rxCorrection;
      int32_t  txCorrection;
      uint32_t term2Time;

      BB_ASSERT(driverState == RX_STATE); /* illegal driver state */
#if defined(NRF52840_XXAA)
      if (NRF_RADIO->EVENTS_RATEBOOST == 1)
      {
        /* If rate boost events is detected, then  will use Coded S2, otherwise will use Coded S8 by default */
        bbRxPhyOptions = BB_PHY_OPTIONS_BLE_S2;
        NRF_RADIO->EVENTS_RATEBOOST = 0;
      }
      else
      {
        bbRxPhyOptions = BB_PHY_OPTIONS_BLE_S8;
      }
#endif
      /* calculate time to transmit CRC and TERM2(Long Range Only) */
      switch (bbRxPhy)
      {
        case BB_PHY_BLE_1M:
          crcTime = LL_BLE_US_PER_BYTE_1M * LL_CRC_LEN;
          term2Time = 0;
          rxCorrection = -2;
          break;
        case BB_PHY_BLE_2M:
          crcTime = LL_BLE_US_PER_BYTE_2M * LL_CRC_LEN;
          term2Time = 0;
          rxCorrection = 2;
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          crcTime = ((bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? LL_BLE_US_PER_BYTE_CODED_S2 : LL_BLE_US_PER_BYTE_CODED_S8) * LL_CRC_LEN;
          term2Time = ((bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? LL_BLE_US_PER_BIT_CODED_S2 : LL_BLE_US_PER_BIT_CODED_S8) * LL_TERM2_LEN_BITS;
          rxCorrection = (bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? -49 : -180;
          break;
#endif
        default:
          crcTime = LL_BLE_US_PER_BYTE_1M * LL_CRC_LEN;
          term2Time = 0;
          rxCorrection = -2;
      }

      switch (bbTxPhy)
      {
        case BB_PHY_BLE_1M:
          txCorrection = 0;
          break;
        case BB_PHY_BLE_2M:
          txCorrection = +1;
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          txCorrection = -1;
          break;
#endif
        default:
          txCorrection = 0;
      }

      /* calculate "antenna time" when the packet ended (see comments in above transmit code for more explanation) */
      antennaTimeRxPktEnd =  PAL_BB_TIMER->CC[2]                                        /* start with timer value when payload just finished */
                              + TICKS_PER_USEC * crcTime                              /* add CRC time to arrive at end of packet */
                              + TICKS_PER_USEC * term2Time                            /* add TERM2 time to arrive at end of packet */
                              - TICKS_PER_USEC * NRF5x_PROP_DELAY_RX_USECS;           /* subtract propagation delay to get "antenna time" */

      /* set timer trigger for upcoming TIFS transmit operation */
      PAL_BB_TIMER->CC[0] = antennaTimeRxPktEnd                                         /* start with antenna time */
                            + TICKS_PER_USEC * LL_BLE_TIFS_US                         /* add the TIFS period */
                            - TICKS_PER_USEC * NRF5x_tTXEN_BLE_USECS                  /* subtract ramp-up time */
                            - TICKS_PER_USEC * NRF5x_PROP_DELAY_TX_USECS              /* subtract internal propagation delay */
                            + TICKS_PER_USEC * rxCorrection                             /* empirically derived trim factor (NOTE!! units are ticks, scaler change affects this) */
                            + TICKS_PER_USEC * txCorrection                             /* empirically derived trim factor (NOTE!! units are ticks, scaler change affects this) */
                            + bbTxTifsAdj;                                            /* adjustment specified via API function call */

      /* configure and enable PPI trigger for TXEN, happens on next timer COMPARE[0] */
      NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_TXEN;  /* configure task */
      NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                  /* enable channel */

      /* an RX is completing, the radio will soon be ramping up for possible TIFS TX */
      tifsState = TIFS_TX_RAMPUP;
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *              RX, set up for TIFS-RX
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    else if ((bbOpParam.ifsMode == PAL_BB_IFS_MODE_SAME_ABS) &&
             (driverState == RX_STATE))
    {
      uint32_t preambleTime;
      uint32_t accessAddrTime;
      int32_t correction;

      /* calculate time to receive preamble and access address */
      switch(bbRxPhy)
      {
        case BB_PHY_BLE_1M:
          preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
          accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
          correction = 4;
          break;
        case BB_PHY_BLE_2M:
          preambleTime = LL_BLE_US_PER_BYTE_2M * LL_PREAMBLE_LEN_2M;
          accessAddrTime = LL_BLE_US_PER_BYTE_2M * LL_AA_LEN;
          correction = 20;
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          preambleTime = LL_BLE_US_PER_BIT_CODED_S8 * LL_PREAMBLE_LEN_CODED_BITS;
          accessAddrTime = LL_BLE_US_PER_BYTE_CODED_S8 * LL_AA_LEN;
          correction = 110;
          break;
#endif
        default:
          preambleTime = LL_BLE_US_PER_BYTE_1M * LL_PREAMBLE_LEN_1M;
          accessAddrTime = LL_BLE_US_PER_BYTE_1M * LL_AA_LEN;
          correction = 4;
      }

      PAL_BB_TIMER->CC[0] = BB_US_TO_BB_TICKS(bbOpParam.ifsTime - (NRF5x_tRXEN_BLE_USECS + NRF5x_PROP_DELAY_RX_USECS));

      /* configure and enable PPI trigger for RXEN, happens on next timer COMPARE[0] just configured above */
      NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_RXEN;                        /* configure task */
      NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                                        /* enable channel */

      /* set timer compare CC[1] for RX timeout (allow time to recognize address, and account for propagation delay) */
      PAL_BB_TIMER->CC[1]  = PAL_BB_TIMER->CC[0]
                           + TICKS_PER_USEC * (30       /* TODO Resolve total deviation allowed */
                                               + preambleTime
                                               + accessAddrTime
                                               + NRF5x_PROP_DELAY_RX_USECS
                                               + NRF5x_tRXEN_BLE_USECS
                                               + correction);

      /*
       *  NOTE:  In the case of an RX timeout, the above compare will trigger an interrupt.
       *         This interrupt requires time to execute. This delay extends the time the radio
       *         is active. The effective timeout period is longer than computed above.
       *
       *         This effective timeout period can be reduced using the trim value.
       *         This would be useful for power savings. But be warned!! Great care
       *         is required as this involves timing the speed of code execution.
       *         Any adjustment must be made with debug code disabled.
       */

      /* clear RX timeout compare event, but do not enable interrupt, that happens in radio isr */
      PAL_BB_TIMER->EVENTS_COMPARE[1] = 0;
      WAIT_FOR_WR_BUF_EMPTY(PAL_BB_TIMER->EVENTS_COMPARE[1]);

      /* update channel parameters */
      if (bbOpParam.pIfsChan)
      {
        palBbBleSetChannelParam(bbOpParam.pIfsChan);
      }

      /* a TX is completing, the radio will soon be ramping up for possible TIFS RX */
      tifsState = TIFS_RX_RAMPUP;
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *            TX, set up for TIFS-TX
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    else if ((bbOpParam.ifsMode == PAL_BB_IFS_MODE_SAME_ABS) &&
             (driverState == TX_STATE))
    {
      /* calculate start time */
      PAL_BB_TIMER->CC[0] = BB_US_TO_BB_TICKS(bbOpParam.ifsTime - (NRF5x_tTXEN_BLE_USECS + NRF5x_PROP_DELAY_TX_USECS));

      /* configure and enable PPI trigger for TXEN, happens on next timer COMPARE[0] */
      NRF_PPI->CH[14].TEP = (uint32_t) &NRF_RADIO->TASKS_TXEN;  /* configure task */
      NRF_PPI->CHENSET = PPI_CHENSET_CH14_Msk;                  /* enable channel */

      /* update channel parameters */
      if (bbOpParam.pIfsChan)
      {
        palBbBleSetChannelParam(bbOpParam.pIfsChan);
      }

      /* a TX is completing, the radio will soon be ramping up for possible TIFS RX */
      tifsState = TIFS_TX_RAMPUP;
    }

    /* ------------------------------------------------------------------------------
     *  Check to see if TIFS window already missed. This would happen if
     *  interrupts were suppressed for a long time.
     *
     *  NOTE! If the timing for the CC[0] compare is *too early*, it may actually
     *        fire while the radio is still active. If this happens, the operation
     *        that was supposed to trigger is ignored. This will cause big problems
     *        as it cannot be detected. The software assumes the operation is going
     *        forward when it is not. However, this is guarded against when the compare
     *        is set, hence, any problem with this is an internal error.
     */

    /* capture current time */
    PAL_BB_TIMER->TASKS_CAPTURE[3] = 1;

    /* see if current time is already past the TX/RX trigger point */
    if ((PAL_BB_TIMER->CC[0] - PAL_BB_TIMER->CC[3]) & 0x80000000)
    {
      /* disable the PPI channel... which was just enabled */
      NRF_PPI->CHENCLR = PPI_CHENCLR_CH14_Msk;  /* COMPARE[0] -> TXEN/RXEN */

      /* override the TIFS state... which was just set */
      tifsState = TIFS_NULL;

      /*
       *  NOTE: If the trigger point were *just* reached, and the RX/TX were actually triggered...
       *        this is OK, the interrupt that fires will be redundant but do no harm.
       */
    }
  }

  /* -----------------------------------------------------------------------------*
   *                       DISABLED  -  "TX-RX Complete"
   * -----------------------------------------------------------------------------*/
  else if ((NRF_RADIO->INTENSET & RADIO_INTENSET_END_Msk) && NRF_RADIO->EVENTS_END)
  {
    /* clear the event, leave the interrupt active */
    NRF_RADIO->EVENTS_END = 0;
    WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_END);

    /* clear diagnostics pins */
    BB_LED_TX_OFF();
    BB_LED_RX_OFF();

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *                     TX Complete
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    if (driverState == TX_STATE)
    {
      /* update driver state, *before* callback */
      driverState = IDLE_STATE;
      BB_LED_OFF();

      /* Workaround for TX idle failure in Coded PHY */
      /* TODO: add max attempts */
      while (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled)
      {
        /* Workaround for TX idle failure in Coded PHY */
        NRF_RADIO->TASKS_RXEN = 1;

        /* disable the radio, start early at the top here */
        NRF_RADIO->TASKS_DISABLE = 1;
      }

      /* run callback function */
      palBbRestoreTrl();
      bbTxCallback(BB_STATUS_SUCCESS);
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *                     RX Complete
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    else if (driverState == RX_STATE)
    {
      uint32_t timestamp;
      uint32_t timestampUsesc;
      int8_t   rssi;

#if defined(NRF52840_XXAA)
      int32_t correction;
#endif

      /* get the PDU payload length from received packet, used later to compute start timestamp */
      uint8_t pduLen = bbpRxBuf[BB_DATA_PDU_LEN_OFFSET];

      if (bbDecryptRxFlag)
      {
        /* disable PPI between RADIO->EVENTS_ADDRESS and CCM->TASKS_CRYPT */
        NRF_PPI->CHENCLR = PPI_CHENSET_CH25_Msk;

        if (pduLen)
        {
          /* shift Rx data to remove NRF CCM RFU byte */
          memcpy(&bbpRxBuf[ENC_RFU_OFFSET], &bbpRxBuf[ENC_RFU_OFFSET+ENC_RFU_LEN], pduLen);
        }
      }

      /*
       *  Calculate the 'start' timestamp.  It is "antenna time" of the first packet bit.
       *
       *  The timestamp is derived from the a previous timer capture (held in CC[4]) that occurred at ADDRESS event.
       *  It calculated by subtracting back from preamble and AA to the packet start.
       *
       *  The PDU payload length was retrieved from the incoming buffer of the last receive packet.
       */
      timestamp = PAL_BB_TIMER->CC[4] - (TICKS_PER_USEC * NRF5x_PROP_DELAY_RX_USECS);
      switch(bbRxPhy)
      {
        case BB_PHY_BLE_1M:
          timestamp -= (TICKS_PER_USEC * LL_BLE_US_PER_BYTE_1M * ( LL_PREAMBLE_LEN_1M +   /* preamble */
                                                                   LL_AA_LEN));           /* address */

#if defined(NRF52840_XXAA)
          timestamp -= (2 * TICKS_PER_USEC);   /* Adjustment based on the sniffer trace observation. */
#endif
          break;
        case BB_PHY_BLE_2M:
          timestamp -= (TICKS_PER_USEC * LL_BLE_US_PER_BYTE_2M * ( LL_PREAMBLE_LEN_2M +   /* preamble */
                                                                   LL_AA_LEN));           /* address */

#if defined(NRF52840_XXAA)
          timestamp += (3 * TICKS_PER_USEC);   /* Adjustment based on the sniffer trace observation. */
#endif
          break;
#if defined(NRF52840_XXAA)
        case BB_PHY_BLE_CODED:
          correction = (bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? 5 : 40;
          timestamp -= (TICKS_PER_USEC * LL_BLE_US_PER_BIT_CODED_S8 * ( LL_PREAMBLE_LEN_CODED_BITS +   /* preamble */
                                                                        LL_AA_LEN * 8 ));              /* address */
          timestamp -= TICKS_PER_USEC * correction;                                                    /* Empirical value */

          /* Adjustment based on the sniffer trace observation. */
          timestamp += ((bbRxPhyOptions == BB_PHY_OPTIONS_BLE_S2) ? -29 : -20) * TICKS_PER_USEC;
          break;
#endif
        default:
          timestamp -= (TICKS_PER_USEC * LL_BLE_US_PER_BYTE_1M * ( LL_PREAMBLE_LEN_1M +   /* preamble */
                                                                     LL_AA_LEN));         /* address */
      }

#if (USE_RTC_BB_CLK)
      /* RTC event start time plus timestamp from HFCLK timer.  */
      timestampUsesc = BB_TICKS_TO_US(bbEventStartTime)  + bbDueOffsetUsec + TICKS_TO_USEC(timestamp);
      /* Handle wraparound. */
      if (timestampUsesc > BB_RTC_MAX_VALUE_US)
      {
        timestampUsesc -= BB_RTC_MAX_VALUE_US;
      }
#else
      timestampUsesc = BB_TICKS_TO_US(timestamp);
#endif

      /* compute RSSI value at antenna end */
      rssi = (int8_t)(-(NRF_RADIO->RSSISAMPLE & 0x7F)) - palRadioGetTxRfPathComp();
      /* update driver state, *before* callback */
      driverState = IDLE_STATE;
      BB_LED_OFF();

      /* Follow Rx PHY options */
      bbTxPhyOptions = bbRxPhyOptions;

      /* Overwrite with TIFS preference */
      if (tifsTxPhyOptions)
      {
        bbTxPhyOptions = tifsTxPhyOptions;
      }

      /* see if the CRC failed */
      if ((NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) == RADIO_CRCSTATUS_CRCSTATUS_CRCError)
      {
        /* CRC error - run the callback function */
        bbRxCallback(BB_STATUS_CRC_FAILED, rssi, NRF_RADIO->RXCRC, timestampUsesc, bbRxPhyOptions);
      }
      else if ((bbDecryptRxFlag) && (pduLen) &&
              ((NRF_CCM->MICSTATUS & CCM_MICSTATUS_MICSTATUS_Msk) == CCM_MICSTATUS_MICSTATUS_CheckFailed))
      {
        /* MIC error - run the callback function */
        bbRxCallback(BB_STATUS_MIC_FAILED, rssi, NRF_RADIO->RXCRC, timestampUsesc, bbRxPhyOptions);
      }
      else
      {
        /* Success! - run the callback function */
        bbRxCallback(BB_STATUS_SUCCESS, rssi, NRF_RADIO->RXCRC, timestampUsesc, bbRxPhyOptions);
      }
    }

    /* - - - - - - - - - - - - - - - - - - - - - - - - - *
     *                 Illegal State
     * - - - - - - - - - - - - - - - - - - - - - - - - - */
    else
    {
      BB_ASSERT(0);  /* most likely IDLE state, if not it is completely unknown */

      /*
       *  Ah ah. You tried this, NRF_RADIO->TASKS_DISABLE = 1, didn't you?
       *  Not allowed! Only palBbRadioHardStop() is permitted to do that.
       */
    }
  }

  /* -----------------------------------------------------------------------------*
   *                               Unexpected Interrupt
   * -----------------------------------------------------------------------------*/
  else
  {
    BB_ASSERT(0);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel TIFS timer.
 *
 *  This stops any active TIFS timer operation. This routine is always called in the callback
 *  (i.e. ISR) context.
 */
/*************************************************************************************************/
void PalBbBleCancelTifs(void)
{
  BB_ASSERT(driverState == IDLE_STATE);  /* driver must be idle */

  /* hard stop will kill any TIFS ramping up */
  palBbRadioHardStop();
  palBbRestoreTrl();
}

/*************************************************************************************************/
/*!
 *  \brief      Cancel a pending transmit or receive.
 *
 *  This stops any active radio operation. This routine is never called in the callback
 *  (i.e. ISR) context.
 */
/*************************************************************************************************/
void PalBbBleCancelData(void)
{
  BB_ASSERT(driverState != NULL_STATE);  /* driver must be initialized */
  BB_ASSERT(driverState != SLEEP_STATE); /* driver cannot be asleep */

  /* cancel RX timeout if active */
  PAL_BB_TIMER->INTENCLR = TIMER_INTENCLR_COMPARE1_Msk;  /* disable interrupt */
  NVIC_ClearPendingIRQ(PAL_BB_IRQn); /* just in case we are running in interrupt context above TIMER0 */

  /* stop radio */
  palBbRadioHardStop();
  palBbRestoreTrl();

  ///////////////////////////////////////////////////////////////////////////////
  #ifdef DIAGNOSTICS
  diagCancels++;

  /* update diagnostics */
  if (diagOp != DIAG_NULL)
  {
    if ((diagOp == DIAG_TX) || (diagOp == DIAG_TX_TIFS))
    {
      diagTxTotalCalls--;
      if (diagOp == DIAG_TX) { diagTxCalls--; }
      else                   { diagTxTifsCalls--; }
    }
    else if ((diagOp == DIAG_RX) || (diagOp == DIAG_RX_TIFS))
    {
      diagRxTotalCalls--;
      if (diagOp == DIAG_RX) { diagRxCalls--; }
      else                   { diagRxTifsCalls--; }
    }
    else
    {
      BB_ASSERT(0); /* unknown value for diagOp */
    }

    /* finally, set to NULL */
    diagOp = DIAG_NULL;
  }
  #endif
  ///////////////////////////////////////////////////////////////////////////////
}

/*===============================================================================================*/
/*!
 *  \brief      Hard stop of the radio.
 *
 *  \param      None.
 *
 *  Immediately stops the radio. All radio interrupts are cancelled. The radio is put into
 *  the disabled state. This function does not return until the disabled state is verified.
 *
 */
/*===============================================================================================*/
static void palBbRadioHardStop(void)
{
  /* hard stop of all radio interrupts */
  NVIC_DisableIRQ(RADIO_IRQn);

  /* Workaround for TX idle failure in Coded PHY */
  NRF_RADIO->TASKS_RXEN = 1;

  /* disable the radio, start early at the top here */
  NRF_RADIO->TASKS_DISABLE = 1;

  /* disable the PPI channel of TX/RX operations */
  NRF_PPI->CHENCLR = PPI_CHENCLR_CH14_Msk;  /* COMPARE[0] -> TXEN/RXEN */

  /* disable HFCLK */
  PalBbBleLowPower();

  /* update TIFS state */
  tifsState = TIFS_NULL;

  /* update driver state */
  driverState = IDLE_STATE;
  BB_LED_OFF();

  /* clear diagnostics pins */
  BB_LED_TX_OFF();
  BB_LED_RX_OFF();

  /* wait for radio to complete shutdown */
  ///////////////////////////////////////////////////////////////////////////////
  #if BB_ENABLE_ASSERT == TRUE
  uint8_t busycount = 0;
  BB_ASSERT(busycount < 10); /* radio got stuck */
  busycount++;
  #endif
  ///////////////////////////////////////////////////////////////////////////////

#if defined(NRF52840_XXAA)
  /* Restore normal operation for non-Coded PHY. */
  *(volatile uint32_t *)0x4000173C &= ~0x80000000;
#endif

  if (bbDecryptRxFlag)
  {
    /* disable PPI between RADIO->EVENTS_ADDRESS and CCM->TASKS_CRYPT */
    NRF_PPI->CHENCLR = PPI_CHENSET_CH25_Msk;
  }

  /* clear all events */
  NRF_RADIO->EVENTS_READY    = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_READY);
  NRF_RADIO->EVENTS_PAYLOAD  = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_PAYLOAD);
  NRF_RADIO->EVENTS_END = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_END);
  NRF_RADIO->EVENTS_DISABLED = 0;
  WAIT_FOR_WR_BUF_EMPTY(NRF_RADIO->EVENTS_DISABLED);
  /* NOTE: The event ADDRESS is not cleared. There is no associated interrupt.
           It is cleared and set as part of the RX timeout logic. */

  /* stop any encryption that is under way, disable its interrupt */
  NRF_CCM->TASKS_STOP = 1 ;
  NRF_CCM->INTENCLR = CCM_INTENCLR_ENDKSGEN_Msk;

  /* clear any pending radio interrupts */
  NVIC_ClearPendingIRQ(RADIO_IRQn);

  /* re-enable radio interrupts */
  NVIC_EnableIRQ(RADIO_IRQn);
}

/*************************************************************************************************/
/*!
 *  \brief      Get transmit data from transmit descriptor.
 *
 *  \param      descs       Array of transmit buffer descriptor.
 *  \param      cnt         Number of descriptors.
 *
 *  \return     Pointer to transmit data.
 */
/*************************************************************************************************/
static uint8_t *palBbGetTxData(PalBbBleTxBufDesc_t descs[], uint8_t cnt)
{
  uint8_t *pTxBuf;

  if (cnt == 1)
  {
    /* The entire packet is in the 0th buffer. */
    pTxBuf = descs[0].pBuf;
  }
  else
  {
    if (descs[0].len == LL_DATA_HDR_LEN)
    {
      /* Assume Data PDU */

      /* The packet must be gathered into the 1st buffer. */
      pTxBuf = descs[1].pBuf - descs[0].len;

      /* Restore header.  This overwrites data previously transmitted, so it does not need to be preserved. */
      memcpy(pTxBuf, descs[0].pBuf, descs[0].len);

      if (cnt > 2)
      {
        /* The packet has a trailer (i.e., MIC). */
        uint8_t *pTrl = descs[1].pBuf + descs[1].len;
        uint8_t trlLen = descs[2].len;

        /* Preserve overwritten data.  This could be skipped for the last fragment. */
        memcpy(bbTrlSave, pTrl, trlLen);
        bbTrlSavedLen = trlLen;
        bbTrlSavedPtr = pTrl;

        /* Restore trailer. */
        memcpy(pTrl, descs[2].pBuf, trlLen);
      }
    }
    else
    {
      static uint8_t contTxBuf[LL_EXT_ADVB_MAX_LEN];

      /* Assume Extended advertising PDU */
      BB_ASSERT(descs[0].len >= (LL_ADV_HDR_LEN + LL_EXT_ADV_HDR_MIN_LEN));
      BB_ASSERT((descs[0].len + descs[1].len) <= sizeof(contTxBuf));

      pTxBuf = contTxBuf;
      memcpy(contTxBuf, descs[0].pBuf, descs[0].len);
      memcpy(contTxBuf + descs[0].len, descs[1].pBuf, descs[1].len);
    }
  }

  return pTxBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      Restore trailer data.
 *
 */
/*************************************************************************************************/
static void palBbRestoreTrl(void)
{
  if (bbTrlSavedPtr != NULL)
  {
    memcpy(bbTrlSavedPtr, bbTrlSave, bbTrlSavedLen);
    bbTrlSavedPtr = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
#ifdef DIAGNOSTICS
/*===============================================================================================*/
/*!
 *  \brief      Diagnostic function to record callback status.
 *
 *  \param      status      status as reported via callback
 *
 *  For diagnostics, this function is used to intercept passing control to the configured
 *  callback function. It is used to record diagnostic information.
 *
 */
/*===============================================================================================*/
static void DiagFauxTxCallback(uint8_t status)
{
  BB_ASSERT(diagOp != DIAG_NULL); /* more than one callback for the operation? */
  BB_ASSERT((diagOp == DIAG_TX) || (diagOp == DIAG_TX_TIFS)); /* apparent criss-cross of tx/rx, typo somewhere? */

  /* keep account of each operation-result */
  if (diagOp == DIAG_TX)
  {
    if      (status == BB_STATUS_SUCCESS)  { diagTxSuccess++; }
    else if (status == BB_STATUS_FAILED)   { diagTxFail++;    }
    else { BB_ASSERT(0); } /* unexpected status for the operation */
  }
  else if (diagOp == DIAG_TX_TIFS)
  {
    if      (status == BB_STATUS_SUCCESS)  { diagTxTifsSuccess++; }
    else if (status == BB_STATUS_FAILED)   { diagTxTifsFail++;    }
    else { BB_ASSERT(0); } /* unexpected status for the operation */
  }
  else
  {
    BB_ASSERT(0); /* unexpected operation */
  }

  /* extra integrity checks, make sure the accounting adds up */
  BB_ASSERT(diagTxTotalCalls == (uint16_t)(diagTxSuccess + diagTxFail + diagTxTifsSuccess + diagTxTifsFail));
  BB_ASSERT(diagTxCalls      == (uint16_t)(diagTxSuccess + diagTxFail));
  BB_ASSERT(diagTxTifsCalls  == (uint16_t)(diagTxTifsSuccess + diagTxTifsFail));

  BB_ASSERT(diagTxFail       == (uint16_t)(diagTxFailBadDueTime + diagTxFailMissedDueTime));
  BB_ASSERT(diagTxTifsFail   == (uint16_t)(diagTxTifsFailMissed + diagTxTifsFailNotPossible));

  /* keep track of last operation */
  diagOpLast = diagOp;
  diagOp = DIAG_NULL;
  diagLastStatus = status;

  /* finally, run the user specified callback, everything else is just diagnostics */
  diagTxCallback(status);
}

/*===============================================================================================*/
/*!
 *  \brief      Diagnostic function to record callback status.
 *
 *  \param      status      status as reported via callback
 *
 *  For diagnostics, this function is used to intercept passing control to the configured
 *  callback function. It is used to record diagnostic information.
 *
 */
/*===============================================================================================*/
static void DiagFauxRxCallback(uint8_t status, int8_t rssi, uint32_t crc, uint32_t timestamp)
{
  BB_ASSERT(diagOp != DIAG_NULL); /* more than one callback for the operation? */
  BB_ASSERT((diagOp == DIAG_RX) || (diagOp == DIAG_RX_TIFS)); /* apparent criss-cross of tx/rx, typo somewhere? */

  /* keep account of each operation-result */
  if (diagOp == DIAG_RX)
  {
    if      (status == BB_STATUS_SUCCESS)           { diagRxSuccess++; }
    else if (status == BB_STATUS_FAILED)            { diagRxFail++; }
    else if (status == BB_STATUS_RX_TIMEOUT)        { diagRxTimeout++; }
    else if (status == BB_STATUS_CRC_FAILED)        { diagRxCrcFail++; }
    else { BB_ASSERT(0); } /* unexpected status for the operation */
  }
  else if (diagOp == DIAG_RX_TIFS)
  {
    if      (status == BB_STATUS_SUCCESS)           { diagRxTifsSuccess++; }
    else if (status == BB_STATUS_FAILED)            { diagRxTifsFail++; }
    else if (status == BB_STATUS_RX_TIMEOUT)        { diagRxTifsTimeout++; }
    else if (status == BB_STATUS_CRC_FAILED)        { diagRxTifsCrcFail++; }
    else { BB_ASSERT(0); } /* unexpected status for the operation */
  }
  else
  {
    BB_ASSERT(0); /* unexpected operation */
  }

  /* extra integrity check, make sure the accounting adds up */
  BB_ASSERT(diagRxTotalCalls  == (uint16_t)( diagRxSuccess +
                                              diagRxFail +
                                              diagRxTimeout +
                                              diagRxCrcFail +
                                              diagRxTifsSuccess +
                                              diagRxTifsFail +
                                              diagRxTifsTimeout +
                                              diagRxTifsCrcFail ));

  BB_ASSERT(diagRxCalls       == (uint16_t)( diagRxSuccess +
                                              diagRxFail +
                                              diagRxTimeout +
                                              diagRxCrcFail ));

  BB_ASSERT(diagRxTifsCalls   == (uint16_t)( diagRxTifsSuccess +
                                              diagRxTifsFail +
                                              diagRxTifsTimeout +
                                              diagRxTifsCrcFail ));

  BB_ASSERT(diagRxFail     == (uint16_t)(diagRxFailBadDueTime + diagRxFailMissedDueTime));
  BB_ASSERT(diagRxTifsFail == (uint16_t)(diagRxTifsFailMissed + diagRxTifsFailNotPossible));

  /* keep track of last operation */
  diagOpLast = diagOp;
  diagOp = DIAG_NULL;
  diagLastStatus = status;

  /* finally, run the user specified callback, everything else is just diagnostics */
  diagRxCallback(status, rssi, crc, timestamp);
}
#endif

/////////////////////////////////////////////////////////////////////////////////
