/*
 * Copyright (c) 2018, Arm Limited and affiliates.
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

#include "drivers/BufferedSerial.h"

namespace mbed {


BufferedSerial::BufferedSerial(PinName tx, PinName rx, int baud) : SerialBase(tx, rx, baud)
{
}

BufferedSerial::~BufferedSerial()
{
}

ssize_t BufferedSerial::read(void *buffer, size_t length)
{
    return 0;
}

ssize_t BufferedSerial::write(const void *buffer, size_t length)
{
    return 0;
}

off_t BufferedSerial::seek(off_t offset, int whence)
{
    return -ESPIPE;
}

int BufferedSerial::close()
{
    return 0;
}

void BufferedSerial::dcd_irq()
{
}

void BufferedSerial::set_baud(int baud)
{
}

void BufferedSerial::set_data_carrier_detect(PinName dcd_pin, bool active_high)
{
}

void BufferedSerial::set_format(int bits, Parity parity, int stop_bits)
{
}

int BufferedSerial::isatty()
{
    return 1;
}

int BufferedSerial::sync()
{
    return 0;
}

void BufferedSerial::sigio(Callback<void()> func)
{
}

ssize_t BufferedSerial::write_unbuffered(const char *buf_ptr, size_t length)
{
    return 0;
}

bool BufferedSerial::hup() const
{
    return false;
}

void BufferedSerial::wake()
{
}

short BufferedSerial::poll(short events) const
{
    return 0;
}

void BufferedSerial::api_lock(void)
{
}

void BufferedSerial::api_unlock(void)
{
}

void BufferedSerial::rx_irq(void)
{
}

void BufferedSerial::tx_irq(void)
{
}

int BufferedSerial::enable_input(bool enabled)
{
    return 0;
}

int BufferedSerial::enable_output(bool enabled)
{
    return 0;
}

#if DEVICE_SERIAL_FC
void BufferedSerial::set_flow_control(mbed::SerialBase::Flow, PinName, PinName)
{

}
#endif

}
