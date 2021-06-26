/*
 * Copyright (c) 2020, Nuvoton Technology Corporation
 *
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

#include "cmsis.h"
#include "platform_extra_secure.h"

#if __MBED__
#include "mbed_error.h"
#include "nu_bitutil.h"
#include "tfm_platform_api.h"
#elif NU_TFM_PLAT_IOCTL_NS
#include "tfm_platform_api.h"
#elif NU_TFM_PLAT_IOCTL_S
#include "tfm_platform_system.h"
#include <string.h>
#endif
    
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS

#if __MBED__
#define PLAT_NSC_ERROR(SEC_FUNC, RC)                                            \
    MBED_ERROR1(MBED_MAKE_ERROR(MBED_MODULE_HAL, MBED_ERROR_CODE_UNDERFLOW),    \
                #SEC_FUNC "() failed with: ", RC)

#elif NU_TFM_PLAT_IOCTL_NS
#define PLAT_NSC_ERROR(SEC_FUNC, RC)                                            \
    do {                                                                        \
        printf(#SEC_FUNC "() failed with: %d\n", RC);                           \
        while(1);                                                               \
    } while(0)

#endif

/* Secure function call via platform ioctl */
#define PLAT_NSC_CALL(SEC_FUNC, INBUF, INSIZE, OUTBUF, OUTSIZE)                     \
    PLAT_NSC_CALL_(NU_PLAT_XTRA_SEC_REQ(SEC_FUNC), INBUF, INSIZE, OUTBUF, OUTSIZE)  \
    if (rc != TFM_PLATFORM_ERR_SUCCESS) {                                           \
        PLAT_NSC_ERROR(SEC_FUNC, rc);                                               \
    }

#define PLAT_NSC_CALL_(REQ, INBUF, INSIZE, OUTBUF, OUTSIZE)     \
    enum tfm_platform_err_t rc;                                 \
    tfm_platform_ioctl_req_t request;                           \
    psa_invec invec;                                            \
    psa_outvec outvec;                                          \
    request = (tfm_platform_ioctl_req_t) REQ;                   \
    if ((INBUF)) {                                              \
        invec.base = (INBUF);                                   \
        invec.len = (INSIZE);                                   \
        if ((OUTBUF)) {                                         \
            outvec.base = (OUTBUF);                             \
            outvec.len = (OUTSIZE);                             \
            rc = tfm_platform_ioctl(request, &invec, &outvec);  \
        } else {                                                \
            rc = tfm_platform_ioctl(request, &invec, NULL);     \
        }                                                       \
    } else if ((OUTBUF)) {                                      \
        outvec.base = (OUTBUF);                                 \
        outvec.len = (OUTSIZE);                                 \
        rc = tfm_platform_ioctl(request, NULL, &outvec);        \
    } else {                                                    \
        rc = tfm_platform_ioctl(request, NULL, NULL);           \
    }

#elif NU_TFM_PLAT_IOCTL_S

/* NOTE: Hazard of passing struct argument between client and service
 *
 * In TF-M IPC mode (TFM_PSA_API), struct argument is serialized and struct
 * alignment doesn't guarantee as original. We resolve it by introducing
 * intermediate struct during struct de-serialize, reglardless library or IPC
 * mode.
 *
 * Check platform_sp_ioctl_ipc for struct serialize and align hazard:
 * trusted-firmware-m > secure_fw > partitions > platform > platform_sp.c
 */

/* Check parameter validity */
#define NU_CHK_PARAM_VAL(INSIZE, OUTSIZE)                       \
    if (INSIZE) {                                               \
        if (!in_vec ||                                          \
            !in_vec->base ||                                    \
            (in_vec->len != INSIZE && INSIZE != INT_MAX)) {     \
            return TFM_PLATFORM_ERR_INVALID_PARAM;              \
        }                                                       \
    } else {                                                    \
        if (in_vec) {                                           \
            return TFM_PLATFORM_ERR_INVALID_PARAM;              \
        }                                                       \
    }                                                           \
    if (OUTSIZE) {                                              \
        if (!out_vec ||                                         \
            !out_vec->base ||                                   \
            (out_vec->len != OUTSIZE && OUTSIZE != INT_MAX)) {  \
            return TFM_PLATFORM_ERR_INVALID_PARAM;              \
        }                                                       \
    } else {                                                    \
        if (out_vec) {                                          \
            return TFM_PLATFORM_ERR_INVALID_PARAM;              \
        }                                                       \
    }                                                           \

