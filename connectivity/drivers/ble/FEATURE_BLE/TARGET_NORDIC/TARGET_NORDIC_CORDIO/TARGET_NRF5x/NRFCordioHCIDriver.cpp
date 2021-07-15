/* mbed Microcontroller Library
 * Copyright (c) 2017-2017 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// mbed Includes
#include "mbed_assert.h"
#include "platform/mbed_power_mgmt.h"
#include "mbed_critical.h"

// Cordio Includes
#include "ll_init_api.h"
#include "ll_defs.h"
#include "custom_chci_tr.h"
#include "pal_bb.h"
#include "pal_cfg.h"
#include "pal_frc.h"
#include "lhci_api.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_timer.h"
#include "wsf_trace.h"
#include "dm_api.h"

// Nordic Includes
#include "nrf.h"
#include "nrf5x_lf_clk_helper.h"

#include "NRFCordioHCIDriver.h"
#include "NRFCordioHCITransportDriver.h"

#ifdef DEBUG
#include <stdio.h>
#include <stdarg.h>
#define DBG_WARN(...) printf(__VA_ARGS__)
#else
#define DBG_WARN(...)
#endif

#include "mbed_nrf5x_adaptation.h"

using namespace ble;

/*! \brief      Typical implementation revision number (LlRtCfg_t::implRev). */
#define LL_IMPL_REV             0x2303

#if MBED_CONF_NORDIC_NRF_LF_CLOCK_SRC == NRF_LF_SRC_SYNTH
#define NRF_LF_CLK_SRC CLOCK_LFCLKSRC_SRC_Synth
#elif MBED_CONF_NORDIC_NRF_LF_CLOCK_SRC == NRF_LF_SRC_XTAL
#define NRF_LF_CLK_SRC CLOCK_LFCLKSRC_SRC_Xtal
#elif MBED_CONF_NORDIC_NRF_LF_CLOCK_SRC == NRF_LF_SRC_RC
#define NRF_LF_CLK_SRC CLOCK_LFCLKSRC_SRC_RC
#endif

// Note to implementer: this should be amended if the Cordio stack is updated

// The Nordic-specific baseband configuration
// The BB_ config macros are set in the bb_api.h header file
const BbRtCfg_t NRFCordioHCIDriver::_bb_cfg = {
    /*clkPpm*/ 20,
    /*rfSetupDelayUs*/ BB_RF_SETUP_DELAY_US,
    /*maxScanPeriodMs*/ BB_MAX_SCAN_PERIOD_MS,
    /*schSetupDelayUs*/ BB_SCH_SETUP_DELAY_US,
    /*BbTimerBoundaryUs*/
#if (BB_CLK_RATE_HZ == 32768)
  BB_RTC_MAX_VALUE_US
#elif (BB_CLK_RATE_HZ == 8000000)
  BB_TIMER_8MHZ_MAX_VALUE_US
#elif (BB_CLK_RATE_HZ == 1000000)
  BB_TIMER_1MHZ_MAX_VALUE_US
#else
  #error "Unsupported platform."
#endif
};

/* +12 for message headroom, + 2 event header, +255 maximum parameter length. */
static const uint16_t maxRptBufSize = 12 + 2 + 255;

/* +12 for message headroom, +4 for header. */
static const uint16_t aclBufSize = 12 + MBED_CONF_CORDIO_LL_MAX_ACL_SIZE + 4 + BB_DATA_PDU_TAILROOM;

