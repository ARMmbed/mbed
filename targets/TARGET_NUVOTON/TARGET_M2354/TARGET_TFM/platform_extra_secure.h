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

#ifndef PLATFORM_EXTRA_SECURE_H
#define PLATFORM_EXTRA_SECURE_H

#include <stdint.h>
#include <limits.h>

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
typedef void *      S_RTC_TIME_DATA_T_PTR;
#elif NU_TFM_PLAT_IOCTL_S
#include "tfm_platform_system.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: This header file is designed to be across Mbed OS and TF-M. */

/* Helper to define request code with platform extra secure function */
#define NU_PLAT_XTRA_SEC_REQ(SEC_FUNC)      \
    NU_PLAT_XTRA_SEC_REQ_##SEC_FUNC

/* Table of request code with platform extra secure function */
typedef enum {
    /* Prefix with "SYS" */
    NU_PLAT_XTRA_SEC_REQ(SYS_ResetModule_S)              = 0,
    NU_PLAT_XTRA_SEC_REQ(SYS_LockReg_S),
    NU_PLAT_XTRA_SEC_REQ(SYS_UnlockReg_S),

    /* Prefix with "CLK" */
    NU_PLAT_XTRA_SEC_REQ(CLK_SetModuleClock_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_EnableModuleClock_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_DisableModuleClock_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_Idle_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_PowerDown_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetHXTFreq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetLXTFreq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetHCLKFreq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetPCLK0Freq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetPCLK1Freq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetCPUFreq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetPLLClockFreq_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetModuleClockSource_S),
    NU_PLAT_XTRA_SEC_REQ(CLK_GetModuleClockDivider_S),

    /* Prefix with "RTC" */
    NU_PLAT_XTRA_SEC_REQ(RTC_Open_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_Close_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_WaitAccessEnable_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_GetDateAndTime_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_GetAlarmDateAndTime_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_SetDateAndTime_S),
    NU_PLAT_XTRA_SEC_REQ(RTC_SetAlarmDateAndTime_S),

    /* Prefix with "nu" */
    NU_PLAT_XTRA_SEC_REQ(nu_pin_function_s),
    NU_PLAT_XTRA_SEC_REQ(nu_idle_s),
    NU_PLAT_XTRA_SEC_REQ(nu_powerdown_s),
    NU_PLAT_XTRA_SEC_REQ(nu_rtc_read_spare_register_s),
    NU_PLAT_XTRA_SEC_REQ(nu_rtc_write_spare_register_s),
    NU_PLAT_XTRA_SEC_REQ(nu_rtc_isenabled_s),

    /* Max request code, plays as number of valid request code */
    NU_PLAT_XTRA_SEC_REQ(MAX),

    /* Following entry is only to ensure the error code of int size */
    NU_PLAT_XTRA_SEC_REQ(INT_SIZE) = INT_MAX
} nu_plat_req_t;

#if NU_TFM_PLAT_IOCTL_S

#define NU_PLAT_XTRA_SEC_HDLR(SEC_FUN)             \
    enum tfm_platform_err_t SEC_FUN(psa_invec *in_vec, psa_outvec *out_vec)

#endif

