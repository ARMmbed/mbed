/* mbed Microcontroller Library
 * Copyright (c) 2018-2019 ARM Limited
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

#ifndef MBED_VERSION_H
#define MBED_VERSION_H

/** \addtogroup platform-public-api */
/** @{*/

/**
 * \defgroup platform_version Version macros
 * @{
 */

/** MBED_MAJOR_VERSION
  * Mbed OS major version
  *
  * @note 99 is default value for development version (master branch)
  */
#define MBED_MAJOR_VERSION 6

/** MBED_MINOR_VERSION
  * Mbed OS minor version
  *
  * @note 99 is default value for development version (master branch)
  */
#define MBED_MINOR_VERSION 12

/** MBED_PATCH_VERSION
  * Mbed OS patch version
  *
  * @note 99 is default value for development version (master branch)
  */
#define MBED_PATCH_VERSION 0

#define MBED_ENCODE_VERSION(major, minor, patch) ((major)*10000 + (minor)*100 + (patch))

/** MBED_VERSION
  * Mbed OS 5 version (MMmmpp - M(Major); m(minor); p(patch))
  *
  * @note 999999 is default value for development version (master branch)
  */
#define MBED_VERSION MBED_ENCODE_VERSION(MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION)

/** MBED_VERSION_CHECK
  * Macro can be used to check minimum Mbed OS version required for feature/library. If current version
  * is less than required, it will assert.
  *
  * @note: Version of master branch will be 999999 as default, hence no assert/warning is provided for
  * master branch code
  */
#define MBED_VERSION_CHECK(major, minor, patch) do { \
                                       static_assert((MBED_VERSION >= MBED_ENCODE_VERSION((major),(minor),(patch))), "Incompatible mbed-os version detected!!"); \
                                     } while(0)

#endif

/** @}*/
/** @}*/
