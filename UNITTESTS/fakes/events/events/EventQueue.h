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

#ifndef EVENTQUEUE_FAKE_H
#define EVENTQUEUE_FAKE_H

#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <vector>
#include "events/EventQueue.h"
#include <chrono>
#include <mstd_tuple>

namespace events {

typedef int handle_t;
typedef std::function<void()> function_t;
typedef unsigned tick_t;

class EventQueue {
    using duration = std::chrono::duration<int, std::milli>;

public:
    EventQueue(unsigned size = 0, unsigned char *buffer = NULL) { delete buffer; };

    ~EventQueue() { };

    /** This will advance time by given amount of milliseconds and then dispatch all events that were set to happen in that time.
     *
     * @param ms number of miliseconds to advance time
     */
    void dispatch(int milliseconds = -1) {
        if (milliseconds > 0) {
            process_events(milliseconds);
        } else {
            _now = (tick_t)-1;
            process_events();
            _now = 0;
        }
    };

    tick_t tick() {
        return _now;
    };

    bool cancel(handle_t id) {
        return cancel_handler(id);
    };

    /** Get number of events in queue.
     *
     * @return Number of events waiting in the queue.
     */
    size_t size() const {
        return _handlers.size();
    }

    template<typename F, typename ... ArgTs>
    handle_t call(F f, ArgTs... args) {
        return call_handler(
            [f, args = mstd::make_tuple(args...)]() {
                mstd::apply(f, args);
            }
        );
    }

    template<typename F, typename ... ArgTs>
    handle_t call_in(duration ms, F f, ArgTs... args) {
        return call_handler_in(
            ms.count(),
            [f, args = mstd::make_tuple(args...)]() {
                mstd::apply(f, args);
            }
        );
    }

    template <typename T, typename R, typename... ArgTs>
    int call(T *obj, R(T::*method)(ArgTs...), ArgTs... args) {
        return call_handler(
            [obj, method, args = mstd::make_tuple(args...)]() {
                mstd::apply(method, obj, args);
            }
        );
    }

    template <typename T, typename R, typename... ArgTs>
    int call_in(duration ms, T *obj, R(T::*method)(ArgTs...), ArgTs... args) {
        return call_handler_in(
            ms.count(),
            [obj, method, args = mstd::make_tuple(args...)]() {
                mstd::apply(method, obj, args);
            }
        );
    }

private:
    handle_t call_handler(function_t handler);

    handle_t call_handler_in(tick_t ms, function_t handler);

    bool cancel_handler(handle_t handle);

    void process_events(tick_t duration_ms);

    void process_events();

private:
    struct internal_event {
        std::unique_ptr<function_t> handler;
        tick_t tick;
        handle_t handle;
    };

    std::vector<internal_event> _handlers;
    tick_t _now = 0;
    handle_t _handler_id = 0;
};

}

#endif //EVENTQUEUE_FAKE_H