/* Secure SYS_ResetModule
 *
 * Guard access to secure module from non-secure domain before SYS_ResetModule.
 * Its synopsis is the same as SYS_ResetModule.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_ResetModule_S(uint32_t u32ModuleIndex);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_ResetModule_S);
#endif

/* Secure SYS_LockReg
 *
 * Guard access to secure module from non-secure domain before SYS_LockReg.
 * Its synopsis is the same as SYS_LockReg.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_LockReg_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_LockReg_S);
#endif

/* Secure SYS_UnlockReg
 *
 * Guard access to secure module from non-secure domain before SYS_UnlockReg.
 * Its synopsis is the same as SYS_UnlockReg.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void SYS_UnlockReg_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(SYS_UnlockReg_S);
#endif

/* Secure CLK_SetModuleClock
 *
 * Guard access to secure module from non-secure domain before CLK_SetModuleClock.
 * Its synopsis is the same as CLK_SetModuleClock.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_SetModuleClock_S(uint32_t u32ModuleIndex, uint32_t u32ClkSrc, uint32_t u32ClkDiv);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_SetModuleClock_S);
#endif

/* Secure CLK_EnableModuleClock
 *
 * Guard access to secure module from non-secure domain before CLK_EnableModuleClock.
 * Its synopsis is the same as CLK_EnableModuleClock.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_EnableModuleClock_S(uint32_t u32ModuleIndex);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_EnableModuleClock_S);
#endif

/* Secure CLK_DisableModuleClock
 *
 * Guard access to secure module from non-secure domain before CLK_DisableModuleClock.
 * Its synopsis is the same as CLK_DisableModuleClock.
 */

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_DisableModuleClock_S(uint32_t u32ModuleIndex);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_DisableModuleClock_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_Idle_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_Idle_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void CLK_PowerDown_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_PowerDown_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetHXTFreq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetHXTFreq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetLXTFreq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetLXTFreq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetHCLKFreq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetHCLKFreq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPCLK0Freq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPCLK0Freq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPCLK1Freq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPCLK1Freq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetCPUFreq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetCPUFreq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetPLLClockFreq_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetPLLClockFreq_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetModuleClockSource_S(uint32_t u32ModuleIndex);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetModuleClockSource_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t CLK_GetModuleClockDivider_S(uint32_t u32ModuleIndex);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(CLK_GetModuleClockDivider_S);
#endif

/* NOTE: time_t
 *
 * time_t may be defined to int32_t/int64_t dependent on compilers. For binary-compatible
 * among compilers, we must replace time_t with int32_t/int64_t explicitly in rtc secure
 * functions. We determine to choose int64_t rather than int32_t to avoid 'Year 2038 problem'.
 */

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_Open_S(S_RTC_TIME_DATA_T_PTR sPt);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_Open_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_Close_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_Close_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_WaitAccessEnable_S(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_WaitAccessEnable_S);

__STATIC_INLINE void RTC_WaitAccessEnable(void)
{
    /* On M2354, RTC_WaitAccessEnable() is unnecessary and is not provided by BSP.
     * Provide a dummy one to make code consistent. */
}
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_GetDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_GetDateAndTime_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_GetAlarmDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_GetAlarmDateAndTime_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_SetDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_SetDateAndTime_S);
#endif

#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void RTC_SetAlarmDateAndTime_S(S_RTC_TIME_DATA_T_PTR sPt);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(RTC_SetAlarmDateAndTime_S);
#endif

/* Secure pin_function
 *
 * Guard access to secure GPIO from non-secure domain.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_pin_function_s(uint32_t port_index, uint32_t pin_index, uint32_t data);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_pin_function_s);
#endif

/* Secure sequence of SYS_UnlockReg > CLK_Idle > SYS_LockReg */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_idle_s(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_idle_s);
#endif

/* Secure sequence of SYS_UnlockReg > CLK_PowerDown > SYS_LockReg */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_powerdown_s(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_powerdown_s);
#endif

/* Read RTC spare register
 *
 * Secure sequence:
 * RTC_WaitAccessEnable();
 * RTC_EnableSpareAccess();
 * RTC_WaitAccessEnable();
 * RTC_READ_SPARE_REGISTER(RTC, reg_num);
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
uint32_t nu_rtc_read_spare_register_s(uint32_t reg_num);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_read_spare_register_s);
#endif

/* Write RTC spare register
 *
 * Secure sequence:
 * RTC_WaitAccessEnable();
 * RTC_EnableSpareAccess();
 * RTC_WaitAccessEnable();
 * RTC_WRITE_SPARE_REGISTER(RTC, reg_num, reg_val);
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
void nu_rtc_write_spare_register_s(uint32_t reg_num, uint32_t reg_val);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_write_spare_register_s);
#endif

/* Secure rtc_isenabled
 *
 * Its synopsis is the same as normal version except change of return/argument type for
 * binary-compatible across compilers.
 */
#if __MBED__ || NU_TFM_PLAT_IOCTL_NS
int32_t nu_rtc_isenabled_s(void);
#elif NU_TFM_PLAT_IOCTL_S
NU_PLAT_XTRA_SEC_HDLR(nu_rtc_isenabled_s);
#endif

#ifdef __cplusplus
}
#endif

#endif
