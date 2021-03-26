/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef __SAMSUNG_RIL_STRING_UTIL_H__
#define __SAMSUNG_RIL_STRING_UTIL_H__

class StringUtil {
public:
    static int hexToVal(char ch);
    static int hexStringToVal(unsigned char *pHexDecOut, const char *pszHexStrIn);
    static bool isEmpty(const char *str);
    static bool equals(const char *l, const char *r);
    static int convertBytesToHexString(char *buf, int size, char *data, int datalen);
    static int convertHexStringToBytes(char *buf, int size, char *data, int datalen);
};

class SmsUtil {
public:
    static int convertSmscBcdToNumber(const char *tpdu, int tpduLen, char *smsc, int smscLen);
};

#endif // __STRING_UTIL_H__