const LlRtCfg_t NRFCordioHCIDriver::_ll_cfg = {
/* Device */
/** Company ID (default to ARM Ltd. ID). */
/*uint16_t*/  .compId = LL_COMP_ID_ARM,
/** Implementation revision number. */
/*uint16_t*/  .implRev = LL_IMPL_REV,
/** Core specification implementation level (LL_VER_BT_CORE_SPEC_4_2). */
/*uint8_t*/   .btVer = LL_VER_BT_CORE_SPEC_5_0,
/** Unused. Align next field to word boundary. */
/*uint32_t*/  ._align32 = 0,

/* Advertiser */
/** Maximum number of advertising sets. */
/*uint8_t*/   .maxAdvSets = MBED_CONF_CORDIO_LL_MAX_ADVERTISING_SETS,
/** Maximum number of pending legacy or extended advertising reports. */
/*uint8_t*/   .maxAdvReports = MBED_CONF_CORDIO_LL_MAX_ADVERTISING_REPORTS,
/** Maximum extended advertising data size. */
/*uint16_t*/  .maxExtAdvDataLen = MBED_CONF_CORDIO_LL_EXTENDED_ADVERTISING_SIZE,
/** Default extended advertising data fragmentation size. */
/*uint8_t*/   .defExtAdvDataFrag = MBED_CONF_CORDIO_LL_DEFAULT_EXTENDED_ADVERTISING_FRAGMENTATION_SIZE,
/** Auxiliary Offset delay above T_MAFS in microseconds. */
/*uint16_t*/  .auxDelayUsec = 0,
/** Delay of auxiliary packet in microseconds from the time specified by auxPtr. */
/*uint16_t*/  .auxPtrOffsetUsec = 0,
/* Scanner */
/** Maximum scan request received events. */
/*uint8_t*/   .maxScanReqRcvdEvt = MBED_CONF_CORDIO_LL_MAX_SCAN_REQUEST_EVENTS,
/** Maximum extended scan data size. */
/*uint16_t*/  .maxExtScanDataLen = MBED_CONF_CORDIO_LL_EXTENDED_ADVERTISING_SIZE,

/* Connection */
/** Maximum number of connections. */
/*uint8_t*/   .maxConn = DM_CONN_MAX,
/** Default number of transmit buffers. */
/*uint8_t*/   .numTxBufs = MBED_CONF_CORDIO_LL_TX_BUFFERS,
/** Default number of receive buffers. */
/*uint8_t*/   .numRxBufs = MBED_CONF_CORDIO_LL_RX_BUFFERS,
/** Maximum ACL buffer size. */
/*uint16_t*/  .maxAclLen = MBED_CONF_CORDIO_LL_MAX_ACL_SIZE,
/** Default Tx power level for connections. */
/*int8_t*/    .defTxPwrLvl = 0,
/** Allowable CE jitter on a slave (account for master's sleep clock resolution). */
/*uint8_t*/   .ceJitterUsec = 0,
/* ISO */
/** Default number of ISO transmit buffers. */
/*uint8_t*/   .numIsoTxBuf = 0,
/** Default number of ISO receive buffers. */
/*uint8_t*/   .numIsoRxBuf = 0,
/** Maximum ISO buffer size between host and controller. */
/*uint16_t*/  .maxIsoSduLen = 0,
/** Maximum ISO PDU buffer size. */
/*uint16_t*/  .maxIsoPduLen = 0,
/* CIS */
/** Maximum number of CIG. */
/*uint8_t*/   .maxCig = 0,
/** Maximum number of CIS. */
/*uint8_t*/   .maxCis = 0,
/** Subevent spacing above T_MSS. */
/*uint16_t*/  .cisSubEvtSpaceDelay = 0,
/* BIS */
/** Maximum number of BIG. */
/* uint8_t */ .maxBig = 0,
/** Maximum number of BIS. */
/* uint8_t */ .maxBis = 0,
/* DTM */
/** DTM Rx synchronization window in milliseconds. */
/*uint16_t*/  .dtmRxSyncMs = 10000,
/* PHY */
/** 2M PHY supported. */
/*bool_t*/    .phy2mSup = MBED_CONF_CORDIO_LL_PHY_2M_SUPPORT,
/** Coded PHY supported. */
/*bool_t*/    .phyCodedSup = MBED_CONF_CORDIO_LL_PHY_CODED_SUPPORT,
/** Tx stable modulation index supported. */
/*bool_t*/    .stableModIdxTxSup = TRUE,
/** Rx stable modulation index supported. */
/*bool_t*/    .stableModIdxRxSup = TRUE,
/** High RSSI threshold for power monitoring. */
/*int8_t*/    .pcHighThreshold = -50,
/** Low RSSI threshold for power monitoring. */
/*int8_t*/    .pcLowThreshold = -70,
/** Interval spacing of channel classification reporting. */
/*uint8_t*/   .chClassIntSpacing = 50
};

extern "C" void PAL_BB_IRQ_HANDLER(void);

NRFCordioHCIDriver::NRFCordioHCIDriver(CordioHCITransportDriver& transport_driver) : CordioHCIDriver(transport_driver), _is_init(false), _stack_buffer(NULL)
{
    _stack_buffer = (uint8_t*)malloc(MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE);
    MBED_ASSERT(_stack_buffer != NULL);
}

NRFCordioHCIDriver::~NRFCordioHCIDriver()
{
	// Deactivate all interrupts
	NVIC_DisableIRQ(RADIO_IRQn);
	NVIC_DisableIRQ(PAL_BB_IRQn);

    // Switch off Radio peripheral
    // TODO interop with 802.15.4
    NRF_RADIO->POWER = 0;

    // Stop timer
    PAL_BB_TIMER->TASKS_STOP  = 1;
    PAL_BB_TIMER->TASKS_CLEAR = 1;

	NVIC_ClearPendingIRQ(RADIO_IRQn);
	NVIC_ClearPendingIRQ(PAL_BB_IRQn);

    MBED_ASSERT(_stack_buffer != NULL);
    free(_stack_buffer);
    _stack_buffer = NULL;

    MBED_ASSERT(_stack_buffer == NULL);
}

ble::buf_pool_desc_t NRFCordioHCIDriver::get_buffer_pool_description()
{
    static union {
        uint8_t buffer[MBED_CONF_CORDIO_NORDIC_LL_WSF_POOL_BUFFER_SIZE];
        uint64_t align;
    };
    static const wsfBufPoolDesc_t pool_desc[] = {
            {  16, 16  + 8},
            {  32, 16 + 4 },
            {  64, 8 },
            { 128, 4 + MBED_CONF_CORDIO_LL_MAX_ADVERTISING_REPORTS },
		    { aclBufSize, MBED_CONF_CORDIO_LL_TX_BUFFERS + MBED_CONF_CORDIO_LL_RX_BUFFERS }
    };

    return buf_pool_desc_t(buffer, pool_desc);
}

