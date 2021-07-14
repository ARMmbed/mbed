/*
 * Copyright (c) , Arm Limited and affiliates.
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
#include "CellularUtil.h"
#include <string.h>
#include <stdlib.h>


#include "randLIB.h"
#define RANDOM_PORT_NUMBER_START 49152
#define RANDOM_PORT_NUMBER_END 65535
#define RANDOM_PORT_NUMBER_COUNT (RANDOM_PORT_NUMBER_END - RANDOM_PORT_NUMBER_START + 1)
#define RANDOM_PORT_NUMBER_MAX_STEP 100

namespace mbed_cellular_util {

nsapi_version_t convert_ipv6(char *ip)
{
    if (!ip) {
        return NSAPI_UNSPEC;
    }

    int len = strlen(ip);
    int pos = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (ip[i] == '.') {
            pos++;
        }
        if (pos > 3) {
            break;
        }
    }

    // more that 3 periods mean that it was ipv6 but in format of a1.a2.a3.a4.a5.a6.a7.a8.a9.a10.a11.a12.a13.a14.a15.a16
    // we need to convert it to hexadecimal format separated with colons
    if (pos == 3) {

        return NSAPI_IPv4;

    } else if (pos > 3) {
        pos = 0;
        int ip_pos = 0;
        char b;
        bool set_colon = false;
        for (i = 0; i < len; i++) {
            if (ip[i] == '.') {
                b = (char)strtol(ip + ip_pos, NULL, 10); // convert to char to int so we can change it to hex string
                pos += char_str_to_hex_str(&b, 1, ip + pos, !set_colon); // omit leading zeroes with using set_colon flag
                if (set_colon) {
                    ip[pos++] = ':';
                    set_colon = false;
                } else {
                    set_colon = true;
                }
                ip_pos = i + 1; // skip the '.'
            }

            // handle the last part which does not end with '.' but '\0'
            if (i == len - 1) {
                b = (char)strtol(ip + ip_pos, NULL, 10);
                pos += char_str_to_hex_str(&b, 1, ip + pos, !set_colon);
                ip[pos] = '\0';
            }
        }

        return NSAPI_IPv6;
    }

    return NSAPI_UNSPEC;
}

// For example "32.1.13.184.0.0.205.48.0.0.0.0.0.0.0.0"
void separate_ip4like_addresses(char *orig, char *ip, size_t ip_size, char *ip2, size_t ip2_size)
{
    // ipv4-like notation
    int len = strlen(orig);
    int count = 0, i = 0, pos = 0;
    char *temp;

    for (; i < len; i++) {
        if (orig[i] == '.') {
            count++;
            if (count == 4) {
                pos = i;
            } else if (count == 16) {
                pos = i;
            }
        }
    }

    if (count == 3) { // normal ipv4, copy to ip
        if (ip_size > strlen(orig)) {
            memcpy(ip, orig, strlen(orig));
            ip[strlen(orig)] = '\0';
        } else {
            ip[0] = '\0';
        }
        if (ip2) {
            ip2[0] = '\0';
        }
    } else if (count == 7) { // ipv4 and subnet mask. Need to separate those.
        temp = &orig[pos];
        if ((uint8_t)ip_size > temp - orig) {
            memcpy(ip, orig, temp - orig);
            ip[temp - orig] = '\0';
        }
        temp++; // skip the '.'
        if (ip2 && (ip2_size > strlen(temp))) {
            memcpy(ip2, temp, strlen(temp));
            ip2[strlen(temp)] = '\0';
        }
    } else if (count == 15) { // only one ipv6 address in ipv4-like notation
        if (ip_size > strlen(orig)) {
            memcpy(ip, orig, strlen(orig));
            ip[strlen(orig)] = '\0';
            convert_ipv6(ip);
        } else {
            ip[0] = '\0';
        }
        if (ip2) {
            ip2[0] = '\0';
        }
    } else if (count == 31) { // ipv6 + ipv6subnet mask in ipv4-like notation separated by dot '.'
        temp = &orig[pos];
        if ((uint8_t)ip_size > temp - orig) {
            memcpy(ip, orig, temp - orig);
            ip[temp - orig] = '\0';
            convert_ipv6(ip);
        }
        temp++; // skip the '.'
        if (ip2 && (ip2_size > strlen(temp))) {
            memcpy(ip2, temp, strlen(temp));
            ip2[strlen(temp)] = '\0';
            convert_ipv6(ip2);
        }
    }
}

void separate_ip_addresses(char *orig, char *ip, size_t ip_size, char *ip2, size_t ip2_size)
{
    // orig can include ipv4, ipv6, both or two ip4/ipv6 addresses.
    // also format depends on possible AT+CGPIAF
    if (!orig || !ip) {
        if (ip) {
            ip[0] = '\0';
        }
        if (ip2) {
            ip2[0] = '\0';
        }
        return;
    }
    // 1. try to found ':'. If it's found then we know that possible addresses are separated with space
    char *temp;
    temp = strchr(orig, ':');

    if (temp != NULL) {
        // found ':'
        temp = strstr(orig, " ");
        // found space as separator and it wasn't in beginning --> contains 2 ip addresses
        if (temp && temp != orig) {
            if ((uint8_t)ip_size > temp - orig) {
                memcpy(ip, orig, temp - orig);
                ip[temp - orig] = '\0';
            } else {
                ip[0] = '\0';
            }
            temp++; // skip the space
            if (ip2 && (ip2_size > strlen(temp))) {
                memcpy(ip2, temp, strlen(temp));
                ip2[strlen(temp)] = '\0';
            } else if (ip2) {
                ip2[0] = '\0';
            }
        } else {
            // Space was the first char or no space found ---> only one ip, copy to ip
            size_t size = strlen(orig);
            if (temp) {
                size = strlen(temp);
            }

            if (ip_size > size) {
                memcpy(ip, orig, size);
                ip[size] = '\0';
            } else {
                ip[0] = '\0';
            }
            if (ip2) {
                ip2[0] = '\0';
            }
        }
    } else {
        separate_ip4like_addresses(orig, ip, ip_size, ip2, ip2_size);
    }
}

bool is_ipv6_address(char *ip)
{
    char *temp = strchr(ip, ':');
    if (temp && !strchr(ip, ' ')) {
        return true;
    } else {
        return false;
    }
}

void separate_space_delimited_addresses(bool ipv6,
                                        char *orig,
                                        char *ip1, size_t ip1_size,
                                        char *ip2, size_t ip2_size)
{
    ip1[0] = '\0';
    ip2[0] = '\0';

    if (!orig) {
        return;
    }

    // strtok modifies the original string so let's make a copy of it so we
    // don't modify the given string.
    char *tmp = new char[strlen(orig) + 1];
    strcpy(tmp, orig);

    char *pch;
    pch = strtok(tmp, " ");
    int i = 0;
    while (pch != NULL && i < 2) {
        if (i == 0 && ip1_size > strlen(pch) && ((!ipv6 && !is_ipv6_address(pch)) || (ipv6 && is_ipv6_address(pch)))) {
            strcpy(ip1, pch);
            i++;
        } else if (i == 1 && ip2_size > strlen(pch) && ((!ipv6 && !is_ipv6_address(pch)) || (ipv6 && is_ipv6_address(pch)))) {
            strcpy(ip2, pch);
            i++;
        }

        pch = strtok(NULL, " ");
    }

    delete[] tmp;
}

void prefer_ipv6(char *ip, size_t ip_size, char *ip2, size_t ip2_size)
{
    if (!ip || !ip2) {
        return;
    }
    // assume that that ipv6 is already in formatted to use ':'
    // 1. try to found ':'. If it's found then we know that this is ipv6
    char *temp;
    temp = strchr(ip, ':');
    if (temp) {
        // ip has ipv6 address, we can leave
        return;
    } else {
        // ip was not ipv6, check if ip2 is
        temp = strchr(ip2, ':');
        if (temp) {
            // ipv6 was found in ip2 but not in ip ---> we must swap them. Sadly ip and ip2 might not be pointers
            // so we can't just swap them, must use copy.
            if (strlen(ip) < ip2_size && strlen(ip2) < ip_size && strlen(ip) < 64 && strlen(ip2) < 64) {
                char tmp[64];
                strncpy(tmp, ip, strlen(ip));
                tmp[strlen(ip)] = '\0';
                strncpy(ip, ip2, strlen(ip2));
                ip[strlen(ip2)] = '\0';
                strncpy(ip2, tmp, strlen(tmp));
                ip2[strlen(tmp)] = '\0';
            }
        }
    }
}

void int_to_hex_str(uint8_t num, char *buf)
{
    char charNum = num;
    char_str_to_hex_str(&charNum, 1, buf);
}

int hex_str_to_int(const char *hex_string, int hex_string_length)
{
    const int base = 16;
    int character_as_integer, integer_output = 0;

    for (int i = 0; i < hex_string_length && hex_string[i] != '\0'; i++) {
        if (hex_string[i] >= '0' && hex_string[i] <= '9') {
            character_as_integer = hex_string[i] - '0';
        } else if (hex_string[i] >= 'A' && hex_string[i] <= 'F') {
            character_as_integer = hex_string[i] - 'A' + 10;
        } else {
            character_as_integer = hex_string[i] - 'a' + 10;
        }
        integer_output *= base;
        integer_output += character_as_integer;
    }

    return integer_output;
}

int hex_str_to_char_str(const char *str, uint16_t len, char *buf)
{
    int strcount = 0;
    if (str && buf) {
        for (int i = 0; i + 1 < len; i += 2) {
            char tmp;
            hex_to_char(str + i, tmp);
            buf[strcount] = tmp;
            strcount++;
        }
    }

    return strcount;
}

void hex_to_char(const char *hex, char &buf)
{
    if (hex) {
        int upper = hex_str_to_int(hex, 1);
        int lower = hex_str_to_int(hex + 1, 1);
        buf = ((upper << 4) & 0xF0) | (lower & 0x0F);
    }
}

void uint_to_binary_str(uint32_t num, char *str, int str_size, int bit_cnt)
{
    if (!str || str_size < bit_cnt) {
        return;
    }
    int tmp, pos = 0;

    for (int i = 31; i >= 0; i--) {
        tmp = num >> i;
        if (i < bit_cnt) {
            if (tmp & 1) {
                str[pos] = 1 + '0';
            } else {
                str[pos] = 0 + '0';
            }
            pos++;
        }
    }
}

uint32_t binary_str_to_uint(const char *binary_string, int binary_string_length)
{
    if (!binary_string || !binary_string_length) {
        return 0;
    }

    int integer_output = 0, base_exp = 1;

    for (int i = binary_string_length - 1; i >= 0; i--) {
        if (binary_string[i] == '1') {
            integer_output += base_exp;
        }
        if (binary_string[i] != '\0') {
            base_exp <<= 1;
        }
    }

    return integer_output;
}

int char_str_to_hex_str(const char *str, uint16_t len, char *buf, bool omit_leading_zero)
{
    if (!str || !buf) {
        return 0;
    }

    char *ptr = buf;
    int i = 0;
    while (i < len) {
        if (omit_leading_zero == true && i == 0 && !(str[i] >> 4 & 0x0F)) {
            *ptr++ = hex_values[(str[i]) & 0x0F];
        } else {
            *ptr++ = hex_values[((str[i]) >> 4) & 0x0F];
            *ptr++ = hex_values[(str[i]) & 0x0F];
        }
        i++;
    }
    return ptr - buf;
}

uint16_t get_dynamic_ip_port()
{
    static uint16_t port_counter = RANDOM_PORT_NUMBER_COUNT;

    if (port_counter == RANDOM_PORT_NUMBER_COUNT) {
        randLIB_seed_random();
        port_counter = randLIB_get_random_in_range(0, RANDOM_PORT_NUMBER_COUNT - 1);
    }

    port_counter += randLIB_get_random_in_range(1, RANDOM_PORT_NUMBER_MAX_STEP);
    port_counter %= RANDOM_PORT_NUMBER_COUNT;

    return (RANDOM_PORT_NUMBER_START + port_counter);
}

} // namespace mbed_cellular_util