#define NU_MFP_POS(pin)                             ((pin % 8) * 4)
#define NU_MFP_MSK(pin)                             (0xful << NU_MFP_POS(pin))

bool nu_check_sys_ns(uint32_t modidx);
bool nu_check_clk_ns(uint32_t modidx);
bool nu_check_gpio_ns(uint32_t port_index, uint32_t pin_index);

#endif

#if NU_TFM_PLAT_IOCTL_NS || NU_TFM_PLAT_IOCTL_S

__STATIC_INLINE uint32_t nu_get32_be(const uint8_t *pos)
{
	uint32_t val;
	
	val = *pos ++;
	val <<= 8;
	val += *pos ++;
	val <<= 8;
	val += *pos ++;
	val <<= 8;
	val += *pos;
	
	return val;
}

__STATIC_INLINE void nu_set32_be(uint8_t *pos, uint32_t val)
{
	*pos ++ = val >> 24;
	*pos ++ = val >> 16;
	*pos ++ = val >> 8;
	*pos ++ = (val & 0xFF);
}

#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_ResetModule_S(uint32_t u32ModuleIndex)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, u32ModuleIndex);

    /* Invoke NSC function */
    PLAT_NSC_CALL(SYS_ResetModule_S, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_ResetModule_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 0);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);

    /* Guard access to secure module from non-secure domain */
    if (!nu_check_sys_ns(u32ModuleIndex)) {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    SYS_ResetModule(u32ModuleIndex);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_LockReg_S(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(SYS_LockReg_S, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_LockReg_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    SYS_LockReg();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_UnlockReg_S(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(SYS_UnlockReg_S, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_UnlockReg_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    SYS_UnlockReg();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_SetModuleClock_S(uint32_t u32ModuleIndex, uint32_t u32ClkSrc, uint32_t u32ClkDiv)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[12];
    nu_set32_be(inbuf, u32ModuleIndex);
    nu_set32_be(inbuf + 4, u32ClkSrc);
    nu_set32_be(inbuf + 8, u32ClkDiv);

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_SetModuleClock_S, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_SetModuleClock_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(12, 0);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);
    uint32_t u32ClkSrc = nu_get32_be(((const uint8_t *) in_vec->base) + 4);
    uint32_t u32ClkDiv = nu_get32_be(((const uint8_t *) in_vec->base) + 8);

    /* Guard access to secure module from non-secure domain */
    if (!nu_check_clk_ns(u32ModuleIndex)) {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    CLK_SetModuleClock(u32ModuleIndex, u32ClkSrc, u32ClkDiv);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_EnableModuleClock_S(uint32_t u32ModuleIndex)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, u32ModuleIndex);

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_EnableModuleClock_S, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_EnableModuleClock_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 0);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);

    /* Guard access to secure module from non-secure domain */
    if (!nu_check_clk_ns(u32ModuleIndex)) {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    CLK_EnableModuleClock(u32ModuleIndex);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_DisableModuleClock_S(uint32_t u32ModuleIndex)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, u32ModuleIndex);

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_DisableModuleClock_S, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_DisableModuleClock_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 0);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);

    /* Guard access to secure module from non-secure domain */
    if (!nu_check_clk_ns(u32ModuleIndex)) {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    CLK_DisableModuleClock(u32ModuleIndex);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_Idle_S(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_Idle_S, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_Idle_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    SYS_UnlockReg();
    CLK_Idle();
    SYS_LockReg();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_PowerDown_S(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_PowerDown_S, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_PowerDown_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    SYS_UnlockReg();
    CLK_PowerDown();
    SYS_LockReg();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetHXTFreq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetHXTFreq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetHXTFreq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetHXTFreq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetLXTFreq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetLXTFreq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetLXTFreq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetLXTFreq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetHCLKFreq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetHCLKFreq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetHCLKFreq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetHCLKFreq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPCLK0Freq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetPCLK0Freq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPCLK0Freq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetPCLK0Freq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPCLK1Freq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetPCLK1Freq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPCLK1Freq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetPCLK1Freq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetCPUFreq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetCPUFreq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetCPUFreq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetCPUFreq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPLLClockFreq_S(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetPLLClockFreq_S, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPLLClockFreq_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    uint32_t output = CLK_GetPLLClockFreq();
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetModuleClockSource_S(uint32_t u32ModuleIndex)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, u32ModuleIndex);

    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetModuleClockSource_S, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetModuleClockSource_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 4);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);
    uint32_t output = CLK_GetModuleClockSource(u32ModuleIndex);
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetModuleClockDivider_S(uint32_t u32ModuleIndex)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, u32ModuleIndex);

    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(CLK_GetModuleClockDivider_S, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));

    /* Finalize output */
    uint32_t output = nu_get32_be(outbuf);
    return output;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetModuleClockDivider_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 4);

    uint32_t u32ModuleIndex = nu_get32_be(in_vec->base);
    uint32_t output = CLK_GetModuleClockDivider(u32ModuleIndex);
    nu_set32_be(out_vec->base, output);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_Open_S(S_RTC_TIME_DATA_T_PTR sPt)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_Open_S, sPt, (sPt ? sizeof(S_RTC_TIME_DATA_T) : 0), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_Open_S)
{
    /* Check parameter validity */
    if (!in_vec) {
        RTC_Open(NULL);
    } else if (in_vec->base && in_vec->len == sizeof(S_RTC_TIME_DATA_T)) {
        S_RTC_TIME_DATA_T sPt;
        memcpy(&sPt, in_vec->base, sizeof(S_RTC_TIME_DATA_T));
        RTC_Open(&sPt);
    } else {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_Close_S(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_Close_S, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_Close_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    RTC_Close();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_WaitAccessEnable_S(void)
{
    /* On M2354, RTC_WaitAccessEnable() is unnecessary and is not provided by BSP.
     * Provide a dummy one to make code consistent. */
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_WaitAccessEnable_S)
{
    /* On M2354, RTC_WaitAccessEnable() is unnecessary and is not provided by BSP.
     * Provide a dummy one to make code consistent. */

    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_GetDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_GetDateAndTime_S, NULL, 0, sPt, sizeof(S_RTC_TIME_DATA_T));
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_GetDateAndTime_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, sizeof(S_RTC_TIME_DATA_T));

    S_RTC_TIME_DATA_T sPt;
    RTC_GetDateAndTime(&sPt);
    memcpy(out_vec->base, &sPt, sizeof(S_RTC_TIME_DATA_T));

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_GetAlarmDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_GetAlarmDateAndTime_S, NULL, 0, sPt, sizeof(S_RTC_TIME_DATA_T));
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_GetAlarmDateAndTime_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, sizeof(S_RTC_TIME_DATA_T));

    S_RTC_TIME_DATA_T sPt;
    RTC_GetAlarmDateAndTime(&sPt);
    memcpy(out_vec->base, &sPt, sizeof(S_RTC_TIME_DATA_T));

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_SetDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_SetDateAndTime_S, sPt, sizeof(S_RTC_TIME_DATA_T), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_SetDateAndTime_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(sizeof(S_RTC_TIME_DATA_T), 0);

    S_RTC_TIME_DATA_T sPt;
    memcpy(&sPt, in_vec->base, sizeof(S_RTC_TIME_DATA_T));
    RTC_SetDateAndTime(&sPt);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_SetAlarmDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(RTC_SetAlarmDateAndTime_S, sPt, sizeof(S_RTC_TIME_DATA_T), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_SetAlarmDateAndTime_S)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(sizeof(S_RTC_TIME_DATA_T), 0);

    S_RTC_TIME_DATA_T sPt;
    memcpy(&sPt, in_vec->base, sizeof(S_RTC_TIME_DATA_T));
    RTC_SetAlarmDateAndTime(&sPt);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_pin_function_s(uint32_t port_index, uint32_t pin_index, uint32_t data)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[12];
    nu_set32_be(inbuf, port_index);
    nu_set32_be(inbuf + 4, pin_index);
    nu_set32_be(inbuf + 8, data);

    /* Invoke NSC function */
    PLAT_NSC_CALL(nu_pin_function_s, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_pin_function_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(12, 0);

    uint32_t port_index = nu_get32_be(in_vec->base);
    uint32_t pin_index = nu_get32_be(((const uint8_t *) in_vec->base) + 4);
    uint32_t data = nu_get32_be(((const uint8_t *) in_vec->base) + 8);

    /* Guard access to secure GPIO from non-secure domain */
    if (!nu_check_gpio_ns(port_index, pin_index)) {
        return TFM_PLATFORM_ERR_INVALID_PARAM;
    }

    __IO uint32_t *GPx_MFPx = ((__IO uint32_t *) &SYS->GPA_MFPL) + port_index * 2 + (pin_index / 8);
    uint32_t MFP_Msk = NU_MFP_MSK(pin_index);

    // E.g.: SYS->GPA_MFPL  = (SYS->GPA_MFPL & (~SYS_GPA_MFPL_PA0MFP_Msk) ) | SYS_GPA_MFPL_PA0MFP_SC0_CD  ;
    *GPx_MFPx  = (*GPx_MFPx & (~MFP_Msk)) | data;

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_idle_s(void)
{
    /* We should have had default (shallow) sleep/idle mode configuration guaranteed by SPE.
     * Just call WFI in NSPE. This is to remove SPE/NSPE boundary latency (200us) and to pass
     * (shallow) sleep-related tests. */
    __WFI();
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_idle_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_powerdown_s(void)
{
    /* Invoke NSC function */
    PLAT_NSC_CALL(nu_powerdown_s, NULL, 0, NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_powerdown_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 0);

    /* Switch to deep sleep/power-down mode configuration */
    SYS_UnlockReg();
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    CLK->PWRCTL |= CLK_PWRCTL_PDEN_Msk;
    SYS_LockReg();

    /* Enter deep sleep/power-down mode */
    __WFI();

    /* Switch back to default (shallow) sleep/idle mode configuration to enable entering
     * (shallow) sleep/idle mode without crossing SPE/NSPE boundary */
    SYS_UnlockReg();
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    CLK->PWRCTL &= ~CLK_PWRCTL_PDEN_Msk;
    SYS_LockReg();

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t nu_rtc_read_spare_register_s(uint32_t reg_num)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[4];
    nu_set32_be(inbuf, reg_num);

    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(nu_rtc_read_spare_register_s, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));

    /* Finalize output */
    int32_t reg_val = nu_get32_be(outbuf);
    return reg_val;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_read_spare_register_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(4, 4);

    uint32_t reg_num = nu_get32_be(in_vec->base);
    RTC_WaitAccessEnable();
    RTC_EnableSpareAccess();
    RTC_WaitAccessEnable();
    uint32_t reg_val = RTC_READ_SPARE_REGISTER(RTC_S, reg_num);
    nu_set32_be(out_vec->base, reg_val);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_rtc_write_spare_register_s(uint32_t reg_num, uint32_t reg_val)
{
    /* Set up input parameter for NSC call */
    uint8_t inbuf[8];
    nu_set32_be(inbuf, reg_num);
    nu_set32_be(inbuf + 4, reg_val);

    /* Invoke NSC function */
    PLAT_NSC_CALL(nu_rtc_write_spare_register_s, inbuf, sizeof(inbuf), NULL, 0);
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_write_spare_register_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(8, 0);

    uint32_t reg_num = nu_get32_be(in_vec->base);
    uint32_t reg_val = nu_get32_be(((const uint8_t *) in_vec->base) + 4);
    RTC_WaitAccessEnable();
    RTC_EnableSpareAccess();
    RTC_WaitAccessEnable();
    RTC_WRITE_SPARE_REGISTER(RTC_S, reg_num, reg_val);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
int32_t nu_rtc_isenabled_s(void)
{
    /* Set up output parameter for NSC call */
    uint8_t outbuf[4];

    /* Invoke NSC function */
    PLAT_NSC_CALL(nu_rtc_isenabled_s, NULL, 0, outbuf, sizeof(outbuf));

    /* Finalize output */
    int32_t enabled = nu_get32_be(outbuf);
    return enabled;
}
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_isenabled_s)
{
    /* Check parameter validity */
    NU_CHK_PARAM_VAL(0, 4);

    // To access (RTC) registers, clock must be enabled first.
    // For TZ, with RTC being secure, we needn't call the secure gateway versions.
    CLK_EnableModuleClock(RTC_MODULE);

    CLK_SetModuleClock(RTC_MODULE, RTC_LXTCTL_RTCCKSEL_LXT, 0);

    // NOTE: Check RTC Init Active flag to support crossing reset cycle.
    int32_t enabled = !! (RTC_S->INIT & RTC_INIT_ACTIVE_Msk);
    nu_set32_be(out_vec->base, enabled);

    return TFM_PLATFORM_ERR_SUCCESS;
}
#endif
