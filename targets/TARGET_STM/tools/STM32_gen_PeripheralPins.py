#!/usr/bin/env python

"""
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2016-2020 STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
"""

import argparse
import datetime
import fnmatch
import json
import os
import re
import sys
import textwrap
from xml.dom.minidom import parse, Node
from argparse import RawTextHelpFormatter
import subprocess

GENPINMAP_VERSION = "1.20.1"

ADD_DEVICE_IF = 0
ADD_GPIO_PINMAP = 0
DEBUG_PRINT = 0
FLAT_DIRECTORY = 0

mcu_file=""
mcu_list = []       #'name'
gpio_list = []        #'PIN','name','BOOT/OSC'
adclist = []        #'PIN','name','ADCSignal'
daclist = []        #'PIN','name','DACSignal'
i2cscl_list = []    #'PIN','name','I2CSCLSignal'
i2csda_list = []    #'PIN','name','I2CSDASignal'
pwm_list = []       #'PIN','name','PWM'
uarttx_list = []    #'PIN','name','UARTtx'
uartrx_list = []    #'PIN','name','UARTrx'
uartcts_list = []   #'PIN','name','UARTcts'
uartrts_list = []   #'PIN','name','UARTrts'
spimosi_list = []   #'PIN','name','SPIMOSI'
spimiso_list = []   #'PIN','name','SPIMISO'
spissel_list = []   #'PIN','name','SPISSEL'
spisclk_list = []   #'PIN','name','SPISCLK'
cantd_list = []     #'PIN','name','CANTD'
canrd_list = []     #'PIN','name','CANRD'
eth_list = []       #'PIN','name','ETH'
quadspidata0_list = []     #'PIN','name','QUADSPIDATA0'
quadspidata1_list = []     #'PIN','name','QUADSPIDATA1'
quadspidata2_list = []     #'PIN','name','QUADSPIDATA2'
quadspidata3_list = []     #'PIN','name','QUADSPIDATA3'
quadspisclk_list = []      #'PIN','name','QUADSPISCLK'
quadspissel_list = []      #'PIN','name','QUADSPISSEL'
octospidata0_list = []     #'PIN','name','OCTOSPIDATA0'
octospidata1_list = []     #'PIN','name','OCTOSPIDATA1'
octospidata2_list = []     #'PIN','name','OCTOSPIDATA2'
octospidata3_list = []     #'PIN','name','OCTOSPIDATA3'
octospidata4_list = []     #'PIN','name','OCTOSPIDATA4'
octospidata5_list = []     #'PIN','name','OCTOSPIDATA5'
octospidata6_list = []     #'PIN','name','OCTOSPIDATA6'
octospidata7_list = []     #'PIN','name','OCTOSPIDATA7'
octospidqs_list = []       #'PIN','name','OCTOSPIDQS'
octospisclk_list = []      #'PIN','name','OCTOSPISCLK'
octospissel_list = []      #'PIN','name','OCTOSPISSEL'
usb_list = []        # 'PIN','name','USB'
usb_otgfs_list = []  # 'PIN','name','USB'
usb_otghs_list = []  # 'PIN','name','USB'
osc_list = []      #'PIN','name','OSC'
sys_list = []      #'PIN','name','SYS'

STDIO_list = ["Pxx", "Pxx"] # TX , RX
LED_list   = []
BUTTON_list   = []
DUAL_PAD = False
MCU_USERNAME= ""
TIM_MST = ""
ALTERNATE_DEFINITION = 0
TARGET_NAME = ""

TIM_DUALCORE_LIST = { # Timer used for us ticker is hardcoded in this script
"H745":"TIM2",
"H747":"TIM2",
"H750":"TIM2",
"H755":"TIM2"
}

VCP_UART_LIST = { # Used interface is HW option
"Nucleo_NUCLEO-L552ZE-Q":"LPUART1",
"Discovery_STM32L4R9I":"USART2",
"Discovery_STM32L496G":"USART2"
}

def print_debug(console_line):
    if DEBUG_PRINT == 1:
        print("DEBUG: %s" % console_line)

def find_gpio_file():
    res = "ERROR"
    itemlist = xml_mcu.getElementsByTagName("IP")
    for s in itemlist:
        a = s.attributes["Name"].value
        if "GPIO" in a:
            res = s.attributes["Version"].value
    return res


def find_tim_mst():
    global TIM_MST

    # Let's list first the available timers
    tim_list = []
    itemlist = xml_mcu.getElementsByTagName("IP")
    for s in itemlist:
        a = s.attributes["Name"].value
        if "TIM" in a:
            tim_list.append(s.attributes["InstanceName"].value)

    # Then choose 1 timer for us ticker
    TIM_MST = ""
    if TARGET_FAMILY == "STM32F0":
        search_order = ["TIM5", "TIM2", "TIM1"]
    elif TARGET_FAMILY == "STM32F1":
        search_order = ["TIM5", "TIM4", "TIM2"]
    elif TARGET_FAMILY == "STM32F3":
        search_order = ["TIM5", "TIM2"]
    elif TARGET_FAMILY == "STM32G0":
        search_order = ["TIM5", "TIM2", "TIM3"]
    elif TARGET_FAMILY == "STM32G4":
        search_order = ["TIM5", "TIM2"]
    elif TARGET_FAMILY == "STM32L0":
        search_order = ["TIM5", "TIM21"]
    elif TARGET_FAMILY == "STM32L1":
        search_order = ["TIM5", "TIM2"]
    elif TARGET_FAMILY == "STM32L4":
        search_order = ["TIM5", "TIM2"]
    elif TARGET_FAMILY == "STM32WB":
        search_order = ["TIM16", "TIM2"]
    elif TARGET_FAMILY == "STM32WL":
        search_order = ["TIM2"]
    else:
        search_order = ["TIM5"]
    for EachTimer in search_order:
        if EachTimer in tim_list:
            TIM_MST = EachTimer
            break
    if TIM_MST == "":
        print("!!! error TIM_MST not found")
    else:
        print_debug("TIM_MST=%s" % TIM_MST)

def get_gpio_af_num(pintofind, iptofind):
    pintofind = pintofind.split("-")[0].split(" ")[0] # to avoid for ex "PC14-OSC32_IN", "PB4 (NJTRST)"

    if "STM32F10" in mcu_file:
        return get_gpio_af_num_stm32f1(pintofind, iptofind)
    i = 0
    mygpioaf = ""
    for n in xml_gpio.documentElement.childNodes:
        i += 1
        j = 0
        if n.nodeType == Node.ELEMENT_NODE:
            for firstlevel in n.attributes.items():
                # if 'PB7' in firstlevel:
                if pintofind == firstlevel[1].split("-")[0].split(" ")[0]: # to avoid for ex "PC14-OSC32_IN", "PB4 (NJTRST)"
                    # n = pin node found
                    for each_child_node in n.childNodes:
                        j += 1
                        k = 0
                        if each_child_node.nodeType == Node.ELEMENT_NODE:
                            for secondlevel in each_child_node.attributes.items():
                                k += 1
                                # if 'I2C1_SDA' in secondlevel:
                                if iptofind in secondlevel[1].replace("_CTS_NSS", "_CTS"): # to avoid "USART2_CTS_NSS"
                                    # m = IP node found
                                    for p in each_child_node.childNodes:
                                        if p.nodeType == Node.ELEMENT_NODE:
                                            # p node of 'Specific parameter'
                                            for myc in p.childNodes:
                                                if myc.nodeType == Node.ELEMENT_NODE:
                                                    # myc = node of ALTERNATE
                                                    for mygpioaflist in myc.childNodes:
                                                        if mygpioaflist.data not in mygpioaf:
                                                            if mygpioaf != "":
                                                                mygpioaf += " "
                                                            mygpioaf += mygpioaflist.data
    if mygpioaf == "":
        mygpioaf = "GPIO_AF_NONE"
    return mygpioaf


