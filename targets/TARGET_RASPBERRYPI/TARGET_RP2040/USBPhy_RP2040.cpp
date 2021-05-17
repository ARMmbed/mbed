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

#include "USBPhyHw.h"

#ifdef __cplusplus
extern "C" {
#endif

// USB register definitions from pico-sdk
#include "hardware/regs/usb.h"
// USB hardware struct definitions from pico-sdk
#include "hardware/structs/usb.h"
// For interrupt enable and numbers
#include "hardware/irq.h"
// For resetting the USB controller
#include "hardware/resets.h"

#ifdef PICO_RP2040_USB_DEVICE_ENUMERATION_FIX
#include "rp2040_usb_device_enumeration.h"
#include "hardware/structs/iobank0.h"
#endif

#ifdef __cplusplus
}
#endif

// These accessor functions are used to implement bit clear / bit sets through
// an atomic alias (this handles the state where both cores can access a register
// and cause a bit loss through a read-modify-write access)
#define usb_hw_set hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

static USBPhyHw *instance;

USBPhy *get_usb_phy()
{
    static USBPhyHw usbphy;
    return &usbphy;
}

USBPhyHw::USBPhyHw(): events(NULL)
{

}

USBPhyHw::~USBPhyHw()
{

}

void USBPhyHw::init(USBPhyEvents *events)
{
    this->events = events;
    this->new_addr = 0;
    instance = this;

    // Disable IRQ
    NVIC_DisableIRQ(USBCTRL_IRQn);

    // Reset usb controller
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

    // Clear any previous state in dpram just in case
    memset(usb_dpram, 0, sizeof(*usb_dpram));

    // Mux the controller to the onboard usb phy
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;

    // Force VBUS detect so the device thinks it is plugged into a host
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

    // Enable the USB controller in device mode.
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

    // Enable IRQ
    NVIC_SetVector(USBCTRL_IRQn, (uint32_t)&_usbisr);
    NVIC_EnableIRQ(USBCTRL_IRQn);
}

void USBPhyHw::deinit()
{
    // Disconnect and disable interrupt
    disconnect();
    NVIC_DisableIRQ(USBCTRL_IRQn);
}

bool USBPhyHw::powered()
{
    return true;
}

void USBPhyHw::connect()
{
    // Enable interrupts for when a buffer is done, when the bus is reset,
    // and when a setup packet is received
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS   |
                   USB_INTS_BUS_RESET_BITS     |
                   USB_INTS_SETUP_REQ_BITS     |
                   USB_INTS_HOST_CONN_DIS_BITS |
                   USB_INTS_HOST_RESUME_BITS   |
                   USB_INTS_ERROR_RX_OVERFLOW_BITS |
                   USB_INTS_STALL_BITS;

    // Present full speed device by enabling pull up on DP
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;

    // Enable interrupts on EP0, single buffered
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
}

