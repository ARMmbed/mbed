/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2017, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */
#include "mbed_assert.h"
#include "pinmap.h"
#include "PortNames.h"
#include "mbed_error.h"
#include "pin_device.h"

extern GPIO_TypeDef *Set_GPIO_Clock(uint32_t port_idx);

#if defined(DUAL_PAD) // STM32H7

typedef struct {
  PinName pin;
  uint32_t LL_AnalogSwitch;
} PinAnalogSwitch;


const PinAnalogSwitch PinMapAnalogSwitch[] = {
  {PA_0,     LL_SYSCFG_ANALOG_SWITCH_PA0},
  {PA_1,     LL_SYSCFG_ANALOG_SWITCH_PA1},
  {PC_2,     LL_SYSCFG_ANALOG_SWITCH_PC2},
  {PC_3,     LL_SYSCFG_ANALOG_SWITCH_PC3},
  {NC,   0}
};

/**
 * Configure Analog dualpad switch if necessary
 * LL_AnalogSwitch: LL define to be used to configure Analog switch
 */
static void configure_dualpad_switch(PinName pin, int function, uint32_t LL_AnalogSwitch)
{
  if (LL_AnalogSwitch == 0) {
    return ;
  }

  if (((function & STM_MODE_ANALOG) != STM_MODE_ANALOG)
      && ((pin & DUAL_PAD) == DUAL_PAD)) {
    /**
      * We don't configure an analog function but the pin is an analog pad
      * Pxy_C. In this cases Analog switch should be closed
      */
    LL_SYSCFG_CloseAnalogSwitch(LL_AnalogSwitch);
    return ;
  } else {
    /**
      * Either we configure an analog function,
      * or it is not an analog function but it is not an analog pad Pxy_C.
      * In both cases Analog switch should be opened
      * Note: direct ADC is restricted to Pxy_C,  pin only
      */
    LL_SYSCFG_OpenAnalogSwitch(LL_AnalogSwitch);
    return ;
  }
}

/**
 * In case of dual pad, determine whether gpio needs to be configured
 * pLL_AnalogSwitch: pointer used to retrun LL define to be used to configure
 * Analog switch
 * return: true when gpio must be configured
 */
static bool is_dualpad_switch_gpio_configurable(PinName pin, int function, uint32_t *pLL_AnalogSwitch)
{
  PinAnalogSwitch *AnalogSwitch = (PinAnalogSwitch *) PinMapAnalogSwitch;

  /* Read through PinMapAnalogSwitch array */
  while (AnalogSwitch->pin != NC) {
    /* Check whether pin is or is associated to dualpad Analog Input */
    if ((AnalogSwitch->pin | DUAL_PAD)  == (pin | DUAL_PAD)) {
      *pLL_AnalogSwitch = AnalogSwitch->LL_AnalogSwitch;
      if (((function & STM_MODE_ANALOG) == STM_MODE_ANALOG)
          && ((pin & DUAL_PAD) == DUAL_PAD)) {
        /**
         * We configure an analog function and the pin is an analog pad Pxy_C
         * In this case gpio configuration must be skipped
         */
        return false;
      } else {
        return true;
      }
    }
    AnalogSwitch ++;
  }
  *pLL_AnalogSwitch = 0;
  return true;
}
#endif /* DUAL_PAD */

const uint32_t ll_pin_defines[16] = {
    LL_GPIO_PIN_0,
    LL_GPIO_PIN_1,
    LL_GPIO_PIN_2,
    LL_GPIO_PIN_3,
    LL_GPIO_PIN_4,
    LL_GPIO_PIN_5,
    LL_GPIO_PIN_6,
    LL_GPIO_PIN_7,
    LL_GPIO_PIN_8,
    LL_GPIO_PIN_9,
    LL_GPIO_PIN_10,
    LL_GPIO_PIN_11,
    LL_GPIO_PIN_12,
    LL_GPIO_PIN_13,
    LL_GPIO_PIN_14,
    LL_GPIO_PIN_15
};

/**
 * Configure pin (mode, speed, output type and pull-up/pull-down)
 */