void NRFCordioHCIDriver::do_initialize()
{
	if(_is_init) {
		return;
	}

	_is_init = true;

    // Setup BB & LL config
    LlInitRtCfg_t ll_init_cfg =
    {
        .pBbRtCfg     = &_bb_cfg,
        .wlSizeCfg    = 4,
        .rlSizeCfg    = 4,
        .plSizeCfg    = 4,
        .pLlRtCfg     = &_ll_cfg,
        .pFreeMem     = _stack_buffer,
        .freeMemAvail = MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE
    };

    /* switch to more accurate 16 MHz crystal oscillator (system starts up using 16MHz RC oscillator) */
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
    }

    /* configure low-frequency clock */
    NRF_CLOCK->LFCLKSRC             = (NRF_LF_CLK_SRC << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED  = 0;
    NRF_CLOCK->TASKS_LFCLKSTART     = 1;
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
    }
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    // Start RTC.
    PAL_BB_RTC->TASKS_STOP  = 1;
    PAL_BB_RTC->TASKS_CLEAR = 1;
    PAL_BB_RTC->PRESCALER   = 0;            /* clear prescaler */
    PAL_BB_RTC->TASKS_START = 1;

#if PAL_ALLOW_DEEP_SLEEP
    // Start FRC.
    PalFrcInit();
#endif

    // Cycle radio peripheral power to guarantee known radio state
    NRF_RADIO->POWER = 0;
    NRF_RADIO->POWER = 1;

    // mbed-os target uses IRQ Handler names with _v added at the end
    // (PAL_BB_IRQ_HANDLER_v) so we need to register this manually
    NVIC_SetVector(PAL_BB_IRQn, (uint32_t)PAL_BB_IRQ_HANDLER);

    // Extremely ugly
    for(uint32_t irqn = 0; irqn < 32; irqn++)
    {
        uint8_t prio = NVIC_GetPriority((IRQn_Type)irqn);
        if( prio < 2 ) {
            NVIC_SetPriority((IRQn_Type)irqn, 2);
        }
    }

    // WARNING
    // If a submodule does not have enough space to allocate its memory from buffer, it will still allocate its memory (and do a buffer overflow) and return 0 (as in 0 byte used)
    // however that method will still continue which will lead to undefined behaviour
    // So whenever a change of configuration is done, it's a good idea to set MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE to a high value and then reduce accordingly
    uint32_t mem_used = LlInitControllerInit(&ll_init_cfg);
    if( mem_used < MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE )
    {
        // Sub-optimal, give warning
        DBG_WARN("NRFCordioHCIDriver: MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE can be reduced to %lu instead of %lu", mem_used, MBED_CONF_CORDIO_NORDIC_LL_HCI_DRIVER_BUFFER_SIZE);
    }

    // BD Addr
    bdAddr_t bd_addr;

    /* Load address from nRF configuration. */
    uint64_t address_int = (((uint64_t)NRF_FICR->DEVICEADDR[0]) <<  0) |
                           (((uint64_t)NRF_FICR->DEVICEADDR[1]) << 32);

    for (unsigned int i = 0; i < BDA_ADDR_LEN; ++i) {
        bd_addr[i] = address_int >> (i * 8);
    }

    LlSetBdAddr((uint8_t *)&bd_addr);
    LlMathSetSeed((uint32_t *)&bd_addr);

//#ifdef DEBUG
//    WsfTraceRegister(wsf_trace_handler);
//#endif

    // We're sharing the host stack's event queue
}

void NRFCordioHCIDriver::do_terminate()
{

}

void NRFCordioHCIDriver::start_reset_sequence()
{
#if BLE_FEATURE_EXTENDED_ADVERTISING
    // Make sure extended adv is init
    DmExtAdvInit();
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    CordioHCIDriver::start_reset_sequence();
}

bool NRFCordioHCIDriver::get_random_static_address(ble::address_t& address)
{
    PalCfgLoadData(PAL_CFG_ID_BD_ADDR, address.data(), sizeof(bdAddr_t));

    MBED_ASSERT((address[5] & 0xC0) == 0xC0);

    return true;
}

void NRFCordioHCIDriver::on_deep_sleep(timestamp_t wakeTimeMs)
{
#if PAL_ALLOW_DEEP_SLEEP
    if (wakeTimeMs > PAL_WAKE_LATENCY_MS) {
        palFrcSetWakeup(wakeTimeMs - PAL_WAKE_LATENCY_MS);
    }
#endif
}

ble::CordioHCIDriver& ble_cordio_get_hci_driver() {
    static NRFCordioHCITransportDriver transport_driver;

    static NRFCordioHCIDriver hci_driver(
        transport_driver
    );

    return hci_driver;
}
