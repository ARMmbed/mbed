/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
	#define MBED_CONF_NORDIC_NRF_LF_CLOCK_SRC		(NRF_LF_SRC_RC)
	#define MBED_CONF_NORDIC_NRF_LF_CLOCK_CALIB_TIMER_INTERVAL	16
	#define MBED_CONF_NORDIC_NRF_LF_CLOCK_CALIB_MODE_CONFIG	1
void mbed_sdk_init()
{
	printf("", __TIME__, __DATE__);	
	
}
