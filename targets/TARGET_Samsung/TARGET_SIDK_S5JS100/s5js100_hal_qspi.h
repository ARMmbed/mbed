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
// Flash type used
#define FLASH_OTHERS 0
#define FLASH_MXIC 1
#define FLASH_MXIC_4IO 2
#define FLASH_WINBOND 3
#define FLASH_MICRON 4
#define FLASH_EON 5
#define FLASH_GD 6
#define FLASH_CYPRESS 7

/*Common command*/
#define FLASH_CMD_WREN      0x06            //write enable
#define FLASH_CMD_WRDI      0x04            //write disable
#define FLASH_CMD_WRSR      0x01            //write status register
#define FLASH_CMD_RDID      0x9F            //read idenfication
#define FLASH_CMD_RDUID     0x4B          //Read Unique ID
#define FLASH_CMD_RDSR      0x05            //read status register
#define FLASH_CMD_RDSFDP    0x5A            //Read SFDP
#define FLASH_CMD_READ      0x03            //read data
#define FLASH_CMD_FREAD     0x0B            //fast read data
#define FLASH_CMD_PP        0x02            //Page Program
#define FLASH_CMD_DREAD     0x3B            //Double Output Mode command 1-1-2
#define FLASH_CMD_2READ     0xBB            // 2 x I/O read  command 1-2-2
#define FLASH_CMD_QREAD     0x6B            // 1I / 4O read command 1-1-4
#define FLASH_CMD_4READ     0xEB            // 4 x I/O read  command 1-4-4
#define FLASH_CMD_DPP       0xA2            // 1-1-2
#define FLASH_CMD_2PP       0xD2            //  1-2-2
#define FLASH_CMD_QPP       0x32            //  1-1-4
#define FLASH_CMD_4PP       0x38            //quad page program 1-4-4
#define FLASH_CMD_SE        0x20            //Sector Erase
#define FLASH_CMD_BE        0xD8            //Block Erase(or 0x52)
#define FLASH_CMD_CE        0xC7            //Chip Erase(or 0xC7)
#define FLASH_CMD_DP        0xB9            //Deep Power Down
#define FLASH_CMD_RDP       0xAB            //Release from Deep Power-Down


