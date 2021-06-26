/*
 * Copyright (c) 2017, Arm Limited and affiliates.
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

#include "drivers/SerialBase.h"

namespace mbed {

SerialBase::SerialBase(PinName tx, PinName rx, int baud) :
    _tx_pin(tx), _rx_pin(rx)
{
}

SerialBase::~SerialBase()
{

}

void SerialBase::baud(int baudrate)
{
}

void SerialBase::format(int bits, Parity parity, int stop_bits)
{
}

int SerialBase::readable()
{
    return 0;
}


int SerialBase::writeable()
{
    return 0;
}

void SerialBase::attach(Callback<void()> func, IrqType type)
{
}

void SerialBase::_irq_handler(uint32_t id, SerialIrq irq_type)
{
}

int SerialBase::_base_getc()
{
    return 0;
}

int SerialBase::_base_putc(int c)
{
    return 0;
}

void SerialBase::send_break()
{
}

void SerialBase::lock()
{
}

void SerialBase:: unlock()
{
}

}
