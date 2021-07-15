/* Copyright (c) 2009-2019 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"){}
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cordio_stack/platform/include/pal_types.h"
#include "cordio_stack/platform/include/pal_uart.h"
#include "hal/ticker_api.h"
#include "platform/mbed_critical.h"
#include "pal_codec.h"
#include "platform/mbed_error.h"

#ifdef __cplusplus
extern "C" {
#endif

static const mbed_error_status_t function_not_implemented = MBED_MAKE_ERROR(MBED_MODULE_BLE, ENOSYS);

/* UART */

MBED_WEAK PalUartState_t PalUartGetState(PalUartId_t id)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalUartGetState");
    return PAL_UART_STATE_UNINIT;
}

MBED_WEAK void PalUartInit(PalUartId_t id, const PalUartConfig_t *pCfg)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalUartInit");
}

MBED_WEAK void PalUartReadData(PalUartId_t id, uint8_t *pData, uint16_t len)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalUartReadData");
}

MBED_WEAK void PalUartWriteData(PalUartId_t id, const uint8_t *pData, uint16_t len)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalUartWriteData");
}

/* LED */

MBED_WEAK void PalLedOn(uint8_t id)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalLedOn");
}

MBED_WEAK void PalLedOff(uint8_t id)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalLedOff");
}

/* RTC */

MBED_WEAK void PalRtcInit()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcInit");
}

MBED_WEAK void PalRtcEnableCompareIrq(uint8_t channelId)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcEnableCompareIrq");
}

MBED_WEAK void PalRtcDisableCompareIrq(uint8_t channelId)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcDisableCompareIrq");
}

MBED_WEAK uint32_t PalRtcCounterGet()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcCounterGet");
    return 0;
}

MBED_WEAK void PalRtcCompareSet(uint8_t channelId, uint32_t value)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcCompareSet");
}

MBED_WEAK uint32_t PalRtcCompareGet()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalRtcCompareGet");
    return 0;
}

/* SYS */

MBED_WEAK bool_t PalSysIsBusy()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalSysIsBusy");
    return 0;
}

MBED_WEAK void PalSysAssertTrap()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalSysAssertTrap");
}

MBED_WEAK void PalSysSleep()
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalSysSleep");
}

MBED_WEAK void PalSysSetTrap(bool_t enable)
{
    (void)enable;
}

MBED_WEAK uint32_t PalSysGetAssertCount()
{
    return 0;
}

MBED_WEAK uint32_t PalSysGetStackUsage()
{
    return 0;
}

/* CS */

MBED_WEAK void PalEnterCs(void)
{
    core_util_critical_section_enter();
}
MBED_WEAK void PalExitCs(void)
{
    core_util_critical_section_exit();
}

/* ISO channels */
MBED_WEAK void PalCodecReadLocalSupportedCodecs(uint8_t *pNumStd, PalCodecStdInfo_t stdCodecs[],
                                                uint8_t *pNumVs, PalCodecVsInfo_t vsCodecs[])
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecReadLocalSupportedCodecs");
}

MBED_WEAK bool_t PalCodecReadLocalSupportedCodecCapabilities(
    uint8_t codingFmt, uint16_t compId, uint16_t vsCodecId, PalCodecDir_t dir, uint8_t transType,
                                                   uint8_t *pCapLen, uint8_t *pCap)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecReadLocalSupportedCodecCapabilities");
    return 0;
}

MBED_WEAK bool_t PalCodecReadLocalSupportedControllerDelay(
    uint8_t codingFmt, uint16_t compId, uint16_t vsCodecId, PalCodecDir_t dir, uint8_t transType,
    uint32_t *pMinDly, uint32_t *pMaxDly)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecReadLocalSupportedControllerDelay");
    return 0;
}
MBED_WEAK bool_t PalCodecConfigureDataPath(PalCodecDir_t dir, uint8_t vsCfgLen, const uint8_t *pVsCfg)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecConfigureDataPath");
    return 0;
}

MBED_WEAK void PalCodecDataInit(void)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecDataInit");
}

MBED_WEAK bool_t PalCodecDataStartStream(uint16_t id, PalCodecStreamParam_t *pParam)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecDataStartStream");
    return 0;
}

MBED_WEAK void PalCodecDataStopStream(uint16_t id, PalCodecDir_t dir)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecDataStopStream");
}

MBED_WEAK void PalCodecDataStreamIn(uint16_t id, uint8_t *pBuf, uint16_t len)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecDataStreamIn");
}

MBED_WEAK void PalCodecDataStreamOut(uint16_t id, const uint8_t *pBuf, uint16_t len, uint32_t sduRef)
{
    MBED_ERROR(function_not_implemented, "Provide implementation of PalCodecDataStreamOut");
}


#ifdef __cplusplus
};
#endif
