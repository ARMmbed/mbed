/*
 * Copyright (c) 2021 ARM Limited
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

#ifndef MSG_HEADER_H
#define MSG_HEADER_H

#include "netsocket/nsapi_types.h"

struct MsgHeaderIterator {
    MsgHeaderIterator(nsapi_msghdr_t *hdr, nsapi_size_t size) :
        start(hdr),
        current(nullptr),
        size(size)
    {}

    bool has_next()
    {
        if (current == nullptr) {
            if (start != nullptr) {
                return true;
            } else {
                return false;
            }
        }

        if (current->len < sizeof(*current)) {
            return false;
        }

        if ((reinterpret_cast<uint8_t *>(current) + current->len) >= (reinterpret_cast<uint8_t *>(start) + size)) {
            return false;
        }

        return true;
    }

    nsapi_msghdr_t *next()
    {
        if (!has_next()) {
            return nullptr;
        }

        if (current == nullptr) {
            current = start;
        } else {
            current = reinterpret_cast<nsapi_msghdr_t *>(reinterpret_cast<uint8_t *>(current) + current->len);
        }

        return current;
    }

private:
    nsapi_msghdr_t *start;
    nsapi_msghdr_t *current;
    nsapi_size_t size;
};


#endif

