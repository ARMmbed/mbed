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

#include "cmsis.h"
#include "s5js100_pmip.h"

extern void s5js100_modem_start(void);
extern void sflash_os_env_parser(void);
void mbed_sdk_init(void)
{
    volatile unsigned int *wdtsfr = (unsigned int *)(0x83011000);
    *wdtsfr = *wdtsfr & ~0x4;

    /* GPIO 0, 1 ouput for LED */
    modifyreg32(0x85026100, 0x3, 0x1);
    modifyreg32(0x85026000, 0x3, 0x0);

    SystemPowerConfig();

    s5js100_pmip_initialize();

    /* Config EFlash Controller Clock */
    SFlash_DriverInitialize();

    sflash_os_env_parser();
}

void mbed_main(void)
{
#if DEVICE_RTC
    rtc_restore();
#endif
}
