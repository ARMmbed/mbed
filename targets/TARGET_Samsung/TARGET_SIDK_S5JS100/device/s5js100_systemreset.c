/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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
/* @file  : s5js100_systemreset.c
 * @brief : board reset source file
 * @date  : June 2019
 *
 * @note  : Add chip dependent feature and more detail error status
 *
 */

/****************************************************************************
  Included Files
 ****************************************************************************/
#include "cmsis.h"
#include "mbed_interface.h"
#include "mbed_power_mgmt.h"
#include "mbed_stats.h"
#include "mbed_atomic.h"
#include "sflash_api.h"
#include "rtx_os.h"
#include <inttypes.h>
#include <string.h>


/****************************************************************************
 * Name: up_systemreset
 *
 * Description:
 *   Internal, reset logic.
 *
 ****************************************************************************/
void up_systemreset(void)
{
#if DEVICE_RTC
    printf("\r\n reset..now\r\n");
    char rtc_string[100] = {0};
    up_progmem_erasepage(S5JS100_RTC_RET_OFFSET / 256);
    sprintf(rtc_string, "%d", rtc_read());
    up_progmem_write(S5JS100_RTC_RET_OFFSET + S5JS100_FLASH_PADDR, rtc_string, sizeof(rtc_string));
#endif

    putreg32(0x1, 0x8301100C);
    putreg32(0x1 << 1, 0x82020018);

    putreg32(0x4, 0x83011000); // enable watchdog
    putreg32(0x1, 0x83011010);
    putreg32(0x1, 0x83011020);
    putreg32(327, 0x83011004);  //set 10ms to be reset , 1 sec=32768
    putreg32(0xFF, 0x83011008); // force to load value to be reset

    /* Wait for the reset */
    for (; ;) {
        printf("\r\n...");
    }
}

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   Reset board.  This function may or may not be supported by a
 *   particular board architecture.
 *
 * Input Parameters:
 *   status - Status information provided with the reset event.  This
 *     meaning of this status information is board-specific.  If not used by
 *     a board, the value zero may be provided in calls to board_reset.
 *
 * Returned Value:
 *   If this function returns, then it was not possible to power-off the
 *   board due to some constraints.  The return value int this case is a
 *   board-specific reason for the failure to shutdown.
 *
 ****************************************************************************/
int board_reset(void)
{
    up_systemreset();
    return 0;
}