def get_gpio_af_num_stm32f1(pintofind, iptofind):
    print_debug('pin to find ' + pintofind + ' ip to find ' + iptofind)
    i = 0
    mygpioaf = ""
    for gpio_child_node in xml_gpio.documentElement.childNodes:
        i += 1
        j = 0
        if gpio_child_node.nodeType == Node.ELEMENT_NODE:
            for firstlevel in gpio_child_node.attributes.items():
                if pintofind == firstlevel[1]:
                    # gpio_child_node = pin node found
                    for each_child_node in gpio_child_node.childNodes:
                        j += 1
                        k = 0
                        if each_child_node.nodeType == Node.ELEMENT_NODE:
                            for secondlevel in each_child_node.attributes.items():
                                k += 1
                                # if 'I2C1_SDA' in secondlevel:
                                if iptofind in secondlevel:
                                    # m = IP node found
                                    for p in each_child_node.childNodes:
                                        # p node 'RemapBlock'
                                        if (
                                            p.nodeType == Node.ELEMENT_NODE
                                            and p.hasChildNodes() is False
                                        ):
                                            if mygpioaf != "":
                                                mygpioaf += " "
                                            mygpioaf += "AFIO_NONE"
                                        else:
                                            for s in p.childNodes:
                                                if s.nodeType == Node.ELEMENT_NODE:
                                                    # s node 'Specific parameter'
                                                    for myc in s.childNodes:
                                                        if (
                                                            myc.nodeType
                                                            == Node.ELEMENT_NODE
                                                        ):
                                                            # myc = AF value
                                                            for (
                                                                mygpioaflist
                                                            ) in myc.childNodes:
                                                                if mygpioaf != "":
                                                                    mygpioaf += " "
                                                                mygpioaf += mygpioaflist.data.replace(
                                                                    "__HAL_", ""
                                                                ).replace(
                                                                    "_REMAP", ""
                                                                )
    if mygpioaf == "":
        mygpioaf = "AFIO_NONE"
    return mygpioaf.replace("AFIO_NONE", "0")\
       .replace("AFIO_SPI1_ENABLE", "1")\
       .replace("AFIO_I2C1_ENABLE", "2")\
       .replace("AFIO_USART1_ENABLE", "3")\
       .replace("AFIO_USART3_PARTIAL", "5")\
       .replace("AFIO_TIM1_PARTIAL", "6")\
       .replace("AFIO_TIM3_PARTIAL", "7")\
       .replace("AFIO_TIM2_ENABLE", "8")\
       .replace("AFIO_TIM2_PARTIAL_1", "8")\
       .replace("AFIO_TIM2_PARTIAL_2", "8")\
       .replace("AFIO_TIM3_ENABLE", "9")\
       .replace("AFIO_CAN1_2", "10")

def store_pin(pin, name, functionality):
    # store pin I/O
    gpio_list.append([pin, name, functionality])


# function to store ADC list
def store_adc(pin, name, signal):
    adclist.append([pin, name, signal])


# function to store DAC list
def store_dac(pin, name, signal):
    daclist.append([pin, name, signal])


# function to store I2C list
def store_i2c(pin, name, signal):
    # is it SDA or SCL ?
    if "_SCL" in signal:
        i2cscl_list.append([pin, name, signal])
    if "_SDA" in signal:
        i2csda_list.append([pin, name, signal])


# function to store timers
def store_pwm(pin, name, signal):
    if "_CH" in signal:
        pwm_list.append([pin, name, signal])


# function to store Uart pins
def store_uart(pin, name, signal):
    if "_TX" in signal:
        uarttx_list.append([pin, name, signal])
    if "_RX" in signal:
        uartrx_list.append([pin, name, signal])
    if "_CTS" in signal:
        uartcts_list.append([pin, name, signal])
    if "_RTS" in signal:
        uartrts_list.append([pin, name, signal])


# function to store SPI pins
def store_spi(pin, name, signal):
    if "_MISO" in signal:
        spimiso_list.append([pin, name, signal])
    if "_MOSI" in signal:
        spimosi_list.append([pin, name, signal])
    if "_SCK" in signal:
        spisclk_list.append([pin, name, signal])
    if "_NSS" in signal:
        spissel_list.append([pin, name, signal])


# function to store CAN pins
def store_can(pin, name, signal):
    if "_RX" in signal:
        canrd_list.append([pin, name, signal])
    if "_TX" in signal:
        cantd_list.append([pin, name, signal])


# function to store ETH list
def store_eth(pin, name, signal):
    eth_list.append([pin, name, signal])


# function to store QSPI pins
def store_qspi(pin, name, signal):
    if "_IO0" in signal:
        quadspidata0_list.append([pin, name, signal])
    if "_IO1" in signal:
        quadspidata1_list.append([pin, name, signal])
    if "_IO2" in signal:
        quadspidata2_list.append([pin, name, signal])
    if "_IO3" in signal:
        quadspidata3_list.append([pin, name, signal])
    if "_CLK" in signal:
        quadspisclk_list.append([pin, name, signal])
    if "_NCS" in signal:
        quadspissel_list.append([pin, name, signal])


# function to store OSPI pins
def store_ospi(pin, name, signal):
    if "_IO0" in signal:
        octospidata0_list.append([pin, name, signal])
    if "_IO1" in signal:
        octospidata1_list.append([pin, name, signal])
    if "_IO2" in signal:
        octospidata2_list.append([pin, name, signal])
    if "_IO3" in signal:
        octospidata3_list.append([pin, name, signal])
    if "_IO4" in signal:
        octospidata4_list.append([pin, name, signal])
    if "_IO5" in signal:
        octospidata5_list.append([pin, name, signal])
    if "_IO6" in signal:
        octospidata6_list.append([pin, name, signal])
    if "_IO7" in signal:
        octospidata7_list.append([pin, name, signal])
    if "_CLK" in signal:
        octospisclk_list.append([pin, name, signal])
    if "_NCS" in signal:
        octospissel_list.append([pin, name, signal])
    if "_DQS" in signal:
        octospidqs_list.append([pin, name, signal])


# function to store USB pins
def store_usb(pin, name, signal):
    if "OTG" not in signal:
        usb_list.append([pin, name, signal])
    elif signal.startswith("USB_OTG_FS"):
        usb_otgfs_list.append([pin, name, signal])
    elif signal.startswith("USB_OTG_HS"):
        usb_otghs_list.append([pin, name, signal])


# function to store OSC pins
def store_osc(pin, name, signal):
    osc_list.append([pin, name, signal])


# function to store SYS pins
def store_sys(pin, name, signal):
    sys_list.append([pin, name, signal])


def print_header():
    global ALTERNATE_DEFINITION
    date_year = datetime.datetime.now().year

    line_to_write =  ("""/* mbed Microcontroller Library
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2016-%i STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 *
 * Automatically generated from STM32CubeMX/db/mcu/%s
 */

#include "PeripheralPins.h"
#include "mbed_toolchain.h"

//==============================================================================
// Notes
//
// - The pins mentioned Px_y_ALTz are alternative possibilities which use other
//   HW peripheral instances. You can use them the same way as any other "normal"
//   pin (i.e. PwmOut pwm(PA_7_ALT0);). These pins are not displayed on the board
//   pinout image on mbed.org.
//
// - The pins which are connected to other components present on the board have
//   the comment "Connected to xxx". The pin function may not work properly in this
//   case. These pins may not be displayed on the board pinout image on mbed.org.
//   Please read the board reference manual and schematic for more information.
//
// - Warning: pins connected to the default STDIO_UART_TX and STDIO_UART_RX pins are commented
//   See https://os.mbed.com/teams/ST/wiki/STDIO for more information.
//
//==============================================================================

""" % (date_year, os.path.basename(input_file_name)))
    out_c_file.write(line_to_write)

    line_to_write =  ("""/* mbed Microcontroller Library
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2016-%i STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 *
 * Automatically generated from STM32CubeMX/db/mcu/%s
 */

/* MBED TARGET LIST: %s */

#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "cmsis.h"
#include "PinNamesTypes.h"

#ifdef __cplusplus
extern "C" {
#endif
""" % (date_year, os.path.basename(input_file_name), TARGET_NAME))
    out_h_file.write(line_to_write)

    if DUAL_PAD:
        line_to_write = ("""
#define DUAL_PAD 0xF00
""")
        out_h_file.write(line_to_write)

    if ADD_GPIO_PINMAP:
        line_to_write = ("""
/* If this macro is defined, then PinMap_GPIO is present in PeripheralPins.c */
#define GPIO_PINMAP_READY 1
""")
        out_h_file.write(line_to_write)

    line_to_write =  ("""
typedef enum {
""")
    out_h_file.write(line_to_write)


def print_footer():
    line_to_write = ("""
    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

// Standardized LED and button names
""")
    out_h_file.write(line_to_write)

    name_counter = 1
    if not LED_list:
       LED_list.append("Pxx")
    StandardLED = {}
    for EachLED in LED_list:
        PinLabel[EachLED] = "TODO"
        StandardLED[PinLabel[EachLED]] = EachLED

    for EachLED in sorted(StandardLED):
        led_label = " // %s" % EachLED
        out_h_file.write("#define LED%i     %-5s %s\n" % (name_counter, re.sub(r'(P.)', r'\1_', StandardLED[EachLED]), led_label))
        name_counter += 1

    name_counter = 1
    if not BUTTON_list:
        BUTTON_list.append("Pxx")
    for EachBUTTON in BUTTON_list:
        button_label = ""
        if EachBUTTON in PinLabel:
            button_label = " // %s" % PinLabel[EachBUTTON]
        out_h_file.write("#define BUTTON%i  %-5s %s\n" % (name_counter, re.sub(r'(P.)', r'\1_', EachBUTTON).split('/')[0].split('-')[0], button_label))
        name_counter += 1

    line_to_write = ("""
#ifdef __cplusplus
}
#endif

#endif
""")
    out_h_file.write(line_to_write)


