/*!
 * @file      lr1110_driver_version.h
 *
 * @brief     Placeholder to keep the version of LR1110 driver.
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LR1110_DRIVER_VERSION_H__
#define __LR1110_DRIVER_VERSION_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LR1110_DRIVER_VERSION_MAJOR 3
#define LR1110_DRIVER_VERSION_MINOR 0
#define LR1110_DRIVER_VERSION_PATCH 0

/*!
 * @brief Compare version information with current ones
 *
 * This macro expands to true boolean value if the version information provided in argument is compatible or
 * retro-compatible with the version of this code base
 */
#define LR1110_DRIVER_VERSION_CHECK( x, y, z ) \
    ( x == LR1110_DRIVER_VERSION_MAJOR &&      \
      ( y < LR1110_DRIVER_VERSION_MINOR ||     \
        ( y == LR1110_DRIVER_VERSION_MINOR && z <= LR1110_DRIVER_VERSION_PATCH ) ) )

const char* lr1110_driver_version_get_version_string( void );

#ifdef __cplusplus
}
#endif

#endif
