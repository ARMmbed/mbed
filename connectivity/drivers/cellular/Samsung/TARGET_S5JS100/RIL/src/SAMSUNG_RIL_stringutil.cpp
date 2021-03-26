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

#include <string.h>
#include "SAMSUNG_RIL_stringutil.h"
#include "SAMSUNG_RIL_constdef.h"

inline bool isHexChar(int ch)
{
    switch (ch) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return true;
    }
    return false;
}

int StringUtil::hexToVal(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}

int StringUtil::hexStringToVal(unsigned char *pHexDecOut, const char *pszHexStrIn)
{
    int nResult = 0;

    if (pszHexStrIn == NULL) {
        return 0;
    }

    int nLength = strlen(pszHexStrIn);
    if (pHexDecOut && pszHexStrIn) {
        int nSrcIdx, nDstIdx;
        for (nSrcIdx = 0, nDstIdx = 0; nSrcIdx < nLength && pszHexStrIn[nSrcIdx] != '\0'; nSrcIdx++) {
            // Even index is upper 4 bit
            if (nSrcIdx == 0 || nSrcIdx % 2 == 0) {
                pHexDecOut[nDstIdx] = ((hexToVal(pszHexStrIn[nSrcIdx]) << 4) & 0xF0);
            } else {
                pHexDecOut[nDstIdx] |= (hexToVal(pszHexStrIn[nSrcIdx]) & 0x0F);
                nDstIdx++;
            }
        }

        // Odd Length of Source Data
        if (nSrcIdx != 0 && nSrcIdx % 2 == 1) {
            nDstIdx++;
        }
        nResult = nDstIdx;
    }

    return nResult;
}

bool StringUtil::isEmpty(const char *str)
{
    return (str == NULL || *str == 0);
}

bool StringUtil::equals(const char *l, const char *r)
{
    if (isEmpty(l) || isEmpty(r)) {
        return false;
    }

    return strcmp(l, r) == 0;
}

int StringUtil::convertBytesToHexString(char *buf, int size, char *data, int datalen)
{
    if (buf == NULL || data == NULL) {
        return -1;
    }

    if (size <= 0 || datalen <= 0) {
        return -1;
    }

    if (size < (datalen * 2) + 1) {
        return -1;
    }

    static char val[] = "0123456789ABCDEF";
    char *p = buf;

    for (int i = 0; i < datalen; i++) {
        *(p++) = val[(data[i] >> 4) & 0xF];
        *(p++) = val[data[i] & 0xF];
    } // end for i ~
    *p = 0;
    return (p - buf);
}

int StringUtil::convertHexStringToBytes(char *buf, int size, char *data, int datalen)
{
    if (buf == NULL || data == NULL) {
        return -1;
    }

    if (size <= 0 || datalen <= 0) {
        return -1;
    }

    if (datalen % 2 != 0 || size < datalen / 2) {
        return -1;
    }

    char *p = buf;
    for (int i = 0; i < datalen; i += 2) {
        if (!isHexChar(data[i]) || !isHexChar(data[i + 1])) {
            return -1;
        }
        int val[2];
        val[0] = hexToVal(data[i]);
        val[1] = hexToVal(data[i + 1]);
        *(p++) = (val[0] << 4 | val[1]) & 0xFF;
    } // end for i ~

    return (p - buf);
}

int SmsUtil::convertSmscBcdToNumber(const char *tpdu, int tpduLen, char *smsc, int smscLen)
{
    if (tpduLen <= 0) {
        return -1;
    }

    int len = (tpduLen - 1) * 2;
    char buf[MAX_GSM_SMS_SERVICE_CENTER_ADDR * 2] = {0, };

    //Check '+'
    int i = 0, j = 1;
    if (tpdu[j] == RIL_TOA_INTERNATIONAL) {
        buf[i] = '+';
        i++;
        j++;
        len--;
    } else if (tpdu[j] == RIL_TOA_UNKNOWN) {
        // TOA: National Numbering skip
        j++;
        len--;
    }

    //Convert BCD -> ASCII
    for (; j < tpduLen; i++, j++) {
        buf[i] = (tpdu[j] & 0x0F) + '0';
        if ((tpdu[j] >> 4) == 0x0F) {
            // odd count smsc
            len--;
            break;
        } else {
            buf[++i] = ((tpdu[j] >> 4) & 0x0F) + '0';
        }
    }

    if (len > smscLen) {
        return -1;
    }

    strncpy(smsc, buf, len);
    return len;
}

