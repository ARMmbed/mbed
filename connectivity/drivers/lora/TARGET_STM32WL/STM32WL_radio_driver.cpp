/* mbed Microcontroller Library
 * SPDX-License-Identifier: BSD-3-Clause
 ******************************************************************************
 *
 * Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#include "STM32WL_radio_driver.h"
#include "drivers/DigitalOut.h"


/* Sets up radio switch position according to the radio mode */
/* This configuration is for NUCLEO_WL55JC */
/* But provided as __weak so it has to be overwritten to match each specicific HW board */
MBED_WEAK void set_antenna_switch(RBI_Switch_TypeDef state)
{

    // Radio specific controls (TX/RX duplexer switch control)
    mbed::DigitalOut _rf_switch_ctrl1(MBED_CONF_STM32WL_LORA_DRIVER_RF_SWITCH_CTL1);
    mbed::DigitalOut _rf_switch_ctrl2(MBED_CONF_STM32WL_LORA_DRIVER_RF_SWITCH_CTL2);
    mbed::DigitalOut _rf_switch_ctrl3(MBED_CONF_STM32WL_LORA_DRIVER_RF_SWITCH_CTL3);

    switch (state) {
        case RBI_SWITCH_OFF: {
            /* Turn off switch */
            _rf_switch_ctrl3 = 0;
            _rf_switch_ctrl1 = 0;
            _rf_switch_ctrl2 = 0;
            break;
        }
        case RBI_SWITCH_RX: {
            /*Turns On in Rx Mode the RF Switch */
            _rf_switch_ctrl3 = 1;
            _rf_switch_ctrl1 = 1;
            _rf_switch_ctrl2 = 0;
            break;
        }
        case RBI_SWITCH_RFO_LP: {
            /*Turns On in Tx Low Power the RF Switch */
            _rf_switch_ctrl3 = 1;
            _rf_switch_ctrl1 = 1;
            _rf_switch_ctrl2 = 1;
            break;
        }
        case RBI_SWITCH_RFO_HP: {
            /*Turns On in Tx High Power the RF Switch */
            _rf_switch_ctrl3 = 1;
            _rf_switch_ctrl1 = 0;
            _rf_switch_ctrl2 = 1;
            break;
        }
        default:
            break;
    }
}

