/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
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
#ifndef MSTD_NEW_
#define MSTD_NEW_

/* <mstd_new>
 *
 * - includes toolchain's <new>
 * - For all toolchains, C++17 backports:
 *   - mstd::launder
 */

#include <new>
#if __cpp_lib_launder < 201606
#include <type_traits>
#endif

namespace mstd
{
using std::nothrow_t;
using std::nothrow;
using std::new_handler;
using std::set_new_handler;

#if __cpp_lib_launder >= 201606
using std::launder;
#else
template <typename T>
constexpr T *launder(T *p) noexcept
{
    static_assert(!std::is_function<T>::value && !std::is_void<T>::value, "Can only launder complete object types");
#if defined __clang__ || __GNUC__ >= 9
    return __builtin_launder(p);
#else
    return p;
#endif
}
#endif

} // namespace mstd

#endif // MSTD_NEW_