def print_all_lists():
    if ADD_GPIO_PINMAP:
        if print_list_header("GPIO", "GPIO", gpio_list, "GPIO"):
            print_gpio()
    if print_list_header("ADC", "ADC", adclist, "ANALOGIN"):
        print_adc()
    if print_list_header("DAC", "DAC", daclist, "ANALOGOUT"):
        print_dac()
    if print_list_header("I2C", "I2C_SDA", i2csda_list, "I2C"):
        print_i2c(i2csda_list)
    if print_list_header("", "I2C_SCL", i2cscl_list, "I2C"):
        print_i2c(i2cscl_list)
    if print_list_header("PWM", "PWM", pwm_list, "PWMOUT"):
        print_pwm()
    if print_list_header("SERIAL", "UART_TX", uarttx_list, "SERIAL"):
        print_uart(uarttx_list)
    if print_list_header("", "UART_RX", uartrx_list, "SERIAL"):
        print_uart(uartrx_list)
    if print_list_header("", "UART_RTS", uartrts_list, "SERIAL"):
        print_uart(uartrts_list)
    if print_list_header("", "UART_CTS", uartcts_list, "SERIAL"):
        print_uart(uartcts_list)
    if print_list_header("SPI", "SPI_MOSI", spimosi_list, "SPI"):
        print_spi(spimosi_list)
    if print_list_header("", "SPI_MISO", spimiso_list, "SPI"):
        print_spi(spimiso_list)
    if print_list_header("", "SPI_SCLK", spisclk_list, "SPI"):
        print_spi(spisclk_list)
    if print_list_header("", "SPI_SSEL", spissel_list, "SPI"):
        print_spi(spissel_list)
    if print_list_header("CAN", "CAN_RD", canrd_list, "CAN"):
        print_can(canrd_list)
    if print_list_header("", "CAN_TD", cantd_list, "CAN"):
        print_can(cantd_list)
    if print_list_header("QUADSPI", "QSPI_DATA0", quadspidata0_list, "QSPI"):
        print_qspi(quadspidata0_list)
    if print_list_header("", "QSPI_DATA1", quadspidata1_list, "QSPI"):
        print_qspi(quadspidata1_list)
    if print_list_header("", "QSPI_DATA2", quadspidata2_list, "QSPI"):
        print_qspi(quadspidata2_list)
    if print_list_header("", "QSPI_DATA3", quadspidata3_list, "QSPI"):
        print_qspi(quadspidata3_list)
    if print_list_header("", "QSPI_SCLK", quadspisclk_list, "QSPI"):
        print_qspi(quadspisclk_list)
    if print_list_header("", "QSPI_SSEL", quadspissel_list, "QSPI"):
        print_qspi(quadspissel_list)
    if print_list_header("OCTOSPI", "OSPI_DATA0", octospidata0_list, "OCTO"):
        print_ospi(octospidata0_list)
    if print_list_header("", "OSPI_DATA1", octospidata1_list, "OCTO"):
        print_ospi(octospidata1_list)
    if print_list_header("", "OSPI_DATA2", octospidata2_list, "OCTO"):
        print_ospi(octospidata2_list)
    if print_list_header("", "OSPI_DATA3", octospidata3_list, "OCTO"):
        print_ospi(octospidata3_list)
    if print_list_header("", "OSPI_DATA4", octospidata2_list, "OCTO"):
        print_ospi(octospidata4_list)
    if print_list_header("", "OSPI_DATA5", octospidata3_list, "OCTO"):
        print_ospi(octospidata5_list)
    if print_list_header("", "OSPI_DATA6", octospidata2_list, "OCTO"):
        print_ospi(octospidata6_list)
    if print_list_header("", "OSPI_DATA7", octospidata3_list, "OCTO"):
        print_ospi(octospidata7_list)
    if print_list_header("", "OSPI_DQS", octospidqs_list, "OCTO"):
        print_ospi(octospidqs_list)
    if print_list_header("", "OSPI_SCLK", octospisclk_list, "OCTO"):
        print_ospi(octospisclk_list)
    if print_list_header("", "OSPI_SSEL", octospissel_list, "OCTO"):
        print_ospi(octospissel_list)
    if print_list_header("USBDEVICE", "USB_FS", usb_list, "USBDEVICE"):
        print_usb(usb_list)
    if print_list_header("USBDEVICE", "USB_FS", usb_otgfs_list, "USBDEVICE"):
        print_usb(usb_otgfs_list)
    if print_list_header("USBDEVICE", "USB_HS", usb_otghs_list, "USBDEVICE"):
        print_usb(usb_otghs_list)
    print_pin_list(gpio_list)
    print_h_file(usb_list, "USB")
    print_h_file(usb_otgfs_list, "USB FS")
    print_h_file(usb_otghs_list, "USB HS")
    print_h_file(eth_list, "ETHERNET")
    print_h_file(osc_list, "OSCILLATOR")
    print_h_file(sys_list, "DEBUG")


def print_list_header(comment, name, l, switch):
    line_to_write = ""
    if len(l)>0:
        if comment:
            line_to_write += "\n//*** %s ***\n" % comment

        line_to_write += "\n"

        if name == "PWM":
            line_to_write += "// %s cannot be used because already used by the us_ticker\n" % TIM_MST
            line_to_write += "// (update us_ticker_data.h file if another timer is chosen)\n"

            default_timer_core2 = ""
            for each_target in TIM_DUALCORE_LIST:
                if each_target in mcu_file:
                    default_timer_core2 = TIM_DUALCORE_LIST[each_target]
            if default_timer_core2 != "":
                line_to_write += "// %s cannot be used because already used by the us_ticker (DUAL_CORE)\n" % default_timer_core2

        if ADD_DEVICE_IF:
            line_to_write += "#if DEVICE_%s\n" % switch

        line_to_write += "MBED_WEAK const PinMap PinMap_%s[] = {\n" % name

    out_c_file.write(line_to_write)
    return len(l)


