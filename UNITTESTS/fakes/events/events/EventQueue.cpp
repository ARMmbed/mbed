/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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

#include "EventQueue.h"

namespace events {

handle_t EventQueue::call_handler(function_t handler)
{
    return call_handler_in(0, handler);
}

handle_t EventQueue::call_handler_in(tick_t ms, function_t handler)
{
    _handler_id++;

    _handlers.push_back(
        internal_event{
            std::unique_ptr<function_t>(new function_t(handler)),
            _now + ms,
            _handler_id
        }
    );

    return _handler_id;
}

bool EventQueue::cancel_handler(handle_t handle)
{
    if (!handle) {
        return false;
    }

    auto found = std::remove_if(
        _handlers.begin(),
        _handlers.end(),
        [handle](internal_event& element) -> bool {
            return (handle == element.handle);
        }
    );

    if (found != _handlers.end()) {
        _handlers.erase(
            found,
            _handlers.end()
        );
        return true;
    }

    return false;
}

void EventQueue::process_events(tick_t duration_ms)
{
    // execute all events during the duration
    for (uint64_t i = 0; i < duration_ms; ++i) {
        process_events();
        _now++;
    }

    // last round to execute immediate events
    process_events();
}

void EventQueue::process_events() {
    while (true) {
        if (_handlers.empty()) {
            return;
        }

        /* to guarantee order we only dispatch one tick at a time*/
        auto smallest = std::min_element(
            _handlers.begin(),
            _handlers.end(),
            [](internal_event& element, internal_event& smallest){
                return (element.tick < smallest.tick);
            }
        );
        tick_t earliest_tick = smallest->tick;

        /* stop if all elements happen later */
        if (earliest_tick > _now) {
            return;
        }

        /* dispatch all handlers that happen at this time */
        auto found = std::remove_if(
            _handlers.begin(),
            _handlers.end(),
            [earliest_tick](internal_event& element) -> bool {
                if (earliest_tick >= element.tick) {
                    (*(element.handler))();
                    return true;
                } else {
                    return false;
                }
            }
        );

        if (found != _handlers.end()) {
            _handlers.erase(found, _handlers.end());
        }
    }
}

}
