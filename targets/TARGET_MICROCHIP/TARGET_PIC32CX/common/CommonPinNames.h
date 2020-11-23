/***************************************************************************//**
 * @file CommonPinNames.h
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/
#ifndef MBED_COMMONPINNAMES_H
#define MBED_COMMONPINNAMES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIN_INPUT = 0,
    PIN_OUTPUT = 1
} PinDirection;

typedef enum {
	/* PIC32CX pin modes */
    InputPullDefault = 0,
	InputPullUp   = (1 << 3),
    InputPullDown = (1 << 4),
	InputOpenDrain = (1 << 5),
	InputGlitchFilter = (1 << 6),
	InputDebounce = (1 << 7),

	/* mbed modes:
     * PullUp, PullDown, PullNone, OpenDrain
     *
     * mbed default digital input mode:
     * PullDefault
     *
     * mbed default digital output mode:
     * PullNone
     */
    PullNone = InputPullDefault,
    PullDown = InputPullDown,
    PullUp   = InputPullUp,
	OpenDrain = InputOpenDrain,
    PullDefault = PullUp
} PinMode;

#ifdef __cplusplus
}
#endif

#endif