def print_gpio():

    for parsed_pin in gpio_list:
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        if parsed_pin[1] in PinPuPd:
            commented_line = "//"
        if "OSC" in parsed_pin[2]:
            commented_line = "//"
        line_to_write = "%-11s" % (commented_line + "  {" + parsed_pin[0] + ',')
        line_to_write += ' 0, GPIO_NOPULL},'
        if parsed_pin[1] in PinLabel:
            line_to_write += ' // Connected to ' + PinLabel[parsed_pin[1]]
        if parsed_pin[1] in PinPuPd:
            line_to_write += ' // ' + PinPuPd[parsed_pin[1]]
        if parsed_pin[2] != "":
            line_to_write += ' // ' + parsed_pin[2]
        line_to_write += '\n'
        out_c_file.write(line_to_write)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_adc():
    global ALTERNATE_DEFINITION
    # Check GPIO version (alternate or not)
    s_pin_data = "STM_PIN_DATA_EXT(STM_MODE_ANALOG"
    # For STM32L47xxx/48xxx, it is necessary to configure
    # the GPIOx_ASCR register
    if re.match("STM32L4[78]+", mcu_file):
        s_pin_data += "_ADC_CONTROL"

    prev_p = ''
    alt_index = 0
    for parsed_pin in adclist:
        if "IN" in parsed_pin[2]:
            commented_line = "  "
            if parsed_pin[1] in PinLabel:
                if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                    commented_line = "//"
                if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                    commented_line = "//"
            if commented_line != "//":
                if parsed_pin[0] == prev_p:
                    if "STM32F1" in mcu_file:
                        continue
                    else:
                        prev_p = parsed_pin[0]
                        parsed_pin[0] += '_ALT%d' % alt_index
                        store_pin(parsed_pin[0], parsed_pin[0], "")
                        alt_index += 1
                        if alt_index > ALTERNATE_DEFINITION:
                            ALTERNATE_DEFINITION += 1
                else:
                    prev_p = parsed_pin[0]
                    alt_index = 0
            line_to_write = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
            a = parsed_pin[2].split('_')
            inst = a[0].replace("ADC", "")
            if len(inst) == 0:
                inst = '1' #single ADC for this product
            line_to_write += "%-7s" % ('ADC_' + inst + ',')
            chan = re.sub(r"^IN[N|P]?|\D*$", "", a[1])
            bank = "_ADC_CHANNEL_BANK_B" if a[1].endswith("b") else ""
            line_to_write += s_pin_data + bank + ", GPIO_NOPULL, 0, " + chan
            line_to_write += ', 0)}, // ' + parsed_pin[2]
            if parsed_pin[1] in PinLabel:
                line_to_write += ' // Connected to ' + PinLabel[parsed_pin[1]]
            line_to_write += '\n'
            out_c_file.write(line_to_write)
    out_c_file.write( """    {NC, NC, 0}
};

// !!! SECTION TO BE CHECKED WITH DEVICE REFERENCE MANUAL
MBED_WEAK const PinMap PinMap_ADC_Internal[] = {
//     {ADC_TEMP,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 16, 0)},
//     {ADC_VREF,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 17, 0)},
//     {ADC_VBAT,   ADC_1,    STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, 18, 0)},
    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_dac():
    for parsed_pin in daclist:
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        #parsed_pin[2] : DAC_OUT1 / DAC1_OUT1
        a = parsed_pin[2].split('_')
        inst = a[0].replace("DAC", "")
        b = a[1].replace("OUT", "")
        if len(inst) == 0:
            inst = '1'  # single DAC for this product
        s1 += "%-7s" % ('DAC_' + inst + ',')
        s1 += 'STM_PIN_DATA_EXT(STM_MODE_ANALOG, GPIO_NOPULL, 0, ' + b + ', 0)}, // ' + parsed_pin[2]
        if parsed_pin[1] in PinLabel:
            s1 += ' // Connected to ' + PinLabel[parsed_pin[1]]
        s1 += '\n'
        out_c_file.write(s1)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_i2c(l):
    global ALTERNATE_DEFINITION
    prev_p = ''
    alt_index = 0
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        if commented_line != "//":
            if parsed_pin[0] == prev_p:
                prev_p = parsed_pin[0]
                parsed_pin[0] += '_ALT%d' % alt_index
                store_pin(parsed_pin[0], parsed_pin[0], "")
                alt_index += 1
                if alt_index > ALTERNATE_DEFINITION:
                    ALTERNATE_DEFINITION += 1
            else:
                prev_p = parsed_pin[0]
                alt_index = 0
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : I2C1_SDA / FMPI2C1_SDA
        if "FMP" in parsed_pin[2]:
            inst = parsed_pin[2].split('_')[0].replace("FMPI2C", "")
            s1 += "%-10s" % ('FMPI2C_' + inst + ',')
        else:
            inst = parsed_pin[2].split('_')[0].replace("I2C", "")
            s1 += "%-7s" % ('I2C_' + inst + ',')
        s1 += 'STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, '
        r = result.split(' ')
        for af in r:
            s2 = s1 + af  + ')},'
            if parsed_pin[1] in PinLabel:
                s2 += ' // Connected to ' + PinLabel[parsed_pin[1]]
            s2 += '\n'
            out_c_file.write(s2)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_pwm():
    global ALTERNATE_DEFINITION
    prev_p = ''
    alt_index = 0

    tim_dualcore = "NOT_KNOWN"
    for EachTarget in TIM_DUALCORE_LIST:
        if EachTarget in mcu_file:
            tim_dualcore = TIM_DUALCORE_LIST[EachTarget]

    for parsed_pin in pwm_list:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        if "%s_" % TIM_MST in parsed_pin[2]:
            commented_line = "//"
        if "%s_" % tim_dualcore in parsed_pin[2]:
            commented_line = "//"
        if commented_line != "//":
            if parsed_pin[0] == prev_p:
                prev_p = parsed_pin[0]
                parsed_pin[0] += '_ALT%d' % alt_index
                store_pin(parsed_pin[0], parsed_pin[0], "")
                alt_index += 1
                if alt_index > ALTERNATE_DEFINITION:
                    ALTERNATE_DEFINITION = alt_index
            else:
                prev_p = parsed_pin[0]
                alt_index = 0
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : TIM2_CH1 / TIM15_CH1N
        a = parsed_pin[2].split('_')
        inst = a[0].replace("TIM", "PWM_")
        # if len(inst) == 3:
        #     inst += '1'
        s1 += "%-8s" % (inst + ',')
        chan = a[1].replace("CH", "")
        if chan.endswith('N'):
            neg = ', 1'
            chan = chan.strip('N')
        else:
            neg = ', 0'
        s1 += 'STM_PIN_DATA_EXT(STM_MODE_AF_PP, GPIO_NOPULL, '
        r = result.split(' ')
        prev_s1 = ""
        for af in r:
            if s1 == prev_s1:
                continue
            else:
                prev_s1 = s1
                s2 = s1 + af + ', ' + chan + neg + ')}, // ' + parsed_pin[2]
                if parsed_pin[1] in PinLabel:
                    s2 += ' // Connected to ' + PinLabel[parsed_pin[1]]
                s2 += '\n'
                out_c_file.write(s2)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_uart(l):
    global ALTERNATE_DEFINITION
    prev_p = ''
    alt_index = 0
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        if commented_line != "//":
            if parsed_pin[0] == prev_p:
                prev_p = parsed_pin[0]
                parsed_pin[0] += '_ALT%d' % alt_index
                store_pin(parsed_pin[0], parsed_pin[0], "")
                alt_index += 1
                if alt_index > ALTERNATE_DEFINITION:
                    ALTERNATE_DEFINITION += 1
            else:
                prev_p = parsed_pin[0]
                alt_index = 0
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : USART2_RX
        b=parsed_pin[2].split('_')[0]
        b = b.replace("UART", "UART_")
        b = b.replace("USART", "UART_")
        s1 += "%-10s" % (b[:len(b)-1] +  b[len(b)-1:] + ',')
        if 'STM32F10' in mcu_file and l == uartrx_list:
            s1 += 'STM_PIN_DATA(STM_MODE_INPUT, GPIO_PULLUP, '
        else:
            s1 += 'STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, '
        r = result.split(' ')
        for af in r:
            s2 = s1 + af  + ')},'
            if parsed_pin[1] in PinLabel:
                s2 += ' // Connected to ' + PinLabel[parsed_pin[1]]
            s2 += '\n'
            out_c_file.write(s2)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_spi(l):
    global ALTERNATE_DEFINITION
    prev_p = ''
    alt_index = 0
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        if commented_line != "//":
            if parsed_pin[0] == prev_p:
                prev_p = parsed_pin[0]
                parsed_pin[0] += '_ALT%d' % alt_index
                store_pin(parsed_pin[0], parsed_pin[0], "")
                alt_index += 1
                if alt_index > ALTERNATE_DEFINITION:
                    ALTERNATE_DEFINITION += 1
            else:
                prev_p = parsed_pin[0]
                alt_index = 0
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : SPI1_MISO
        instance=parsed_pin[2].split('_')[0].replace("SPI", "")
        s1 += "%-7s" % ('SPI_' + instance + ',')
        s1 += 'STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, '
        r = result.split(' ')
        for af in r:
            s2 = s1 + af  + ')},'
            if parsed_pin[1] in PinLabel:
                s2 += ' // Connected to ' + PinLabel[parsed_pin[1]]
            s2 += '\n'
            out_c_file.write(s2)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_can(l):
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        s1 = "%-17s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : CAN_RX / CAN1_RX
        parsed_pin[2] = parsed_pin[2].replace("FD", "")
        instance = parsed_pin[2].split('_')[0].replace("CAN", "")
        if len(instance) == 0:
            instance = '1'
        s1 += "%-7s" % ('CAN_' + instance + ',')
        if 'STM32F10' in mcu_file and l == canrd_list:
            s1 += 'STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, '
        else:
            s1 += 'STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, '
        r = result.split(' ')
        for af in r:
            s2 = s1 + af  + ')},'
            if parsed_pin[1] in PinLabel:
                s2 += ' // Connected to ' + PinLabel[parsed_pin[1]]
            s2 += '\n'
            out_c_file.write(s2)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_qspi(l):
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        if "BK2" in parsed_pin[2]: # QSPI Bank 2 is not supported
            continue
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        s1 = "%-16s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : QUADSPI_BK1_IO3 / QUADSPI_CLK / QUADSPI_NCS
        if "OCTOSPIM_P2" in parsed_pin[2]:
            s1 += "%-8s" % 'QSPI_2,'
        else:
            s1 += "%-8s" % 'QSPI_1,'
        result = result.replace("GPIO_AF10_OTG_FS", "GPIO_AF10_QSPI")
        s1 += 'STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, ' + result +')},'
        s1 += '  // ' + parsed_pin[2]
        if parsed_pin[1] in PinLabel:
            s1 += ' // Connected to ' + PinLabel[parsed_pin[1]]
        s1 += '\n'
        out_c_file.write(s1)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_ospi(l):
    for parsed_pin in l:
        result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])
        commented_line = "  "
        if parsed_pin[1] in PinLabel:
            if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
            if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                commented_line = "//"
        s1 = "%-16s" % (commented_line + "  {" + parsed_pin[0] + ',')
        # parsed_pin[2] : QUADSPI_BK1_IO3 / QUADSPI_CLK / QUADSPI_NCS
        if "OCTOSPIM_P2" in parsed_pin[2]:
            s1 += "%-8s" % 'OSPI_2,'
        else:
            s1 += "%-8s" % 'OSPI_1,'
        # result = result.replace("GPIO_AF10_OTG_FS", "GPIO_AF10_QSPI")
        s1 += 'STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, ' + result +')},'
        s1 += '  // ' + parsed_pin[2]
        if parsed_pin[1] in PinLabel:
            s1 += ' // Connected to ' + PinLabel[parsed_pin[1]]
        s1 += '\n'
        out_c_file.write(s1)
    out_c_file.write( """    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_usb(lst):
    use_hs_in_fs = False
    nb_loop = 1
    inst = "USB_FS"
    if lst is usb_otgfs_list:
        inst = "USB_FS"
    elif lst is usb_otghs_list:
        inst = "USB_HS"
        nb_loop = 2

    for nb in range(nb_loop):
        for parsed_pin in lst:
            result = get_gpio_af_num(parsed_pin[1], parsed_pin[2])

            commented_line = "  "

            if parsed_pin[1] in PinLabel:
                if "STDIO_UART" in PinLabel[parsed_pin[1]]:
                    commented_line = "//"
                if "RCC_OSC" in PinLabel[parsed_pin[1]]:
                    commented_line = "//"

            if "_SOF" in parsed_pin[2] or "_NOE" in parsed_pin[2]:
                commented_line = "//"

            if lst is usb_otghs_list:
                if nb == 0:
                    if "ULPI" in parsed_pin[2]:
                        continue
                    elif not use_hs_in_fs:
                        out_c_file.write("#if (MBED_CONF_TARGET_USB_SPEED == USE_USB_HS_IN_FS)\n")
                        use_hs_in_fs = True
                else:
                    if "ULPI" not in parsed_pin[2]:
                        continue
                    elif use_hs_in_fs:
                        out_c_file.write("#else /* MBED_CONF_TARGET_USB_SPEED */\n")
                        use_hs_in_fs = False

            s1 = "%-16s" % (commented_line + "  {" + parsed_pin[0] + ',')

            # 2nd element is the USB_XXXX signal
            if not parsed_pin[2].startswith("USB_D") and "VBUS" not in parsed_pin[2]:
                if "ID" not in parsed_pin[2]:
                    s1 += inst + ", STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, "
                else:
                    # ID pin: AF_PP + PULLUP
                    s1 += inst + ", STM_PIN_DATA(STM_MODE_AF_OD, GPIO_PULLUP, "
            else:
                # USB_DM/DP and VBUS: INPUT + NOPULL
                s1 += inst + ", STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, "
            if result == "NOTFOUND":
                s1 += "0)},"
            else:
                r = result.split(" ")
                for af in r:
                    s1 += af + ")},"
            s1 += " // " + parsed_pin[2]
            if parsed_pin[1] in PinLabel:
                s1 += ' // Connected to ' + PinLabel[parsed_pin[1]]
            s1 += "\n"
            out_c_file.write(s1)
    if lst:
        if lst is usb_otghs_list:
            out_c_file.write("#endif /* MBED_CONF_TARGET_USB_SPEED */\n")
        out_c_file.write("""    {NC, NC, 0}
};
""")
    if ADD_DEVICE_IF:
        out_c_file.write( "#endif\n" )


def print_pin_list(pin_list):
    if ALTERNATE_DEFINITION > 0:
        line_to_write =  ("""    ALT0  = 0x100,""")
        if ALTERNATE_DEFINITION > 1:
            line_to_write += """
    ALT1  = 0x200,"""
        if ALTERNATE_DEFINITION > 2:
            line_to_write += """
    ALT2  = 0x300,"""
        if ALTERNATE_DEFINITION > 3:
            line_to_write += """
    ALT3  = 0x400,"""
        if ALTERNATE_DEFINITION > 4:
            line_to_write += """
    ALT4  = 0x500,"""
        line_to_write += """
} ALTx;

typedef enum {
"""
        out_h_file.write(line_to_write)

    pin_list.sort(key=natural_sortkey)
    previous_pin = ""
    for parsed_pin in pin_list:
        print_debug("pin %s => %s" % (parsed_pin, parsed_pin[0]))
        if parsed_pin[0] == previous_pin:
            continue
        previous_pin = parsed_pin[0]

        if "_ALT" in parsed_pin[0]:
            s1 = "    %-10s = %-5s | %s, // same pin used for alternate HW\n" % (parsed_pin[0], parsed_pin[0].split('_A')[0], parsed_pin[0].split('_')[2])
        elif len(parsed_pin[0]) > 4 and "C" == parsed_pin[0][4]:
            s1 = "    %-10s = %-5s | DUAL_PAD, // dual pad\n" % (parsed_pin[0], parsed_pin[0].split('_A')[0].replace("PC", "PP").replace("C", "").replace("PP", "PC"))
        else:
            pin_value = 0
            if "PA" in parsed_pin[0]:
                pin_value = 0
            elif "PB" in parsed_pin[0]:
                pin_value = 0x10
            elif "PC" in parsed_pin[0]:
                pin_value = 0x20
            elif "PD" in parsed_pin[0]:
                pin_value = 0x30
            elif "PE" in parsed_pin[0]:
                pin_value = 0x40
            elif "PF" in parsed_pin[0]:
                pin_value = 0x50
            elif "PG" in parsed_pin[0]:
                pin_value = 0x60
            elif "PH" in parsed_pin[0]:
                pin_value = 0x70
            elif "PI" in parsed_pin[0]:
                pin_value = 0x80
            elif "PJ" in parsed_pin[0]:
                pin_value = 0x90
            elif "PK" in parsed_pin[0]:
                pin_value = 0xA0
            elif "PZ" in parsed_pin[0]:
                pin_value = 0x0 # to update
            else:
                print("error in print_pin_list with pin %s" % parsed_pin[0])
            pin_value += int(parsed_pin[0].split('_')[1])
            s1 = "    %-10s = 0x%02X,\n" % (parsed_pin[0], pin_value)

        out_h_file.write(s1)

    out_h_file.write("""\n    /**** ADC internal channels ****/

    ADC_TEMP = 0xF0, // Internal pin virtual value
    ADC_VREF = 0xF1, // Internal pin virtual value
    ADC_VBAT = 0xF2, // Internal pin virtual value

#ifdef TARGET_FF_ARDUINO_UNO
    // Arduino Uno (Rev3) pins
    ARDUINO_UNO_A0  = Px_x,
    ARDUINO_UNO_A1  = Px_x,
    ARDUINO_UNO_A2  = Px_x,
    ARDUINO_UNO_A3  = Px_x,
    ARDUINO_UNO_A4  = Px_x,
    ARDUINO_UNO_A5  = Px_x,

    ARDUINO_UNO_D0  = Px_x,
    ARDUINO_UNO_D1  = Px_x,
    ARDUINO_UNO_D2  = Px_x,
    ARDUINO_UNO_D3  = Px_x,
    ARDUINO_UNO_D4  = Px_x,
    ARDUINO_UNO_D5  = Px_x,
    ARDUINO_UNO_D6  = Px_x,
    ARDUINO_UNO_D7  = Px_x,
    ARDUINO_UNO_D8  = Px_x,
    ARDUINO_UNO_D9  = Px_x,
    ARDUINO_UNO_D10 = Px_x,
    ARDUINO_UNO_D11 = Px_x,
    ARDUINO_UNO_D12 = Px_x,
    ARDUINO_UNO_D13 = Px_x,
    ARDUINO_UNO_D14 = Px_x,
    ARDUINO_UNO_D15 = Px_x,
#endif
""")

    s = ("""
    // STDIO for console print
#ifdef MBED_CONF_TARGET_STDIO_UART_TX
    CONSOLE_TX = MBED_CONF_TARGET_STDIO_UART_TX,
#else
    CONSOLE_TX = %s,
#endif
#ifdef MBED_CONF_TARGET_STDIO_UART_RX
    CONSOLE_RX = MBED_CONF_TARGET_STDIO_UART_RX,
#else
    CONSOLE_RX = %s,
#endif
""" % (re.sub(r'(P.)', r'\1_', STDIO_list[0]), re.sub(r'(P.)', r'\1_', STDIO_list[1])))
    out_h_file.write(s)


def print_h_file(pin_list, comment):
    global ALTERNATE_DEFINITION
    pin_list.sort(key=natural_sortkey2)
    if len(pin_list) > 0:
        line_to_write = ("\n    /**** %s pins ****/\n" % comment)
        out_h_file.write(line_to_write)

        prev_s = ''
        alt_index = 0
        for parsed_pin in pin_list:
            if parsed_pin[2] == prev_s:
                prev_s = parsed_pin[2]
                parsed_pin[2] += '_ALT%d' % alt_index
                store_pin(parsed_pin[0], parsed_pin[0], "")
                alt_index += 1
                if alt_index > ALTERNATE_DEFINITION:
                    ALTERNATE_DEFINITION += 1
            else:
                prev_s = parsed_pin[2]
                alt_index = 0
            line_to_write = "    %s = %s,\n" % (parsed_pin[2].replace("-", "_"), parsed_pin[0])
            out_h_file.write(line_to_write)


tokenize = re.compile(r"(\d+)|(\D+)").findall


def natural_sortkey(list_2_elem):
    return tuple(int(num) if num else alpha for num, alpha in tokenize(list_2_elem[0]))

def natural_sortkey2(list_2_elem):
    return tuple(int(num) if num else alpha for num, alpha in tokenize(list_2_elem[2]))

def natural_sortkey_uart(list_2_elem):
    return tuple(int(num) if num else alpha for num, alpha in tokenize(list_2_elem[2].replace("USART", "UART").replace("LPUART", "ZUART")))

def natural_sortkey_i2c(list_2_elem):
    return tuple(int(num) if num else alpha for num, alpha in tokenize(list_2_elem[2].replace("FMPI2C", "ZFMPI2C")))

def sort_my_lists():
    gpio_list.sort(key=natural_sortkey)
    adclist.sort(key=natural_sortkey)
    daclist.sort(key=natural_sortkey)
    i2cscl_list.sort(key=natural_sortkey_i2c) # first sort on name column
    i2csda_list.sort(key=natural_sortkey_i2c) # first sort on name column
    i2cscl_list.sort(key=natural_sortkey)
    i2csda_list.sort(key=natural_sortkey)
    pwm_list.sort(key=natural_sortkey2) # first sort on name column
    pwm_list.sort(key=natural_sortkey)
    uarttx_list.sort(key=natural_sortkey_uart) # first sort on name column
    uartrx_list.sort(key=natural_sortkey_uart) # first sort on name column
    uartcts_list.sort(key=natural_sortkey_uart) # first sort on name column
    uartrts_list.sort(key=natural_sortkey_uart) # first sort on name column
    uarttx_list.sort(key=natural_sortkey)
    uartrx_list.sort(key=natural_sortkey)
    uartcts_list.sort(key=natural_sortkey)
    uartrts_list.sort(key=natural_sortkey)
    spimosi_list.sort(key=natural_sortkey)
    spimiso_list.sort(key=natural_sortkey)
    spissel_list.sort(key=natural_sortkey)
    spisclk_list.sort(key=natural_sortkey)
    cantd_list.sort(key=natural_sortkey)
    canrd_list.sort(key=natural_sortkey)
    quadspidata0_list.sort(key=natural_sortkey)
    quadspidata1_list.sort(key=natural_sortkey)
    quadspidata2_list.sort(key=natural_sortkey)
    quadspidata3_list.sort(key=natural_sortkey)
    quadspisclk_list.sort(key=natural_sortkey)
    quadspissel_list.sort(key=natural_sortkey)
    octospidata0_list.sort(key=natural_sortkey)
    octospidata1_list.sort(key=natural_sortkey)
    octospidata2_list.sort(key=natural_sortkey)
    octospidata3_list.sort(key=natural_sortkey)
    octospidata4_list.sort(key=natural_sortkey)
    octospidata5_list.sort(key=natural_sortkey)
    octospidata6_list.sort(key=natural_sortkey)
    octospidata7_list.sort(key=natural_sortkey)
    octospidqs_list.sort(key=natural_sortkey)
    octospisclk_list.sort(key=natural_sortkey)
    octospissel_list.sort(key=natural_sortkey)
    usb_list.sort(key=natural_sortkey)
    usb_otgfs_list.sort(key=natural_sortkey)
    usb_otghs_list.sort(key=natural_sortkey)

def clean_all_lists():
    del gpio_list[:]
    del adclist[:]
    del daclist[:]
    del i2cscl_list[:]
    del i2csda_list[:]
    del pwm_list[:]
    del uarttx_list[:]
    del uartrx_list[:]
    del uartcts_list[:]
    del uartrts_list[:]
    del spimosi_list[:]
    del spimiso_list[:]
    del spissel_list[:]
    del spisclk_list[:]
    del cantd_list[:]
    del canrd_list[:]
    del eth_list[:]
    del quadspidata0_list[:]
    del quadspidata1_list[:]
    del quadspidata2_list[:]
    del quadspidata3_list[:]
    del quadspisclk_list[:]
    del quadspissel_list[:]
    del octospidata0_list[:]
    del octospidata1_list[:]
    del octospidata2_list[:]
    del octospidata3_list[:]
    del octospidata4_list[:]
    del octospidata5_list[:]
    del octospidata6_list[:]
    del octospidata7_list[:]
    del octospidqs_list[:]
    del octospisclk_list[:]
    del octospissel_list[:]
    del usb_list[:]
    del usb_otgfs_list[:]
    del usb_otghs_list[:]
    del osc_list[:]
    del sys_list[:]


def parse_pins():
    global DUAL_PAD
    pinregex = r"^(P[A-Z][0-9][0-5]?[_]?[C]?)"
    itemlist = xml_mcu.getElementsByTagName("Pin")
    for s in itemlist:
        if "Variant" in s.attributes:
            continue
        pinregex_match = re.match(pinregex, s.attributes["Name"].value)
        if pinregex_match:
            pin = (
                pinregex_match.group(0)[:2] + "_" + pinregex_match.group(0)[2:].replace("_", "")
            )  # pin formatted P<port>_<number>: PF_O
            name = s.attributes["Name"].value.strip()  # full name: "PF0 / OSC_IN"
            if "_C" in name:
                DUAL_PAD = True

            if s.attributes["Type"].value == "I/O":
                if "-" in s.attributes["Name"].value:
                    store_pin(pin, name, s.attributes["Name"].value)
                else:
                    store_pin(pin, name, "")
                if DUAL_PAD:
                    if "_C" in name:
                        store_pin(pin.replace("2C", "2").replace("3C", "3"), name, "")
            else:
                continue
            siglist = s.getElementsByTagName("Signal")
            for a in siglist:
                sig = a.attributes["Name"].value.strip()
                if "ADC" in sig:
                    store_adc(pin, name, sig)
                if all(["DAC" in sig, "_OUT" in sig]):
                    store_dac(pin, name, sig)
                if "I2C" in sig:
                    store_i2c(pin, name, sig)
                if re.match("^TIM", sig) is not None:  # ignore HRTIM
                    store_pwm(pin, name, sig)
                if re.match("^(LPU|US|U)ART", sig) is not None:
                    store_uart(pin, name, sig)
                if "SPI" in sig:
                    store_spi(pin, name, sig)
                if "CAN" in sig:
                    store_can(pin, name, sig)
                if "ETH" in sig:
                    store_eth(pin, name, sig)
                if "QUADSPI" in sig or "OCTOSPI" in sig:
                    store_qspi(pin, name, sig)
                if "OCTOSPI" in sig:
                    store_ospi(pin, name, sig)
                if "USB" in sig:
                    store_usb(pin, name, sig)
                if "RCC_OSC" in sig:
                    store_osc(pin, name, sig)
                if "SYS_" in sig or "PWR_" in sig or "DEBUG_" in sig:
                    store_sys(pin, name, sig)


PinData = {}
PinLabel = {}
PinPuPd = {}


def parse_board_file(file_name):
    global MCU_USERNAME
    print(" * Board file: '%s'" % file_name)
    board_file = open(file_name, "r")
    ioc_pin_pattern = re.compile(r'(.*)\.([\w]*)=(.*)')
    for line in board_file.readlines():
        ioc_pin_match = re.match(ioc_pin_pattern, line)
        if ioc_pin_match:
            if ioc_pin_match.groups()[0] in PinData:
                PinData[ioc_pin_match.groups()[0]][ioc_pin_match.groups()[1]] = ioc_pin_match.groups()[2]
            else:
                PinData[ioc_pin_match.groups()[0]] = {}
                PinData[ioc_pin_match.groups()[0]][ioc_pin_match.groups()[1]] = ioc_pin_match.groups()[2]
        ioc_mcu_match = re.match(r'Mcu\.Name=(.*)', line)
        if ioc_mcu_match:
            mcu_list.append("%s.xml" % ioc_mcu_match.groups()[0])
        ioc_mcu_match = re.match(r'Mcu\.UserName=(.*)', line)
        if ioc_mcu_match:
            MCU_USERNAME = ioc_mcu_match.groups()[0]

    board_file.close()

    for EachPin in PinData:
        PinLabel[EachPin] = ""
        if "Signal" in PinData[EachPin]:
            PinLabel[EachPin] = PinData[EachPin]["Signal"]
        if "GPIO_Label" in PinData[EachPin]:
            PinLabel[EachPin] = PinData[EachPin]["GPIO_Label"]
        if "GPIO_PuPdOD" in PinData[EachPin]:
            if PinData[EachPin]["GPIO_PuPdOD"] == "GPIO_PULLUP":
                PinPuPd[EachPin] = "PULLUP"
            elif PinData[EachPin]["GPIO_PuPdOD"] == "GPIO_NOPULL":
                pass
            else:
                print("!!! error SCRIPT ISSUE with %s for %s" % (PinData[EachPin]["GPIO_PuPdOD"], EachPin))
        if any(led in PinLabel[EachPin].upper() for led in
               ["LED", "LD1", "LD2", "LD3", "LD4", "LD5", "LD6", "LD7", "LD8", "LD9"]):
            LED_list.append(EachPin)
        elif any(button in PinLabel[EachPin].upper() for button in ["BUTTON", "B_USER", "BTN"]):
            BUTTON_list.append(EachPin)

        uart_hw_option = "NO_NEED"
        for each_target in VCP_UART_LIST:
            if each_target in file_name:
                uart_hw_option = VCP_UART_LIST[each_target]

        try:
            if "STLK_RX" in PinLabel[EachPin] or "STLK_TX" in PinLabel[EachPin]:
                # Patch waiting for CubeMX correction
                if "RX" in PinData[EachPin]["Signal"]:
                    PinLabel[EachPin] = "STDIO_UART_RX"
                    STDIO_list[1] = EachPin
                else:
                    PinLabel[EachPin] = "STDIO_UART_TX"
                    STDIO_list[0] = EachPin
            elif "USART_RX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_RX"
                STDIO_list[1] = EachPin
            elif "USART_TX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_TX"
                STDIO_list[0] = EachPin
            elif "VCP_RX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_RX"
                STDIO_list[1] = EachPin
            elif "VCP_TX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_TX"
                STDIO_list[0] = EachPin
            elif "ST_LINK_UART1_RX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_RX"
                STDIO_list[1] = EachPin
            elif "ST_LINK_UART1_TX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_TX"
                STDIO_list[0] = EachPin
            elif "ST-LINK-UART1_RX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_RX"
                STDIO_list[1] = EachPin
            elif "ST-LINK-UART1_TX" in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_TX"
                STDIO_list[0] = EachPin
            elif "STLINK_RX" in PinLabel[EachPin] or "STLINK_TX" in PinLabel[EachPin]:
                # Patch waiting for CubeMX correction
                if "RX" in PinData[EachPin]["Signal"]:
                    PinLabel[EachPin] = "STDIO_UART_RX"
                    STDIO_list[1] = EachPin
                else:
                    PinLabel[EachPin] = "STDIO_UART_TX"
                    STDIO_list[0] = EachPin
            elif "%s_RX" % uart_hw_option in PinLabel[EachPin]:
                PinLabel[EachPin] = "STDIO_UART_RX"
                STDIO_list[1] = EachPin
            elif "%s_TX" % uart_hw_option in PinLabel[EachPin]:
                STDIO_list[0] = EachPin
                PinLabel[EachPin] = "STDIO_UART_TX"
            elif "_RESERVED" in PinLabel[EachPin]:
                PinLabel[EachPin] = "RESERVED_RADIO"
        except:
            pass


# main
print ("\nScript version %s" % GENPINMAP_VERSION)
cur_dir = os.getcwd()
PeripheralPins_c_filename = "PeripheralPins.c"
PinNames_h_filename = "PinNames.h"


parser = argparse.ArgumentParser(
    description=textwrap.dedent('''\
Script will generate %s thanks to the xml files description available in STM32_open_pin_data GitHub repo\n
More information in targets/TARGET_STM/README.md''' % (PeripheralPins_c_filename)),
    epilog=textwrap.dedent('''\
Once generated, you have to check and comment pins that can not be used (specific HW, internal ADC channels, remove PWM using us ticker timer, ...)
'''),
    formatter_class=RawTextHelpFormatter)

group = parser.add_mutually_exclusive_group(required=True)

group.add_argument("-l", "--list", help="list available mcu xml files description in STM32CubeMX", action="store_true")

group.add_argument("-b", "--boards", help="list available boards description in STM32CubeMX", action="store_true")

group.add_argument("-m", "--mcu", metavar='xml', help=textwrap.dedent('''\
specify the mcu xml file description in STM32CubeMX to use (use double quotes).
   Parameter can be a filter like L496 if you want to parse all L496 chips (-m STM32 to parse all).
'''))

group.add_argument("-t", "--target", metavar='HW', help=textwrap.dedent('''\
specify the board file description in STM32CubeMX to use (use double quotes).
   Parameter can be a filter like L496 (only the first file found will be parsed).
'''))

group.add_argument("-c", "--custom", help=textwrap.dedent('''\
specify a custom board .ioc file description to use (use double quotes).
'''))

parser.add_argument("-g", "--gpio", help="Add GPIO PinMap table", action="store_true")
parser.add_argument("-n", "--nopull", help="Avoid STM32_open_pin_data git pull", action="store_true")
parser.add_argument("-f", "--flat", help="All targets stored in targets_custom/TARGET_STM/", action="store_true")

args = parser.parse_args()

print ("\nChecking STM32_open_pin_data repo...")
if not os.path.exists("STM32_open_pin_data"):
    print("*** git clone https://github.com/STMicroelectronics/STM32_open_pin_data.git ***")
    try:
        CONSOLE = subprocess.check_output(["git", "clone", r"https://github.com/STMicroelectronics/STM32_open_pin_data.git"], stderr=subprocess.STDOUT)
        print("*** git clone done\n")
        # print(CONSOLE)
    except:
        print("!!! Repo clone error !!!")
else:
    if args.nopull:
        print("  ... skipped\n")
    else:
        try:
            os.chdir("STM32_open_pin_data")
            CONSOLE = subprocess.check_output(["git", "pull"], stderr=subprocess.STDOUT).decode('ascii')
            print("\t%s" % CONSOLE)
            os.chdir("..")
        except:
            print("!!! git pull issue !!!")
            exit(3)


cubemxdirMCU = os.path.join("STM32_open_pin_data", "mcu")
cubemxdirIP = os.path.join("STM32_open_pin_data", "mcu", "IP")
cubemxdirBOARDS = os.path.join("STM32_open_pin_data", "boards")

os.chdir("STM32_open_pin_data")
# print("*** git tag ***")
CONSOLE = subprocess.check_output(["git", "tag"], stderr=subprocess.STDOUT).decode('ascii')
# print(CONSOLE)
VERSION_LIST=CONSOLE.splitlines()
# print("\t%s" % VERSION_LIST)
cubemx_db_version = VERSION_LIST[-1]
os.chdir("..")
print ("STM32_open_pin_data DB version %s\n" % cubemx_db_version)

if args.gpio:
    ADD_GPIO_PINMAP = 1

if args.flat:
    FLAT_DIRECTORY = 1

if args.list:
    file_count = 0
    for f in fnmatch.filter(os.listdir(cubemxdirMCU), "STM32*.xml"):
        print(f)
        file_count += 1
    print()
    print("%i available xml files description" % file_count)
    sys.exit(0)

if args.boards:
    NucleoFileCount = 0
    DiscoFileCount = 0
    for f in fnmatch.filter(os.listdir(cubemxdirBOARDS), '*AllConfig.ioc'):
        print(f)
        if "Nucleo" in f:
            NucleoFileCount += 1
        elif "Discovery" in f:
            DiscoFileCount += 1
    print()
    print("%2i available Nucleo files description" % NucleoFileCount)
    print("%2i available Disco  files description" % DiscoFileCount)
    sys.exit(0)

if args.mcu:
    #check input file exists
    if os.path.isfile(os.path.join(cubemxdirMCU, args.mcu)):
        mcu_list.append(args.mcu)
    else:
        mcu_list = fnmatch.filter(os.listdir(cubemxdirMCU), '*%s*' % args.mcu)
        if len(mcu_list) == 0:
            print (" ! ! ! " + args.mcu + " file not found")
            print (" ! ! ! Check in " + cubemxdirMCU + " the correct name of this file")
            print (" ! ! ! You may use double quotes for this file if it contains special characters")
            sys.exit(1)

if args.target:
    board_file_name = os.path.join(cubemxdirBOARDS, args.target)
    if not(os.path.isfile(board_file_name)):
        board_list = fnmatch.filter(os.listdir(cubemxdirBOARDS), '*%s*AllConfig.ioc' % args.target)
        if len(board_list) == 0:
            print (" ! ! ! No file contains " + args.target)
            print (" ! ! ! Check in " + cubemxdirBOARDS + " the correct filter to apply")
            sys.exit(1)
        elif len(board_list) > 1:
            print (" ! ! ! Multiple files contains " + args.target)
            for board_elem in board_list: print (board_elem)
            print (" ! ! ! Only the first one will be parsed\n")
        board_file_name = os.path.join(cubemxdirBOARDS,board_list[0])
        if not (os.path.isfile(board_file_name)):
            print (" ! ! ! " + args.target + " file not found")
            print (" ! ! ! Check in " + cubemxdirBOARDS + " the correct name of this file")
            print (" ! ! ! You may use double quotes for this file if it contains special characters")
            sys.exit(1)

    # Add some hardcoded check
    if "J01_" in board_file_name:
        print("J01_Discovery_STM32F4-DISCO-AudioPack_STM32F407V_Board not parsed")
        sys.exit(0)
    elif "G00_" in board_file_name:
        print("G00_Nucleo_NUCLEO-WB52VGY_STM32WB52VGY_Board not parsed")
        sys.exit(0)
    elif "C40_" in board_file_name:
        print("C40_Discovery_STM32F4DISCOVERY_STM32F407VG_Board replaced by C47_Discovery_STM32F407G-DISC1_STM32F407VG_Board")
        sys.exit(0)
    elif "P-NUCLEO-WB55" in board_file_name:
        print("Same board as NUCLEO-WB55 (J02)")
        sys.exit(0)
    elif "MultiToSingleCore_Board" in board_file_name:
        print("Same board as PL0_Nucleo_NUCLEO-WL55JC1_STM32WL55JCI_Board_AllConfig.ioc")
        sys.exit(0)
    elif "WL55JC2" in board_file_name:
        print("Same board as PL0_Nucleo_NUCLEO-WL55JC1_STM32WL55JCI_Board_AllConfig.ioc")
        sys.exit(0)
    elif "B-L475E-IOT01A2" in board_file_name:
        print("Same board as B-L475E-IOT01A1 (42)")
        sys.exit(0)
    elif "USBDongle" in board_file_name:
        print("USB dongle not parsed")
        sys.exit(0)
    elif "TrustZoneEnabled" in board_file_name:
        print("TrustZoneEnabled boards not parsed")
        sys.exit(0)

    parse_board_file(board_file_name)
    if "Nucleo" in board_file_name:
        TARGET_NAME += "NUCLEO_"
    elif "Discovery" in board_file_name:
        TARGET_NAME += "DISCO_"
    elif "Evaluation" in board_file_name:
        TARGET_NAME += "EVAL_"
    m = re.search(r'STM32([MFLGWH][\w]*)_Board', board_file_name)
    if m:
        TARGET_NAME += "%s" % m.group(1)
        # specific case
        if "-P" in board_file_name:
            TARGET_NAME += "_P"
        elif "-Q" in board_file_name:
            TARGET_NAME += "_Q"

        target_rename = {  # manual renaming for some boards
            "DISCO_L072C": "DISCO_L072CZ_LRWAN1",
            "DISCO_L475V": "DISCO_L475VG_IOT01A",
            "DISCO_L4S5V": "B_L4S5I_IOT01A",
            "DISCO_G071RBT": "DISCO_G071RB",
            "DISCO_L4R9A": "DISCO_L4R9I",
            "NUCLEO_WB55R": "NUCLEO_WB55RG",
            "NUCLEO_WL55JCI": "NUCLEO_WL55JC",
            "NUCLEO_H743ZIT": "NUCLEO_H743ZI2",
            "NUCLEO_H7A3ZIT_Q": "NUCLEO_H7A3ZI_Q",
            "DISCO_F0DISCOVERY_STM32F051R8": "DISCO_F051R8",
            "DISCO_F3DISCOVERY_STM32F303VC": "DISCO_F303VC",
            "DISCO_F469NIH": "DISCO_F469NI",
            "DISCO_F412ZGT": "DISCO_F412ZG",
            "DISCO_F746NGH": "DISCO_F746NG",
            "DISCO_F769NIH": "DISCO_F769NI",
            "DISCO_H747XIH": "DISCO_H747I"
        }

        if TARGET_NAME in target_rename:
            TARGET_NAME = target_rename[TARGET_NAME]

        if "DISC1" in board_file_name:
            TARGET_NAME += "_DISC1"

    else:
        sys.exit(1)

# Parse the user's custom board .ioc file
if args.custom:
    parse_board_file(args.custom)

for mcu_file in mcu_list:
    TargetNameList = []

    # print("--- mcu_file %s ---" %(mcu_file))
    m2 = re.match("(.*)\(([\w])\-([\w])\)(.*)", mcu_file)
    m3 = re.match("(.*)\(([\w])\-([\w])\-([\w])\)(.*)", mcu_file)
    m4 = re.match("(.*)\(([\w])\-([\w])\-([\w])\-([\w])\)(.*)", mcu_file)
    m5 = re.match("(.*)\(([\w])\-([\w])\-([\w])\-([\w])\-([\w])\)(.*)", mcu_file)
    if m2:
        new_mcu_file = m2.group(1) + m2.group(2) + m2.group(4)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m2.group(1) + m2.group(3) + m2.group(4)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
    elif m3:
        new_mcu_file = m3.group(1) + m3.group(2) + m3.group(5)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m3.group(1) + m3.group(3) + m3.group(5)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m3.group(1) + m3.group(4) + m3.group(5)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
    elif m4:
        new_mcu_file = m4.group(1) + m4.group(2) + m4.group(6)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m4.group(1) + m4.group(3) + m4.group(6)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m4.group(1) + m4.group(4) + m4.group(6)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m4.group(1) + m4.group(5) + m4.group(6)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
    elif m5:
        new_mcu_file = m5.group(1) + m5.group(2) + m5.group(7)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m5.group(1) + m5.group(3) + m5.group(7)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m5.group(1) + m5.group(4) + m5.group(7)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m5.group(1) + m5.group(5) + m5.group(7)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
        new_mcu_file = m5.group(1) + m5.group(6) + m5.group(7)
        TargetNameList.append(os.path.splitext(new_mcu_file)[0])
    elif "(" in mcu_file:
        print("!!! error SCRIPT ISSUE with file %s" % mcu_file)
        sys.exit(4)
    else:
        TargetNameList.append(os.path.splitext(mcu_file)[0])

    for EachTargetName in TargetNameList:
        # print("EachTargetName %s" % EachTargetName)
        m = re.match("(STM32[\w]{2})", EachTargetName)
        if m:
            TARGET_FAMILY = m.group(0)
        else:
            print("!!! no TARGET_FAMILY")
            sys.exit(2)

        SearchSubFamily = EachTargetName[:9] + 'x' + EachTargetName[10:]
        m = re.match("(STM32[\w]{6})", SearchSubFamily)
        if m:
            TARGET_SUBFAMILY = m.group(0)
        else:
            print("!!! no TARGET_SUBFAMILY")
            sys.exit(2)

        if args.mcu:
            m = re.match("(STM32[\w]{7})", EachTargetName)
            if m:
                out_path = os.path.join(cur_dir, 'targets_custom', 'TARGET_STM', 'TARGET_%s' %TARGET_FAMILY, 'TARGET_%s' %TARGET_SUBFAMILY, 'TARGET_%s' % m.group(0))
                if EachTargetName.endswith('A'):
                    out_path += "_A"
                elif EachTargetName.endswith('P'):
                    out_path += "_P"
                elif EachTargetName.endswith('Q'):
                    out_path += "_Q"
                elif EachTargetName.endswith('N'):
                    out_path += "_N"
                elif EachTargetName.endswith('S'):
                    out_path += "_S"
                elif EachTargetName.endswith('X'):
                    out_path += "_X"
            else:
                print("!!! Warning output directory not found")
                sys.exit(8)
        else:
            if EachTargetName == MCU_USERNAME:
                if FLAT_DIRECTORY == 0:
                    out_path = os.path.join(cur_dir, 'targets_custom', 'TARGET_STM', 'TARGET_%s' % TARGET_FAMILY, 'TARGET_%s' % TARGET_SUBFAMILY, 'TARGET_%s' % TARGET_NAME)
                else:
                    out_path = os.path.join(cur_dir, 'targets_custom', 'TARGET_STM', 'TARGET_%s' % TARGET_NAME)
            else:
                continue

        print(" * Output directory: %s" % out_path)
        if not (os.path.isdir(out_path)):
            os.makedirs(out_path)
        else:
            print("!!! %s already exist" % out_path)

        input_file_name = os.path.join(cubemxdirMCU, mcu_file)
        print(" * Generating %s and %s with '%s'" % (PeripheralPins_c_filename, PinNames_h_filename, input_file_name))
        output_cfilename = os.path.join(out_path, PeripheralPins_c_filename)
        output_hfilename = os.path.join(out_path, PinNames_h_filename)

        if os.path.isfile(output_cfilename):
            print_debug(" * Requested %s file already exists and will be overwritten" % PeripheralPins_c_filename)
            os.remove(output_cfilename)
        out_c_file = open(output_cfilename, 'w')
        out_h_file = open(output_hfilename, 'w')

        #open input file
        try:
            xml_mcu = parse(input_file_name)
        except:
            # Patch waiting for CubeMX correction
            if "STM32F042K6Tx" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32F042K(4-6)Tx.xml")
                xml_mcu = parse(input_file_name)
            elif "STM32F429Z" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32F429ZITx.xml")
                xml_mcu = parse(input_file_name)
            elif "STM32F746Z" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32F746ZGTx.xml")
                xml_mcu = parse(input_file_name)
            elif "STM32F767Z" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32F767ZGTx.xml")
                xml_mcu = parse(input_file_name)

            elif "STM32L011K4Tx" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32L011K(3-4)Tx.xml")
                xml_mcu = parse(input_file_name)
            elif "STM32L432KCUx" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32L432K(B-C)Ux.xml")
                xml_mcu = parse(input_file_name)
            elif "STM32F746N" in input_file_name:
                input_file_name = os.path.join(cubemxdirMCU, "STM32F746NGHx.xml")
                xml_mcu = parse(input_file_name)
            else:
                print ("\n ! ! ! Error in CubeMX file. File " + input_file_name + " doesn't exist")
                print (" ! ! ! Check in " + cubemxdirMCU)
                sys.exit(1)
        gpiofile = find_gpio_file()
        if gpiofile == "ERROR":
            print("error: Could not find GPIO file")
            sys.exit(1)
        xml_gpio = parse(os.path.join(cubemxdirIP, "GPIO-" + gpiofile + "_Modes.xml"))
        print (" * GPIO file: " + os.path.join(cubemxdirIP, "GPIO-" + gpiofile + "_Modes.xml"))

        find_tim_mst()

        parse_pins()
        sort_my_lists()
        print_header()
        print_all_lists()
        print_footer()

        nb_pin = (len(gpio_list))
        nb_connected_pin = len(PinLabel)
        print (" * I/O pins found: %i connected: %i\n" % (nb_pin, nb_connected_pin))
        clean_all_lists()

        out_c_file.close()
        out_h_file.close()
