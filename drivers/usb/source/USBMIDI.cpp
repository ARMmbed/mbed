/*
 * Copyright (c) 2018-2019, Arm Limited and affiliates.
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

#include <stdint.h>
#include <string.h>
#include "USBMIDI.h"
#include "EndpointResolver.h"
#include "usb_phy_api.h"

#define FLAG_WRITE_DONE     (1 << 0)
#define FLAG_DISCONNECT     (1 << 1)
#define FLAG_CONNECT        (1 << 2)

USBMIDI::USBMIDI(bool connect_blocking, uint16_t vendor_id, uint16_t product_id, uint16_t product_release)
    : USBDevice(get_usb_phy(), vendor_id, product_id, product_release)
{
    _init();

    if (connect_blocking) {
        USBDevice::connect();
        wait_ready();
    } else {
        init();
    }
}

USBMIDI::USBMIDI(USBPhy *phy, uint16_t vendor_id, uint16_t product_id, uint16_t product_release)
    : USBDevice(phy, vendor_id, product_id, product_release)
{
    _init();

    // User or child responsible for calling connect or init
}

USBMIDI::~USBMIDI()
{
    deinit();
}

void USBMIDI::_init()
{
    _bulk_buf_pos = 0;
    _bulk_buf_size = 0;

    _data_ready = false;
    _cur_data = 0;

    EndpointResolver resolver(endpoint_table());

    resolver.endpoint_ctrl(64);
    _bulk_in = resolver.endpoint_in(USB_EP_TYPE_BULK, MaxSize);
    _bulk_out = resolver.endpoint_out(USB_EP_TYPE_BULK, MaxSize);
    MBED_ASSERT(resolver.valid());
}

bool USBMIDI::ready()
{
    return _flags.get() & FLAG_CONNECT ? true : false;
}

void USBMIDI::wait_ready()
{
    _flags.wait_any(FLAG_CONNECT, osWaitForever, false);
}

// write plain MIDIMessage that will be converted to USBMidi event packet
bool USBMIDI::write(MIDIMessage m)
{
    _write_mutex.lock();

    bool ret = true;
    // first byte keeped for retro-compatibility
    for (int p = 1; p < m.length; p += 3) {
        uint8_t buf[4];
        // Midi message to USBMidi event packet
        buf[0] = m.data[1] >> 4;
        // SysEx
        if (buf[0] == 0xF) {
            if ((m.length - p) > 3) {
                // SysEx start or continue
                buf[0] = 0x4;
            } else {
                switch (m.length - p) {
                    case 1:
                        // SysEx end with one byte
                        buf[0] = 0x5;
                        break;
                    case 2:
                        // SysEx end with two bytes
                        buf[0] = 0x6;
                        break;
                    case 3:
                        // SysEx end with three bytes
                        buf[0] = 0x7;
                        break;
                }
            }
        }
        buf[1] = m.data[p];

        if (p + 1 < m.length) {
            buf[2] = m.data[p + 1];
        } else {
            buf[2] = 0;
        }

        if (p + 2 < m.length) {
            buf[3] = m.data[p + 2];
        } else {
            buf[3] = 0;
        }

        _flags.clear(FLAG_WRITE_DONE);
        USBDevice::write_start(_bulk_in, buf, 4);
        uint32_t flags = _flags.wait_any(FLAG_WRITE_DONE | FLAG_DISCONNECT | FLAG_CONNECT, osWaitForever, false);
        if (flags & FLAG_DISCONNECT) {
            ret = false;
            break;
        }
        USBDevice::write_finish(_bulk_in);
    }

    _write_mutex.unlock();
    return ret;
}

bool USBMIDI::readable()
{
    lock();

    bool ret = _data_ready;

    unlock();

    return ret;
}

bool USBMIDI::read(MIDIMessage *m)
{
    lock();

    // Invalidate message
    m->length = 0;

    if (!_data_ready) {
        unlock();
        return false;
    }

    m->from_raw(_data, _cur_data);
    _cur_data = 0;
    _next_message();

    if (!_data_ready) {
        read_start(_bulk_out, _bulk_buf, MaxSize);
    }

    unlock();

    return true;
}

void USBMIDI::attach(mbed::Callback<void()> callback)
{
    lock();

    _callback = callback;

    unlock();
}

void USBMIDI::callback_state_change(DeviceState new_state)
{
    assert_locked();

    if (new_state == Configured) {
        _flags.set(FLAG_CONNECT);
        _flags.clear(FLAG_DISCONNECT);
    } else {
        _flags.set(FLAG_DISCONNECT);
        _flags.clear(FLAG_CONNECT | FLAG_WRITE_DONE);
    }
}

void USBMIDI::callback_request(const setup_packet_t *setup)
{
    assert_locked();

    RequestResult result = PassThrough;
    uint8_t *data = NULL;
    uint32_t size = 0;

    complete_request(result, data, size);
}

void USBMIDI::callback_request_xfer_done(const setup_packet_t *setup, bool aborted)
{
    assert_locked();

    complete_request_xfer_done(false);
}

void USBMIDI::callback_set_configuration(uint8_t configuration)
{
    assert_locked();

    if (configuration == DEFAULT_CONFIGURATION) {
        complete_set_configuration(false);
    }

    endpoint_remove_all();
    endpoint_add(_bulk_in, MaxSize, USB_EP_TYPE_BULK, &USBMIDI::_in_callback);
    endpoint_add(_bulk_out, MaxSize, USB_EP_TYPE_BULK, &USBMIDI::_out_callback);

    read_start(_bulk_out, _bulk_buf, MaxSize);

    complete_set_configuration(true);
}

void USBMIDI::callback_set_interface(uint16_t interface, uint8_t alternate)
{
    assert_locked();

    complete_set_interface(true);
}

const uint8_t *USBMIDI::string_iinterface_desc()
{
    static const uint8_t string_iinterface_descriptor[] = {
        0x0c,                           //bLength
        STRING_DESCRIPTOR,              //bDescriptorType 0x03
        'A', 0, 'u', 0, 'd', 0, 'i', 0, 'o', 0 //bString iInterface - Audio
    };
    return string_iinterface_descriptor;
}

const uint8_t *USBMIDI::string_iproduct_desc()
{
    static const uint8_t string_iproduct_descriptor[] = {
        0x16,                                                       //bLength
        STRING_DESCRIPTOR,                                          //bDescriptorType 0x03
        'M', 0, 'b', 0, 'e', 0, 'd', 0, ' ', 0, 'A', 0, 'u', 0, 'd', 0, 'i', 0, 'o', 0 //bString iProduct - Mbed Audio
    };
    return string_iproduct_descriptor;
}

const uint8_t *USBMIDI::configuration_desc(uint8_t index)
{
    if (index != 0) {
        return NULL;
    }

    uint8_t config_descriptor_temp[] = {
        // configuration descriptor
        0x09, 0x02, 0x65, 0x00, 0x02, 0x01, 0x00, 0xc0, 0x50,

        // The Audio Interface Collection
        0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, // Standard AC Interface Descriptor
        0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, // Class-specific AC Interface Descriptor
        0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, // MIDIStreaming Interface Descriptors
        0x07, 0x24, 0x01, 0x00, 0x01, 0x41, 0x00,             // Class-Specific MS Interface Header Descriptor

        // MIDI IN JACKS
        0x06, 0x24, 0x02, 0x01, 0x01, 0x00,
        0x06, 0x24, 0x02, 0x02, 0x02, 0x00,

        // MIDI OUT JACKS
        0x09, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00,
        0x09, 0x24, 0x03, 0x02, 0x06, 0x01, 0x01, 0x01, 0x00,

        // OUT endpoint - Standard MS Bulk Data Endpoint Descriptor
        0x09,       // bLength
        0x05,       // bDescriptorType
        _bulk_out,   // bEndpointAddress
        0x02,       // bmAttributes
        0x40,       // wMaxPacketSize (LSB)
        0x00,       // wMaxPacketSize (MSB)
        0x00,       // bInterval (milliseconds)
        0x00,       // bRefresh
        0x00,       // bSynchAddress

        0x05, 0x25, 0x01, 0x01, 0x01,

        // IN endpoint - Standard MS Bulk Data Endpoint Descriptor
        0x09,       // bLength
        0x05,       // bDescriptorType
        _bulk_in,  // bEndpointAddress
        0x02,       // bmAttributes
        0x40,       // wMaxPacketSize (LSB)
        0x00,       // wMaxPacketSize (MSB)
        0x00,       // bInterval (milliseconds)
        0x00,       // bRefresh
        0x00,       // bSynchAddress

        0x05, 0x25, 0x01, 0x01, 0x03,
    };
    MBED_ASSERT(sizeof(config_descriptor_temp) == sizeof(_config_descriptor));
    memcpy(_config_descriptor, config_descriptor_temp, sizeof(config_descriptor_temp));
    return _config_descriptor;
}

void USBMIDI::_in_callback()
{
    assert_locked();

    _flags.set(FLAG_WRITE_DONE);
}

void USBMIDI::_out_callback()
{
    assert_locked();

    _bulk_buf_size = read_finish(_bulk_out);
    _bulk_buf_pos = 0;

    if (_callback && _next_message()) {
        _callback();
        return;
    }

    read_start(_bulk_out, _bulk_buf, MaxSize);
}

bool USBMIDI::_next_message()
{
    assert_locked();

    bool data_ready = false;
    while (_bulk_buf_pos < _bulk_buf_size) {
        uint8_t data_read;
        bool data_end = true;
        switch (_bulk_buf[_bulk_buf_pos]) {
            case 0x2:
                // Two-bytes System Common Message - undefined in USBMidi 1.0
                data_read = 2;
                break;
            case 0x4:
                // SysEx start or continue
                data_end = false;
                data_read = 3;
                break;
            case 0x5:
                // Single-byte System Common Message or SysEx end with one byte
                data_read = 1;
                break;
            case 0x6:
                // SysEx end with two bytes
                data_read = 2;
                break;
            case 0xC:
                // Program change
                data_read = 2;
                break;
            case 0xD:
                // Channel pressure
                data_read = 2;
                break;
            case 0xF:
                // Single byte
                data_read = 1;
                break;
            default:
                // Others three-bytes messages
                data_read = 3;
                break;
        }

        for (uint8_t j = 1; j < data_read + 1; j++) {
            if (_cur_data < sizeof(_data)) {
                _data[_cur_data] = _bulk_buf[_bulk_buf_pos + j];
            }
            _cur_data++;
        }
        _bulk_buf_pos += 4;

        if (data_end) {
            // Message is ready to be read
            data_ready = true;
            break;
        }
    }

    _data_ready = data_ready;
    return data_ready;
}