void USBPhyHw::disconnect()
{
    // Clear all endpoint interrupts and disable interrupts
    memset(&usb_dpram->ep_ctrl[0], 0, sizeof(*usb_dpram) - sizeof(usb_dpram->setup_packet));

    // TODO - Disable pullup on D+
    usb_hw_clear->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

void USBPhyHw::configure()
{
    // Nothing to be done
}

void USBPhyHw::unconfigure()
{
    // Also nothing to do here
}

void USBPhyHw::sof_enable()
{
    usb_hw_set->inte = USB_INTE_DEV_SOF_BITS;
}

void USBPhyHw::sof_disable()
{
    usb_hw_clear->inte = USB_INTE_DEV_SOF_BITS;
}

void USBPhyHw::set_address(uint8_t address)
{
    // We can't set the device address here, because we're only half way
    // through the control transfer (the OUT bit), we need to wait until the
    // IN bit is completed (the status phase) before setting it...
    this->new_addr = address;
}

void USBPhyHw::remote_wakeup()
{
    // Send remote wakeup over USB lines
    // TODO - confirm that the resume doesn't require resetting
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_RESUME_BITS;
}

const usb_ep_table_t *USBPhyHw::endpoint_table()
{
    static const usb_ep_table_t template_table = {
        sizeof(usb_dpram->epx_data),
        {
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
            {USB_EP_ATTR_ALLOW_ALL | USB_EP_ATTR_DIR_IN_AND_OUT,  1, 0},
        }
    };
    return &template_table;
}

uint32_t USBPhyHw::ep0_set_max_packet(uint32_t max_packet)
{
    // Our max packet size is 64 bytes
    return 64;
}

// read setup packet
void USBPhyHw::ep0_setup_read_result(uint8_t *buffer, uint32_t size)
{
    memcpy(buffer, (void *) usb_dpram->setup_packet, size < 8 ? size : 8);
}

void USBPhyHw::ep0_read(uint8_t *data, uint32_t size)
{
    endpoint_info_t * ep = &this->ep_info_out[0];

    ep->data = data;

    usb_dpram->ep_buf_ctrl[0].out = (size & USB_BUF_CTRL_LEN_MASK) |
                                    USB_BUF_CTRL_AVAIL |
                                    (ep->next_pid ? USB_BUF_CTRL_DATA1_PID : 0);

    ep->next_pid = !ep->next_pid;
}

uint32_t USBPhyHw::ep0_read_result()
{
    endpoint_info_t * ep = &this->ep_info_out[0];
    int buf_ctrl = usb_dpram->ep_buf_ctrl[0].out;
    int sz = buf_ctrl & USB_BUF_CTRL_LEN_MASK;

    if(buf_ctrl & USB_BUF_CTRL_FULL)
    {
        if(ep->data != NULL)
        {
            memcpy(ep->data, ep->dpram, sz);
        }
    }
    else
        sz = 0;

    return sz;
}

void USBPhyHw::ep0_write(uint8_t *buffer, uint32_t size)
{
    endpoint_info_t * ep = &this->ep_info_in[0];

    if(buffer != NULL)
        memcpy((void *) ep->dpram, buffer, size);

    __asm volatile (
            "b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1:\n"
            : : : "memory");

    usb_dpram->ep_buf_ctrl[0].in = size |
                                   USB_BUF_CTRL_FULL  |
                                   USB_BUF_CTRL_AVAIL |
                                   (ep->next_pid ? USB_BUF_CTRL_DATA1_PID : 0) ;
    ep->next_pid = !ep->next_pid;
}

void USBPhyHw::ep0_stall()
{
    usb_hw->ep_stall_arm = 3;
    usb_dpram->ep_buf_ctrl[0].in  = USB_BUF_CTRL_STALL;
    usb_dpram->ep_buf_ctrl[0].out = USB_BUF_CTRL_STALL;
}

bool USBPhyHw::endpoint_add(usb_ep_t endpoint, uint32_t max_packet, usb_ep_type_t type)
{
    int ep_num = endpoint & 0x7f;
    int in     = endpoint >> 7;
    io_rw_32 * ep_ctrl = in ? &usb_dpram->ep_ctrl[ep_num - 1].in :
                              &usb_dpram->ep_ctrl[ep_num - 1].out;
    if(this->dpram_buffer_free_ptr + max_packet < sizeof(usb_dpram->epx_data) )
    {
        endpoint_info_t * ep = in ? &this->ep_info_in[ep_num] : &this->ep_info_out[ep_num];
        ep->next_pid = 0;
        ep->dpram = &usb_dpram->epx_data[this->dpram_buffer_free_ptr];

        *ep_ctrl =
            EP_CTRL_ENABLE_BITS |
            EP_CTRL_INTERRUPT_PER_BUFFER |
            type << EP_CTRL_BUFFER_TYPE_LSB |
            EP_CTRL_INTERRUPT_ON_STALL |
            (this->dpram_buffer_free_ptr + 0x180);

        this->dpram_buffer_free_ptr += (max_packet + 63) & ~63;
    }
    else
    {
        return false;
    }

    return true;
}

void USBPhyHw::endpoint_remove(usb_ep_t endpoint)
{

}

void USBPhyHw::endpoint_stall(usb_ep_t endpoint)
{
    int ep_num = endpoint & 0x7f;
    usb_dpram->ep_buf_ctrl[ep_num].out |= USB_BUF_CTRL_STALL;
}

void USBPhyHw::endpoint_unstall(usb_ep_t endpoint)
{
    int ep_num = endpoint & 0x7f;
    usb_dpram->ep_buf_ctrl[ep_num].out &= ~USB_BUF_CTRL_STALL;
}

bool USBPhyHw::endpoint_read(usb_ep_t endpoint, uint8_t *data, uint32_t size)
{
    int ep_num = endpoint & 0x7f;

    endpoint_info_t * ep = &this->ep_info_out[ep_num];

    ep->data = data;

    usb_dpram->ep_buf_ctrl[ep_num].out =
                                    (size & USB_BUF_CTRL_LEN_MASK) |
                                    USB_BUF_CTRL_AVAIL |
                                    (ep->next_pid ? USB_BUF_CTRL_DATA1_PID : 0);

    ep->next_pid = !ep->next_pid;

    return true;
}

uint32_t USBPhyHw::endpoint_read_result(usb_ep_t endpoint)
{
    int ep_num = endpoint & 0x7f;
    endpoint_info_t * ep = &this->ep_info_out[ep_num];

    int buf_ctrl = usb_dpram->ep_buf_ctrl[ep_num].out;
    int sz = buf_ctrl & USB_BUF_CTRL_LEN_MASK;

    if(buf_ctrl & USB_BUF_CTRL_FULL)
    {
        if(ep->data != NULL)
        {
            memcpy(ep->data, ep->dpram, sz);
        }
    }
    else
        sz = 0;

    return sz;
}

bool USBPhyHw::endpoint_write(usb_ep_t endpoint, uint8_t *data, uint32_t size)
{
    int ep_num = endpoint & 0x7f;

    endpoint_info_t * ep = &this->ep_info_in[ep_num];

    if(data != NULL)
        memcpy(ep->dpram, data, size);

    __asm volatile (
            "b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1: b 1f\n"
            "1:\n"
            : : : "memory");


    usb_dpram->ep_buf_ctrl[ep_num].in = size |
                                        USB_BUF_CTRL_FULL  |
                                        USB_BUF_CTRL_AVAIL |
                                        (ep->next_pid ? USB_BUF_CTRL_DATA1_PID : 0);

    ep->next_pid = !ep->next_pid;

    return true;
}

void USBPhyHw::endpoint_abort(usb_ep_t endpoint)
{

}

void USBPhyHw::process()
{

again:

    // reset interrupt
    if (usb_hw->ints & USB_INTS_BUS_RESET_BITS) {
        // Clear the device address
        usb_hw->dev_addr_ctrl = 0;
        // Reset all endpoint buffers and controls (leave SETUP packet)
        memset(&usb_dpram->ep_ctrl[0], 0, sizeof(*usb_dpram) - sizeof(usb_dpram->setup_packet));
        this->dpram_buffer_free_ptr = 0;
        // Clear the bus reset
        usb_hw->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;

        // This is required, but having trouble working out how to link in
#ifdef PICO_RP2040_USB_DEVICE_ENUMERATION_FIX
        const uint dp = 15;
        auto function = gpio_get_function(dp);
        auto override = iobank0_hw->io[dp].ctrl;
        rp2040_usb_device_enumeration_fix();
        gpio_set_function(dp, function);
        gpio_set_inover(dp, GPIO_OVERRIDE_HIGH);
        hw_set_bits(&iobank0_hw->io[dp].ctrl, override);
#endif

        // reset bus for USBDevice layer
        events->reset();

        // Re-enable interrupt
        NVIC_ClearPendingIRQ(USBCTRL_IRQn);
        NVIC_EnableIRQ(USBCTRL_IRQn);
        return;
    }

    // Received SETUP packet
    if(usb_hw->ints & USB_INTS_SETUP_REQ_BITS)
    {
        this->ep_info_in [0].next_pid = 1;
        this->ep_info_in [0].dpram = usb_dpram->ep0_buf_a;
        this->ep_info_out[0].next_pid = 1;
        this->ep_info_out[0].dpram = usb_dpram->ep0_buf_a;
        events->ep0_setup();
        // Clear interrupt
        usb_hw->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
    }

    // BUFF_STATUS bits have changed
    if(usb_hw->ints & USB_INTS_BUFF_STATUS_BITS)
    {
        uint32_t buff_status = usb_hw->buf_status;
        int i;

        // EP0 IN
        if(buff_status & 1)
        {
            // Clear this bit
            usb_hw->buf_status = 1;
            events->ep0_in();

            // If we got a new address in the (OUT) DATA stage of a control
            // transfer, then set it here after having received the (IN) STATUS
            // stage
            if(this->new_addr != 0)
            {
                usb_hw->dev_addr_ctrl = this->new_addr;
                this->new_addr = 0;
            }
        }

        // EP0 OUT
        if(buff_status & 2)
        {
            // Clear this bit
            usb_hw->buf_status = 2;
            events->ep0_out();
        }

        // Go through the rest of the bits
        for(i = 2; i < 32; i++)
        {
            if(buff_status & (1 << i))
            {
                // Clear the bit
                usb_hw->buf_status = 1 << i;
                if(i & 1)
                {
                    events->out(i / 2);
                }
                else
                {
                    events->in(0x80 | (i / 2));
                }
            }
        }
    }

    // sof interrupt
    if (usb_hw->ints & USB_INTR_DEV_SOF_BITS) {
        // SOF event, read frame number
        events->sof(usb_hw->sof_rd);
    }

    // Suspend / Resume not tested, not found a way of triggering suspend
    if(usb_hw->ints & USB_INTS_DEV_RESUME_FROM_HOST_BITS)
    {
        // Will clear the interrupt
        usb_hw_set->sie_ctrl = USB_SIE_CTRL_RESUME_BITS;
        events->suspend(false);
    }

    if(usb_hw->ints & USB_INTS_DEV_SUSPEND_BITS)
    {
        usb_hw->sie_status = USB_SIE_STATUS_SUSPENDED_BITS;
        events->suspend(true);
    }

    // If any interrupts are still set (except BUFSTATUS), then hang for analysis
    // This is only for debug while developing the driver
    if(usb_hw->ints & ~16)
    {
        volatile int ints = usb_hw->ints;
        volatile int going = 1;
        goto again;
        //while(ints && going);
    }


    // Re-enable interrupt
    NVIC_ClearPendingIRQ(USBCTRL_IRQn);
    NVIC_EnableIRQ(USBCTRL_IRQn);
}

void USBPhyHw::_usbisr(void)
{
    NVIC_DisableIRQ(USBCTRL_IRQn);
    instance->events->start_process();
}