void pin_function(PinName pin, int data)
{
    if (pin == NC) {
        return;
    }

    // Get the pin informations
    uint32_t mode  = STM_PIN_FUNCTION(data);
    uint32_t afnum = STM_PIN_AFNUM(data);
    uint32_t speed = STM_PIN_SPEED(data);
    uint32_t port = STM_PORT(pin);
    uint32_t ll_pin  = ll_pin_defines[STM_PIN(pin)];
    uint32_t ll_mode = 0;

#if defined(DUAL_PAD)
  uint32_t LL_AnalogSwitch = 0;
  if (!is_dualpad_switch_gpio_configurable(pin, data, &LL_AnalogSwitch)) {
    /* Skip gpio configuration */
    configure_dualpad_switch(pin, data, LL_AnalogSwitch);
    return;
  }
#endif /* DUAL_PAD */

    // Enable GPIO clock
    GPIO_TypeDef *const gpio = Set_GPIO_Clock(port);

    /*  Set default speed to high.
     *  For most families there are dedicated registers so it is
     *  not so important, register can be set at any time.
     *  But for families like F1, speed only applies to output.
     */
#if defined (TARGET_STM32F1)
    if (mode == STM_PIN_OUTPUT) {
#endif
#if defined(DUAL_CORE) && (TARGET_STM32H7)
        while (LL_HSEM_1StepLock(HSEM, CFG_HW_GPIO_SEMID)) {
        }
#endif /* DUAL_CORE */
        switch (speed) {
            /* Default value for backward compatibility */
            case STM_PIN_SPEED_MASK:
#if defined (LL_GPIO_SPEED_FREQ_VERY_HIGH)
                LL_GPIO_SetPinSpeed(gpio, ll_pin, LL_GPIO_SPEED_FREQ_VERY_HIGH);
#else
                LL_GPIO_SetPinSpeed(gpio, ll_pin, LL_GPIO_SPEED_FREQ_HIGH);
#endif
                break;
            default:
                LL_GPIO_SetPinSpeed(gpio, ll_pin, speed);
                break;
        }
#if defined(DUAL_CORE) && (TARGET_STM32H7)
        LL_HSEM_ReleaseLock(HSEM, CFG_HW_GPIO_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
#if defined (TARGET_STM32F1)
    }
#endif

    switch (mode) {
        case STM_PIN_INPUT:
            ll_mode = LL_GPIO_MODE_INPUT;
            break;
        case STM_PIN_OUTPUT:
            ll_mode = LL_GPIO_MODE_OUTPUT;
            break;
        case STM_PIN_ALTERNATE:
            ll_mode = LL_GPIO_MODE_ALTERNATE;
            // In case of ALT function, also set he afnum
            stm_pin_SetAFPin(gpio, pin, afnum);
            break;
        case STM_PIN_ANALOG:
            ll_mode = LL_GPIO_MODE_ANALOG;
            break;
        default:
            MBED_ASSERT(0);
            break;
    }

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_GPIO_SEMID)) {
    }
#endif /* DUAL_CORE */

    LL_GPIO_SetPinMode(gpio, ll_pin, ll_mode);

#if defined(GPIO_ASCR_ASC0)
    /*  For families where Analog Control ASC0 register is present */
    if (STM_PIN_ANALOG_CONTROL(data)) {
        LL_GPIO_EnablePinAnalogControl(gpio, ll_pin);
    } else {
        LL_GPIO_DisablePinAnalogControl(gpio, ll_pin);
    }
#endif

    /*  For now by default use Speed HIGH for output or alt modes */
    if ((mode == STM_PIN_OUTPUT) || (mode == STM_PIN_ALTERNATE)) {
        if (STM_PIN_OD(data)) {
            LL_GPIO_SetPinOutputType(gpio, ll_pin, LL_GPIO_OUTPUT_OPENDRAIN);
        } else {
            LL_GPIO_SetPinOutputType(gpio, ll_pin, LL_GPIO_OUTPUT_PUSHPULL);
        }
    }

    stm_pin_PullConfig(gpio, ll_pin, STM_PIN_PUPD(data));

#if defined(DUAL_PAD)
    configure_dualpad_switch(pin, data, LL_AnalogSwitch);
#endif /* DUAL_PAD */

    stm_pin_DisconnectDebug(pin);

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_GPIO_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}

/**
 * Configure pin pull-up/pull-down
 */
void pin_mode(PinName pin, PinMode mode)
{
    if (pin == NC) {
        return;
    }

    uint32_t port_index = STM_PORT(pin);
    uint32_t ll_pin  = ll_pin_defines[STM_PIN(pin)];
    // Enable GPIO clock
    GPIO_TypeDef *gpio = Set_GPIO_Clock(port_index);

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_GPIO_SEMID)) {
    }
#endif /* DUAL_CORE */

    uint32_t function = LL_GPIO_GetPinMode(gpio, ll_pin);

    if ((function == LL_GPIO_MODE_OUTPUT) || (function == LL_GPIO_MODE_ALTERNATE)) {
        if ((mode == OpenDrainNoPull) || (mode == OpenDrainPullUp) || (mode == OpenDrainPullDown)) {
            LL_GPIO_SetPinOutputType(gpio, ll_pin, LL_GPIO_OUTPUT_OPENDRAIN);
        } else {
            LL_GPIO_SetPinOutputType(gpio, ll_pin, LL_GPIO_OUTPUT_PUSHPULL);
        }
    }

    if ((mode == OpenDrainPullUp) || (mode == PullUp)) {
        stm_pin_PullConfig(gpio, ll_pin, GPIO_PULLUP);
    } else if ((mode == OpenDrainPullDown) || (mode == PullDown)) {
        stm_pin_PullConfig(gpio, ll_pin, GPIO_PULLDOWN);
    } else {
        stm_pin_PullConfig(gpio, ll_pin, GPIO_NOPULL);
    }

#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_GPIO_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}
