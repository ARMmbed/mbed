/* Copyright (c) 2019 ARM Limited
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

#ifndef __CTHUNK_H__
#define __CTHUNK_H__

/**
 * Class for created a pointer with data bound to it
 *
 * @note Synchronization level: Not protected
 */
template<class T>
class CThunk {
public:
    typedef void (T::*CCallbackSimple)(void);
    typedef void (T::*CCallback)(void *context);
    CThunk()
    {}
    CThunk(T *instance)
    {}
    CThunk(T &instance)
    {}
    void callback(CCallback callback)
    {}

    void callback(CCallbackSimple callback)
    {
    }
    uint32_t entry(void)
    {
        
        return 0;
    }
};


#endif/*__CTHUNK_H__*/


