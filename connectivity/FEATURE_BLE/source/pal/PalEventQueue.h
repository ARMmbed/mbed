/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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

#ifndef BLE_PAL_EVENT_QUEUE_H_
#define BLE_PAL_EVENT_QUEUE_H_

#include "platform/Callback.h"

namespace ble {

/**
 * Simple interface which allow upper layer to post an event into the event
 * mechanism of the abstracted stack.
 *
 * When an event is posted then it's availability shall be signaled to the upper
 * layer in order to inform the upper layer that events are ready to be
 * processed.
 *
 * Implementation can realize that operation by calling the function
 * signalEventsToProcess of their implementation of BLEInstanceBase.
 *
 * Events in the queue shall be processed at the next invocation of
 * BLEInstanceBase::processEvents.
 */
class PalEventQueue {
public:
    /**
     * Post an event into the event queue.
     *
     * @param event The event to store in the queue, it shall be processed at
     * the next invocation of BLE::processEvent.
     *
     * @return true in case of success and false otherwise
     *
     * @attention Event availability shall be signaled to the upper layer and
     * the event queue shall be processed at the next invocation of
     * BLEInstanceBase::process
     */
    virtual bool post(const mbed::Callback<void()>& event) = 0;

    /** Remove all pending events.
     */
    virtual void clear() = 0;
};

} // namespace ble

#endif /* BLE_PAL_EVENT_QUEUE_H_ */
