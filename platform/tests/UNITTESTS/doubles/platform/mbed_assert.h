
/** \addtogroup platform */
/** @{*/
/**
 * \defgroup platform_Assert Assert macros
 * @{
 */
/*
 * Copyright (c) 2018--2019 Arm Limited and affiliates.
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
#ifndef MBED_ASSERT_H
#define MBED_ASSERT_H

#include "mbed_preprocessor.h"
#include "mbed_toolchain.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Internal mbed assert function which is invoked when MBED_ASSERT macro fails.
 *  This function is active only if NDEBUG is not defined prior to including this
 *  assert header file.
 *  In case of MBED_ASSERT failing condition, error() is called with the assertation message.
 *  @param expr Expression to be checked.
 *  @param file File where assertation failed.
 *  @param line Failing assertation line number.
 */
// mbed_assert_internal UT stub only prints an assert trace and returns, so therefore
// MBED_NORETURN must not be defined for UTs.
MBED_NORETURN
void mbed_assert_internal(const char *expr, const char *file, int line);

#ifdef __cplusplus
}
#endif

/**
 * For unittests: Global flag to select whether MBED_ASSERT
 * throws error. Default false.
 */
extern bool mbed_assert_throw_errors;

/** MBED_ASSERT
 *  Declare runtime assertions: results in runtime error if condition is false
 *
 *  @note
 *  Use of MBED_ASSERT is limited to Debug and Develop builds.
 *
 *  @code
 *
 *  int Configure(serial_t *obj) {
 *      MBED_ASSERT(obj);
 *  }
 *  @endcode
 */
#define MBED_ASSERT(expr)                                \
do {                                                     \
    if (!(expr)) {                                       \
        mbed_assert_internal(#expr, __FILE__, __LINE__); \
    }                                                    \
} while (0)

/** MBED_STATIC_ASSERT
 *  Declare compile-time assertions, results in compile-time error if condition is false
 *
 *  The assertion acts as a declaration that can be placed at file scope, in a
 *  code block (except after a label), or as a member of a C++ class/struct/union.
 *
 *  @note
 *  Use of MBED_STATIC_ASSERT as a member of a struct/union is limited:
 *  - In C++, MBED_STATIC_ASSERT is valid in class/struct/union scope.
 *  - In C, MBED_STATIC_ASSERT is not valid in struct/union scope, and
 *    MBED_STRUCT_STATIC_ASSERT is provided as an alternative that is valid
 *    in C and C++ class/struct/union scope.
 *
 *  @code
 *  MBED_STATIC_ASSERT(MBED_LIBRARY_VERSION >= 120,
 *          "The mbed library must be at least version 120");
 *
 *  int main() {
 *      MBED_STATIC_ASSERT(sizeof(int) >= sizeof(char),
 *              "An int must be larger than a char");
 *  }
 *  @endcode
 */
#if defined(__cplusplus) && (__cplusplus >= 201103L || __cpp_static_assert >= 200410L)
#define MBED_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#elif !defined(__cplusplus) && __STDC_VERSION__ >= 201112L
#define MBED_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#elif defined(__cplusplus) && defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) \
    && (__GNUC__*100 + __GNUC_MINOR__) > 403L
#define MBED_STATIC_ASSERT(expr, msg) __extension__ static_assert(expr, msg)
#elif !defined(__cplusplus) && defined(__GNUC__) \
    && (__GNUC__*100 + __GNUC_MINOR__) > 406L
#define MBED_STATIC_ASSERT(expr, msg) __extension__ _Static_assert(expr, msg)
#elif defined(__ICCARM__)
#define MBED_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#else
#define MBED_STATIC_ASSERT(expr, msg) \
    enum {MBED_CONCAT(MBED_ASSERTION_AT_, __LINE__) = sizeof(char[(expr) ? 1 : -1])}
#endif

/** MBED_STRUCT_STATIC_ASSERT
 *  Declare compile-time assertions, results in compile-time error if condition is false
 *
 *  Unlike MBED_STATIC_ASSERT, MBED_STRUCT_STATIC_ASSERT can and must be used
 *  as a member of a C/C++ class/struct/union.
 *
 *  @code
 *  struct thing {
 *      MBED_STATIC_ASSERT(2 + 2 == 4,
 *              "Hopefully the universe is mathematically consistent");
 *  };
 *  @endcode
 */
#define MBED_STRUCT_STATIC_ASSERT(expr, msg) int : (expr) ? 0 : -1


#endif

/**@}*/

/**@}*/
