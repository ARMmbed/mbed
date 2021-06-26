/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2015-2021, STMicroelectronics
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
#include "i2c_api.h"
#include "platform/mbed_wait_api.h"

#if DEVICE_I2C

#include <string.h>
#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "i2c_device.h" // family specific defines
#include "mbed_error.h"
#include "platform/mbed_power_mgmt.h"

/** @defgroup I2C_DEVICE_Private_Constants I2C_DEVICE Private Constants
 * @{
 */
#ifndef I2C_VALID_TIMING_NBR
#define I2C_VALID_TIMING_NBR                 128U
#endif
#define I2C_SPEED_FREQ_STANDARD                0U    /* 100 kHz */
#define I2C_SPEED_FREQ_FAST                    1U    /* 400 kHz */
#define I2C_SPEED_FREQ_FAST_PLUS               2U    /* 1 MHz */
#define I2C_ANALOG_FILTER_DELAY_MIN            50U   /* ns */
#define I2C_ANALOG_FILTER_DELAY_MAX            260U  /* ns */
#define I2C_USE_ANALOG_FILTER                  1U
#define I2C_DIGITAL_FILTER_COEF                0U
#define I2C_PRESC_MAX                          16U
#define I2C_SCLDEL_MAX                         16U
#define I2C_SDADEL_MAX                         16U
#define I2C_SCLH_MAX                           256U
#define I2C_SCLL_MAX                           256U
#define SEC2NSEC                               1000000000UL
/**
  * @}
  */


/** @defgroup I2C_DEVICE_Private_Types I2C_DEVICE Private Types
  * @{
  */
typedef struct {
    uint32_t freq;       /* Frequency in Hz */
    uint32_t freq_min;   /* Minimum frequency in Hz */
    uint32_t freq_max;   /* Maximum frequency in Hz */
    uint32_t hddat_min;  /* Minimum data hold time in ns */
    uint32_t vddat_max;  /* Maximum data valid time in ns */
    uint32_t sudat_min;  /* Minimum data setup time in ns */
    uint32_t lscl_min;   /* Minimum low period of the SCL clock in ns */
    uint32_t hscl_min;   /* Minimum high period of SCL clock in ns */
    uint32_t trise;      /* Rise time in ns */
    uint32_t tfall;      /* Fall time in ns */
    uint32_t dnf;        /* Digital noise filter coefficient */
} I2C_Charac_t;

typedef struct {
    uint32_t presc;      /* Timing prescaler */
    uint32_t tscldel;    /* SCL delay */
    uint32_t tsdadel;    /* SDA delay */
    uint32_t sclh;       /* SCL high period */
    uint32_t scll;       /* SCL low period */
} I2C_Timings_t;
/**
  * @}
  */

/** @defgroup I2C_DEVICE_Private_Constants I2C_DEVICE Private Constants
  * @{
  */
static const I2C_Charac_t I2C_Charac[] = {
    [I2C_SPEED_FREQ_STANDARD] =
    {
        .freq = 100000,
        .freq_min = 80000,
        .freq_max = 120000,
        .hddat_min = 0,
        .vddat_max = 3450,
        .sudat_min = 250,
        .lscl_min = 4700,
        .hscl_min = 4000,
        .trise = 640,
        .tfall = 20,
        .dnf = I2C_DIGITAL_FILTER_COEF,
    },
    [I2C_SPEED_FREQ_FAST] =
    {
        .freq = 400000,
        .freq_min = 320000,
        .freq_max = 480000,
        .hddat_min = 0,
        .vddat_max = 900,
        .sudat_min = 100,
        .lscl_min = 1300,
        .hscl_min = 600,
        .trise = 250,
        .tfall = 100,
        .dnf = I2C_DIGITAL_FILTER_COEF,
    },
    [I2C_SPEED_FREQ_FAST_PLUS] =
    {
        .freq = 1000000,
        .freq_min = 800000,
        .freq_max = 1200000,
        .hddat_min = 0,
        .vddat_max = 450,
        .sudat_min = 50,
        .lscl_min = 500,
        .hscl_min = 260,
        .trise = 60,
        .tfall = 100,
        .dnf = I2C_DIGITAL_FILTER_COEF,
    },
};
/**
  * @}
  */

/** @defgroup I2C_DEVICE_Private_Variables I2C_DEVICE Private Variables
* @{
*/
static I2C_Timings_t I2c_valid_timing[I2C_VALID_TIMING_NBR];
static uint32_t      I2c_valid_timing_nbr = 0;

#ifndef DEBUG_STDIO
#   define DEBUG_STDIO 0
#endif

#if DEBUG_STDIO
#   include <stdio.h>
#   define DEBUG_PRINTF(...) do { printf(__VA_ARGS__); } while(0)
#else
#   define DEBUG_PRINTF(...) {}
#endif

#if DEVICE_I2C_ASYNCH
#define I2C_S(obj) (struct i2c_s *) (&((obj)->i2c))
#else
#define I2C_S(obj) (struct i2c_s *) (obj)
#endif

/*  Family specific description for I2C */
#define I2C_NUM (5)
static I2C_HandleTypeDef *i2c_handles[I2C_NUM];

/* Timeout values are based on core clock and I2C clock.
   The BYTE_TIMEOUT is computed as twice the number of cycles it would
   take to send 10 bits over I2C. Most Flags should take less than that.
   This is for immediate FLAG or ACK check.
*/
#define BYTE_TIMEOUT ((SystemCoreClock / obj_s->hz) * 2 * 10)
/* Timeout values based on I2C clock.
   The BYTE_TIMEOUT_US is computed as 3x the time in us it would
   take to send 10 bits over I2C. Most Flags should take less than that.
   This is for complete transfers check.
*/
#define BYTE_TIMEOUT_US   ((SystemCoreClock / obj_s->hz) * 3 * 10)
/* Timeout values for flags and events waiting loops. These timeouts are
   not based on accurate values, they just guarantee that the application will
   not remain stuck if the I2C communication is corrupted.
*/
#ifdef TARGET_STM32H7
#define FLAG_TIMEOUT ((int)0x1100)
#else
#define FLAG_TIMEOUT ((int)0x1000)
#endif

#ifdef I2C_IP_VERSION_V1
#define I2C_STATE_NONE            ((uint32_t)(HAL_I2C_MODE_NONE))
#endif

#define SLAVE_MODE_RECEIVE 1
#define SLAVE_MODE_LISTEN  2
#define DEFAULT_SLAVE_MODE SLAVE_MODE_LISTEN

/* Declare i2c_init_internal to be used in this file */
void i2c_init_internal(i2c_t *obj, const i2c_pinmap_t *pinmap);

/* GENERIC INIT and HELPERS FUNCTIONS */

#if defined(I2C1_BASE)
static void i2c1_irq(void)
{
    I2C_HandleTypeDef *handle = i2c_handles[0];
    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);
}
#endif
#if defined(I2C2_BASE)
static void i2c2_irq(void)
{
    I2C_HandleTypeDef *handle = i2c_handles[1];
    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);
}
#endif
#if defined(I2C3_BASE)
static void i2c3_irq(void)
{
    I2C_HandleTypeDef *handle = i2c_handles[2];
    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);
}
#endif
#if defined(I2C4_BASE)
static void i2c4_irq(void)
{
    I2C_HandleTypeDef *handle = i2c_handles[3];
    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);
}
#endif
#if defined(FMPI2C1_BASE)
static void i2c5_irq(void)
{
    I2C_HandleTypeDef *handle = i2c_handles[4];
    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);
}
#endif

void i2c_ev_err_enable(i2c_t *obj, uint32_t handler)
{
    struct i2c_s *obj_s = I2C_S(obj);
    IRQn_Type irq_event_n = obj_s->event_i2cIRQ;
    IRQn_Type irq_error_n = obj_s->error_i2cIRQ;
    /*  default prio in master case is set to 2 */
    uint32_t prio = 2;

    /* Set up ITs using IRQ and handler tables */
    NVIC_SetVector(irq_event_n, handler);
    NVIC_SetVector(irq_error_n, handler);

#if DEVICE_I2CSLAVE
    /*  Set higher priority to slave device than master.
     *  In case a device makes use of both master and slave, the
     *  slave needs higher responsiveness.
     */
    if (obj_s->slave) {
        prio = 1;
    }
#endif

    NVIC_SetPriority(irq_event_n, prio);
    NVIC_SetPriority(irq_error_n, prio);
    NVIC_EnableIRQ(irq_event_n);
    NVIC_EnableIRQ(irq_error_n);
}

void i2c_ev_err_disable(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    IRQn_Type irq_event_n = obj_s->event_i2cIRQ;
    IRQn_Type irq_error_n = obj_s->error_i2cIRQ;

    HAL_NVIC_DisableIRQ(irq_event_n);
    HAL_NVIC_DisableIRQ(irq_error_n);
}

uint32_t i2c_get_irq_handler(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    uint32_t handler = 0;

    switch (obj_s->index) {
#if defined(I2C1_BASE)
        case 0:
            handler = (uint32_t)&i2c1_irq;
            break;
#endif
#if defined(I2C2_BASE)
        case 1:
            handler = (uint32_t)&i2c2_irq;
            break;
#endif
#if defined(I2C3_BASE)
        case 2:
            handler = (uint32_t)&i2c3_irq;
            break;
#endif
#if defined(I2C4_BASE)
        case 3:
            handler = (uint32_t)&i2c4_irq;
            break;
#endif
#if defined(FMPI2C1_BASE)
        case 4:
            handler = (uint32_t)&i2c5_irq;
            break;
#endif
    }

    i2c_handles[obj_s->index] = handle;
    return handler;
}

void i2c_hw_reset(i2c_t *obj)
{
    int timeout;
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    handle->Instance = (I2C_TypeDef *)(obj_s->i2c);

    // wait before reset
    timeout = BYTE_TIMEOUT;
    while ((__HAL_I2C_GET_FLAG(handle, I2C_FLAG_BUSY)) && (--timeout != 0));
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
#if defined I2C1_BASE
    if (obj_s->i2c == I2C_1) {
        __HAL_RCC_I2C1_FORCE_RESET();
        __HAL_RCC_I2C1_RELEASE_RESET();
    }
#endif
#if defined I2C2_BASE
    if (obj_s->i2c == I2C_2) {
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();
    }
#endif
#if defined I2C3_BASE
    if (obj_s->i2c == I2C_3) {
        __HAL_RCC_I2C3_FORCE_RESET();
        __HAL_RCC_I2C3_RELEASE_RESET();
    }
#endif
#if defined I2C4_BASE
    if (obj_s->i2c == I2C_4) {
        __HAL_RCC_I2C4_FORCE_RESET();
        __HAL_RCC_I2C4_RELEASE_RESET();
    }
#endif
#if defined FMPI2C1_BASE
    if (obj_s->i2c == FMPI2C_1) {
        __HAL_RCC_FMPI2C1_FORCE_RESET();
        __HAL_RCC_FMPI2C1_RELEASE_RESET();
    }
#endif
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}

void i2c_sw_reset(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    /*  SW reset procedure:
     *  PE must be kept low during at least 3 APB clock cycles
     *  in order to perform the software reset.
     *  This is ensured by writing the following software sequence:
     *  - Write PE=0
     *  - Check PE=0
     *  - Write PE=1.
     */
    handle->Instance->CR1 &=  ~I2C_CR1_PE;
    while (handle->Instance->CR1 & I2C_CR1_PE);
    handle->Instance->CR1 |=  I2C_CR1_PE;
}

void i2c_init_internal(i2c_t *obj, const i2c_pinmap_t *pinmap)
{
    struct i2c_s *obj_s = I2C_S(obj);

    // Determine the I2C to use
    if (pinmap != NULL) {
        obj_s->sda = pinmap->sda_pin;
        obj_s->scl = pinmap->scl_pin;
#if STATIC_PINMAP_READY
        obj_s->sda_func = pinmap->sda_function;
        obj_s->scl_func = pinmap->scl_function;
#endif
        obj_s->i2c = (I2CName)pinmap->peripheral;
        MBED_ASSERT(obj_s->i2c != (I2CName)NC);
    }

#if defined I2C1_BASE
    // Enable I2C1 clock and pinout if not done
    if (obj_s->i2c == I2C_1) {
        obj_s->index = 0;
        __HAL_RCC_I2C1_CLK_ENABLE();
        // Configure I2C pins
        obj_s->event_i2cIRQ = I2C1_EV_IRQn;
        obj_s->error_i2cIRQ = I2C1_ER_IRQn;

#if defined(TARGET_STM32WL) || defined(TARGET_STM32WB)
        /* In Stop2 mode, I2C1 and I2C2 instances are powered down (only I2C3 register content is kept) */
        sleep_manager_lock_deep_sleep();
#endif
    }
#endif
#if defined I2C2_BASE
    // Enable I2C2 clock and pinout if not done
    if (obj_s->i2c == I2C_2) {
        obj_s->index = 1;
        __HAL_RCC_I2C2_CLK_ENABLE();
        obj_s->event_i2cIRQ = I2C2_EV_IRQn;
        obj_s->error_i2cIRQ = I2C2_ER_IRQn;

#if defined(TARGET_STM32WL)
        /* In Stop2 mode, I2C1 and I2C2 instances are powered down (only I2C3 register content is kept) */
        sleep_manager_lock_deep_sleep();
#endif
    }
#endif
#if defined I2C3_BASE
    // Enable I2C3 clock and pinout if not done
    if (obj_s->i2c == I2C_3) {
        obj_s->index = 2;
        __HAL_RCC_I2C3_CLK_ENABLE();
        obj_s->event_i2cIRQ = I2C3_EV_IRQn;
        obj_s->error_i2cIRQ = I2C3_ER_IRQn;
    }
#endif
#if defined I2C4_BASE
    // Enable I2C3 clock and pinout if not done
    if (obj_s->i2c == I2C_4) {
        obj_s->index = 3;
        __HAL_RCC_I2C4_CLK_ENABLE();
        obj_s->event_i2cIRQ = I2C4_EV_IRQn;
        obj_s->error_i2cIRQ = I2C4_ER_IRQn;
    }
#endif
#if defined FMPI2C1_BASE
    // Enable I2C3 clock and pinout if not done
    if (obj_s->i2c == FMPI2C_1) {
        obj_s->index = 4;
        __HAL_RCC_FMPI2C1_CLK_ENABLE();
        obj_s->event_i2cIRQ = FMPI2C1_EV_IRQn;
        obj_s->error_i2cIRQ = FMPI2C1_ER_IRQn;
    }
#endif

    // Configure I2C pins
#if STATIC_PINMAP_READY
    pin_function(obj_s->sda, obj_s->sda_func);
    pin_function(obj_s->scl, obj_s->scl_func);
#else
    pinmap_pinout(obj_s->sda, PinMap_I2C_SDA);
    pinmap_pinout(obj_s->scl, PinMap_I2C_SCL);
#endif
    pin_mode(obj_s->sda, OpenDrainNoPull);
    pin_mode(obj_s->scl, OpenDrainNoPull);

    // I2C configuration
    // Default hz value used for timeout computation
    if (!obj_s->hz) {
        obj_s->hz = 100000;    // 100 kHz per default
    }

    // Reset to clear pending flags if any
    i2c_hw_reset(obj);
    i2c_frequency(obj, obj_s->hz);

#if DEVICE_I2CSLAVE
    // I2C master by default
    obj_s->slave = 0;
    obj_s->pending_slave_tx_master_rx = 0;
    obj_s->pending_slave_rx_maxter_tx = 0;
#endif

    // I2C Xfer operation init
    obj_s->event = 0;
    obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
#ifdef I2C_IP_VERSION_V2
    obj_s->pending_start = 0;
#endif
}

void i2c_deinit_internal(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);

    i2c_hw_reset(obj);

    HAL_I2C_DeInit(&(obj_s->handle));

#if defined I2C1_BASE
    if (obj_s->i2c == I2C_1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
#if defined(TARGET_STM32WL) || defined(TARGET_STM32WB)
        sleep_manager_unlock_deep_sleep();
#endif
    }
#endif
#if defined I2C2_BASE
    if (obj_s->i2c == I2C_2) {
        __HAL_RCC_I2C2_CLK_DISABLE();
#if defined(TARGET_STM32WL)
        sleep_manager_unlock_deep_sleep();
#endif
    }
#endif
#if defined I2C3_BASE
    if (obj_s->i2c == I2C_3) {
        __HAL_RCC_I2C3_CLK_DISABLE();
    }
#endif
#if defined I2C4_BASE
    if (obj_s->i2c == I2C_4) {
        __HAL_RCC_I2C4_CLK_DISABLE();
    }
#endif
#if defined FMPI2C1_BASE
    if (obj_s->i2c == FMPI2C_1) {
        __HAL_RCC_FMPI2C1_CLK_DISABLE();
    }
#endif

    pin_function(obj_s->sda, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
    pin_function(obj_s->scl, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));

    obj_s->sda = NC;
    obj_s->scl = NC;
    obj_s->i2c = (I2CName)NC;
}

#if STATIC_PINMAP_READY
#define I2C_INIT_DIRECT i2c_init_direct
void i2c_init_direct(i2c_t *obj, const i2c_pinmap_t *pinmap)
#else
#define I2C_INIT_DIRECT _i2c_init_direct
static void _i2c_init_direct(i2c_t *obj, const i2c_pinmap_t *pinmap)
#endif
{
    memset(obj, 0, sizeof(*obj));
    i2c_init_internal(obj, pinmap);
}

void i2c_init(i2c_t *obj, PinName sda, PinName scl)
{
    uint32_t i2c_sda = pinmap_peripheral(sda, PinMap_I2C_SDA);
    uint32_t i2c_scl = pinmap_peripheral(scl, PinMap_I2C_SCL);

    int peripheral = (int)pinmap_merge(i2c_sda, i2c_scl);

    int sda_function = (int)pinmap_find_function(sda, PinMap_I2C_SDA);
    int scl_function = (int)pinmap_find_function(scl, PinMap_I2C_SCL);

    const i2c_pinmap_t explicit_i2c_pinmap = {peripheral, sda, sda_function, scl, scl_function};

    I2C_INIT_DIRECT(obj, &explicit_i2c_pinmap);
}

void i2c_free(i2c_t *obj)
{
    i2c_deinit_internal(obj);
}


void i2c_frequency(i2c_t *obj, int hz)
{
    int timeout;
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    // wait before init
    timeout = BYTE_TIMEOUT;
    while ((__HAL_I2C_GET_FLAG(handle, I2C_FLAG_BUSY)) && (--timeout != 0));

#ifdef I2C_IP_VERSION_V1
    handle->Init.ClockSpeed      = hz;
    handle->Init.DutyCycle       = I2C_DUTYCYCLE_2;
#endif
#ifdef I2C_IP_VERSION_V2
    /*  Only predefined timing for below frequencies are supported */
    MBED_ASSERT((hz == 100000) || (hz == 400000) || (hz == 1000000));
    /* Calculates I2C timing value with respect to I2C input clock and I2C bus frequency */
    handle->Init.Timing = i2c_get_timing(obj_s->i2c, hz);
    // Enable the Fast Mode Plus capability
    if (hz == 1000000) {
#if defined(I2C1_BASE) && defined(I2C_FASTMODEPLUS_I2C1)  // sometimes I2C_FASTMODEPLUS_I2Cx is define even if not supported by the chip
#if defined(SYSCFG_CFGR1_I2C_FMP_I2C1) || defined(SYSCFG_CFGR1_I2C1_FMP) || defined(SYSCFG_PMC_I2C1_FMP) || defined(SYSCFG_PMCR_I2C1_FMP) || defined(SYSCFG_CFGR2_I2C1_FMP)
        if (obj_s->i2c == I2C_1) {
            HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C1);
        }
#endif
#endif
#if defined(I2C2_BASE) && defined(I2C_FASTMODEPLUS_I2C2)  // sometimes I2C_FASTMODEPLUS_I2Cx is define even if not supported by the chip
#if defined(SYSCFG_CFGR1_I2C_FMP_I2C2) || defined(SYSCFG_CFGR1_I2C2_FMP) || defined(SYSCFG_PMC_I2C2_FMP) || defined(SYSCFG_PMCR_I2C2_FMP) || defined(SYSCFG_CFGR2_I2C2_FMP)
        if (obj_s->i2c == I2C_2) {
            HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C2);
        }
#endif
#endif
#if defined(I2C3_BASE) && defined (I2C_FASTMODEPLUS_I2C3)  // sometimes I2C_FASTMODEPLUS_I2Cx is define even if not supported by the chip
#if defined(SYSCFG_CFGR1_I2C_FMP_I2C3) || defined(SYSCFG_CFGR1_I2C3_FMP) || defined(SYSCFG_PMC_I2C3_FMP) || defined(SYSCFG_PMCR_I2C3_FMP) || defined(SYSCFG_CFGR2_I2C3_FMP)
        if (obj_s->i2c == I2C_3) {
            HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C3);
        }
#endif
#endif
#if defined(I2C4_BASE) && defined (I2C_FASTMODEPLUS_I2C4)  // sometimes I2C_FASTMODEPLUS_I2Cx is define even if not supported by the chip
#if defined(SYSCFG_CFGR1_I2C_FMP_I2C4) || defined(SYSCFG_CFGR1_I2C4_FMP) || defined(SYSCFG_PMC_I2C4_FMP) || defined(SYSCFG_PMCR_I2C4_FMP) || defined(SYSCFG_CFGR2_I2C4_FMP)
        if (obj_s->i2c == I2C_4) {
            HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C4);
        }
#endif
#endif
    }
#endif //I2C_IP_VERSION_V2

    /*##-1- Configure the I2C clock source. The clock is derived from the SYSCLK #*/
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
#if defined(I2C1_BASE) && defined (__HAL_RCC_I2C1_CONFIG)
    if (obj_s->i2c == I2C_1) {
        __HAL_RCC_I2C1_CONFIG(I2CAPI_I2C1_CLKSRC);
    }
#endif
#if defined(I2C2_BASE) && defined(__HAL_RCC_I2C2_CONFIG)
    if (obj_s->i2c == I2C_2) {
        __HAL_RCC_I2C2_CONFIG(I2CAPI_I2C2_CLKSRC);
    }
#endif
#if defined(I2C3_BASE) && defined(__HAL_RCC_I2C3_CONFIG)
    if (obj_s->i2c == I2C_3) {
        __HAL_RCC_I2C3_CONFIG(I2CAPI_I2C3_CLKSRC);
    }
#endif
#if defined(I2C4_BASE) && defined(__HAL_RCC_I2C4_CONFIG)
    if (obj_s->i2c == I2C_4) {
        __HAL_RCC_I2C4_CONFIG(I2CAPI_I2C4_CLKSRC);
    }
#endif
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */

#ifdef I2C_ANALOGFILTER_ENABLE
    /* Enable the Analog I2C Filter */
    HAL_I2CEx_ConfigAnalogFilter(handle, I2C_ANALOGFILTER_ENABLE);
#endif

    // I2C configuration
    handle->Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    handle->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    handle->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle->Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    handle->Init.OwnAddress1     = 0;
    handle->Init.OwnAddress2     = 0;
#ifdef I2C_IP_VERSION_V2
    handle->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
#endif
    HAL_I2C_Init(handle);

    /*  store frequency for timeout computation */
    obj_s->hz = hz;
}

i2c_t *get_i2c_obj(I2C_HandleTypeDef *hi2c)
{
    /* Aim of the function is to get i2c_s pointer using hi2c pointer */
    /* Highly inspired from magical linux kernel's "container_of" */
    /* (which was not directly used since not compatible with IAR toolchain) */
    struct i2c_s *obj_s;
    i2c_t *obj;

    obj_s = (struct i2c_s *)((char *)hi2c - offsetof(struct i2c_s, handle));
    obj = (i2c_t *)((char *)obj_s - offsetof(i2c_t, i2c));

    return (obj);
}

void i2c_reset(i2c_t *obj)
{
    /*  As recommended in i2c_api.h, mainly send stop */
    i2c_stop(obj);
    /* then re-init */
    i2c_init_internal(obj, NULL);
}

/*
 *  UNITARY APIS.
 *  For very basic operations, direct registers access is needed
 *  There are 2 different IPs version that need to be supported
 */
#ifdef I2C_IP_VERSION_V1
int i2c_start(i2c_t *obj)
{

    int timeout;
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    // Clear Acknowledge failure flag
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_AF);

    // Wait the STOP condition has been previously correctly sent
    // This timeout can be avoid in some specific cases by simply clearing the STOP bit
    timeout = FLAG_TIMEOUT;
    while ((handle->Instance->CR1 & I2C_CR1_STOP) == I2C_CR1_STOP) {
        if ((timeout--) == 0) {
            return 1;
        }
    }

    // Generate the START condition
    handle->Instance->CR1 |= I2C_CR1_START;

    // Wait the START condition has been correctly sent
    timeout = FLAG_TIMEOUT;
    while (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_SB) == RESET) {
        if ((timeout--) == 0) {
            return 1;
        }
    }

    return 0;
}

int i2c_stop(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_TypeDef *i2c = (I2C_TypeDef *)obj_s->i2c;

    // Generate the STOP condition
    i2c->CR1 |= I2C_CR1_STOP;

    /*  In case of mixed usage of the APIs (unitary + SYNC)
     *  re-init HAL state
     */
    if (obj_s->XferOperation != I2C_FIRST_AND_LAST_FRAME) {
        i2c_init_internal(obj, NULL);
    }

    return 0;
}

int i2c_byte_read(i2c_t *obj, int last)
{

    int timeout;
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    if (last) {
        // Don't acknowledge the last byte
        handle->Instance->CR1 &= ~I2C_CR1_ACK;
    } else {
        // Acknowledge the byte
        handle->Instance->CR1 |= I2C_CR1_ACK;
    }

    // Wait until the byte is received
    timeout = FLAG_TIMEOUT;
    while (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_RXNE) == RESET) {
        if ((timeout--) == 0) {
            return -1;
        }
    }

    return (int)handle->Instance->DR;
}

int i2c_byte_write(i2c_t *obj, int data)
{

    int timeout;
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    handle->Instance->DR = (uint8_t)data;

    // Wait until the byte (might be the address) is transmitted
    timeout = FLAG_TIMEOUT;
    while ((__HAL_I2C_GET_FLAG(handle, I2C_FLAG_TXE) == RESET) &&
            (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_BTF) == RESET) &&
            (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_ADDR) == RESET)) {
        if ((timeout--) == 0) {
            return 2;
        }
    }

    if (__HAL_I2C_GET_FLAG(handle, I2C_FLAG_ADDR) != RESET) {
        __HAL_I2C_CLEAR_ADDRFLAG(handle);
    }

    return 1;
}
#endif //I2C_IP_VERSION_V1
#ifdef I2C_IP_VERSION_V2

int i2c_start(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    /*  This I2C IP doesn't  */
    obj_s->pending_start = 1;
    return 0;
}

int i2c_stop(i2c_t *obj)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int timeout = FLAG_TIMEOUT;
#if DEVICE_I2CSLAVE
    if (obj_s->slave) {
        /*  re-init slave when stop is requested */
        i2c_init_internal(obj, NULL);
        return 0;
    }
#endif

    // Ensure the transmission is started before sending a stop
    if ((handle->Instance->CR2 & (uint32_t)I2C_CR2_RD_WRN) == 0) {
        timeout = FLAG_TIMEOUT;
        while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_TXIS)) {
            if ((timeout--) == 0) {
                return I2C_ERROR_BUS_BUSY;
            }
        }
    }

    // Generate the STOP condition
    handle->Instance->CR2 = I2C_CR2_STOP;

    timeout = FLAG_TIMEOUT;
    while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_STOPF)) {
        if ((timeout--) == 0) {
            return I2C_ERROR_BUS_BUSY;
        }
    }

    /* Clear STOP Flag */
    __HAL_I2C_CLEAR_FLAG(handle, I2C_FLAG_STOPF);

    /* Erase slave address, this wiil be used as a marker
     * to know when we need to prepare next start */
    handle->Instance->CR2 &=  ~I2C_CR2_SADD;

    /*
     * V2 IP is meant for automatic STOP, not user STOP
     * SW reset the IP state machine before next transaction
     */
    i2c_sw_reset(obj);

    /*  In case of mixed usage of the APIs (unitary + SYNC)
     *  re-init HAL state */
    if (obj_s->XferOperation != I2C_FIRST_AND_LAST_FRAME) {
        i2c_init_internal(obj, NULL);
    }

    return 0;
}

int i2c_byte_read(i2c_t *obj, int last)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int timeout = FLAG_TIMEOUT;
    uint32_t tmpreg = handle->Instance->CR2;
    char data;
#if DEVICE_I2CSLAVE
    if (obj_s->slave) {
        return i2c_slave_read(obj, &data, 1);
    }
#endif
    /* Then send data when there's room in the TX fifo */
    if ((tmpreg & I2C_CR2_RELOAD) != 0) {
        while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_TCR)) {
            if ((timeout--) == 0) {
                DEBUG_PRINTF("timeout in i2c_byte_read\r\n");
                return -1;
            }
        }
    }

    if (last) {
        /* Disable Address Acknowledge */
        tmpreg = tmpreg & (~I2C_CR2_RELOAD);
        tmpreg |= I2C_CR2_NACK | (I2C_CR2_NBYTES & (1 << 16));
    } else {
        /* Enable reload mode as we don't know how many bytes will be sent */
        /* and set transfer size to 1 */
        tmpreg |= I2C_CR2_RELOAD | (I2C_CR2_NBYTES & (1 << 16));
    }

    /* Set the prepared configuration */
    handle->Instance->CR2 = tmpreg;

    timeout = FLAG_TIMEOUT;
    while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_RXNE)) {
        if ((timeout--) == 0) {
            return -1;
        }
    }

    /* Then Get Byte */
    data = handle->Instance->RXDR;

    return data;
}

int i2c_byte_write(i2c_t *obj, int data)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int timeout = FLAG_TIMEOUT;
    uint32_t tmpreg = handle->Instance->CR2;
#if DEVICE_I2CSLAVE
    if (obj_s->slave) {
        return i2c_slave_write(obj, (char *) &data, 1);
    }
#endif
    if (obj_s->pending_start) {
        obj_s->pending_start = 0;
        //*  First byte after the start is the address */
        tmpreg |= (uint32_t)((uint32_t)data & I2C_CR2_SADD);
        if (data & 0x01) {
            tmpreg |= I2C_CR2_START | I2C_CR2_RD_WRN;
        } else {
            tmpreg |= I2C_CR2_START;
            tmpreg &= ~I2C_CR2_RD_WRN;
        }
        /*  Disable reload first to use it later */
        tmpreg &= ~I2C_CR2_RELOAD;
        /*  Disable Autoend */
        tmpreg &= ~I2C_CR2_AUTOEND;
        /* Do not set any transfer size for now */
        tmpreg |= (I2C_CR2_NBYTES & (1 << 16));
        /* Set the prepared configuration */
        handle->Instance->CR2 = tmpreg;
    } else {
        /* Set the prepared configuration */
        tmpreg = handle->Instance->CR2;

        /* Then send data when there's room in the TX fifo */
        if ((tmpreg & I2C_CR2_RELOAD) != 0) {
            while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_TCR)) {
                if ((timeout--) == 0) {
                    DEBUG_PRINTF("timeout in i2c_byte_write\r\n");
                    return 2;
                }
            }
        }
        /*  Enable reload mode as we don't know how many bytes will eb sent */
        tmpreg |= I2C_CR2_RELOAD;
        /*  Set transfer size to 1 */
        tmpreg |= (I2C_CR2_NBYTES & (1 << 16));
        /* Set the prepared configuration */
        handle->Instance->CR2 = tmpreg;
        /*  Prepare next write */
        timeout = FLAG_TIMEOUT;
        while (!__HAL_I2C_GET_FLAG(handle, I2C_FLAG_TXE)) {
            if ((timeout--) == 0) {
                return 2;
            }
        }
        /*  Write byte */
        handle->Instance->TXDR = data;
    }

    return 1;
}
#endif //I2C_IP_VERSION_V2

/*
 *  SYNC APIS
 */
int i2c_read(i2c_t *obj, int address, char *data, int length, int stop)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int count = I2C_ERROR_BUS_BUSY, ret = 0;
    uint32_t timeout = 0;
#if defined(I2C_IP_VERSION_V1)
    // Trick to remove compiler warning "left and right operands are identical" in some cases
    uint32_t op1 = I2C_FIRST_AND_LAST_FRAME;
    uint32_t op2 = I2C_LAST_FRAME;
    if ((obj_s->XferOperation == op1) || (obj_s->XferOperation == op2)) {
        if (stop) {
            obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_FIRST_FRAME;
        }
    } else if ((obj_s->XferOperation == I2C_FIRST_FRAME) ||
               (obj_s->XferOperation == I2C_NEXT_FRAME)) {
        if (stop) {
            obj_s->XferOperation = I2C_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_NEXT_FRAME;
        }
    }
#elif defined(I2C_IP_VERSION_V2)
    if ((obj_s->XferOperation == I2C_FIRST_FRAME) || (obj_s->XferOperation == I2C_FIRST_AND_LAST_FRAME) || (obj_s->XferOperation == I2C_LAST_FRAME)) {
        if (stop) {
            obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_FIRST_FRAME;
        }
    } else {
        // should not happend
        error("I2C: abnormal case should not happend");
    }
#endif

    obj_s->event = 0;

    /* Activate default IRQ handlers for sync mode
    * which would be overwritten in async mode
    */
    i2c_ev_err_enable(obj, i2c_get_irq_handler(obj));

    ret = HAL_I2C_Master_Seq_Receive_IT(handle, address, (uint8_t *) data, length, obj_s->XferOperation);

    if (ret == HAL_OK) {
        timeout = BYTE_TIMEOUT_US * (length + 1);
        /*  transfer started : wait completion or timeout */
        while (!(obj_s->event & I2C_EVENT_ALL) && (--timeout != 0)) {
            wait_us(1);
        }

        i2c_ev_err_disable(obj);

        if ((timeout == 0) || (obj_s->event != I2C_EVENT_TRANSFER_COMPLETE)) {
            DEBUG_PRINTF("TIMEOUT or error in i2c_read\r\n");
            /* re-init IP to try and get back in a working state */
            i2c_init_internal(obj, NULL);
        } else {
            count = length;
        }
    } else {
        DEBUG_PRINTF("ERROR in i2c_read:%d\r\n", ret);
    }

    return count;
}

int i2c_write(i2c_t *obj, int address, const char *data, int length, int stop)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int count = I2C_ERROR_BUS_BUSY, ret = 0;
    uint32_t timeout = 0;

#if defined(I2C_IP_VERSION_V1)
    // Trick to remove compiler warning "left and right operands are identical" in some cases
    uint32_t op1 = I2C_FIRST_AND_LAST_FRAME;
    uint32_t op2 = I2C_LAST_FRAME;
    if ((obj_s->XferOperation == op1) || (obj_s->XferOperation == op2)) {
        if (stop) {
            obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_FIRST_FRAME;
        }
    } else if ((obj_s->XferOperation == I2C_FIRST_FRAME) ||
               (obj_s->XferOperation == I2C_NEXT_FRAME)) {
        if (stop) {
            obj_s->XferOperation = I2C_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_NEXT_FRAME;
        }
    }
#elif defined(I2C_IP_VERSION_V2)
    if ((obj_s->XferOperation == I2C_FIRST_FRAME) || (obj_s->XferOperation == I2C_FIRST_AND_LAST_FRAME) || (obj_s->XferOperation == I2C_LAST_FRAME)) {
        if (stop) {
            obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_FIRST_FRAME;
        }
    } else {
        // should not happend
        error("I2C: abnormal case should not happend");
    }
#endif

    obj_s->event = 0;

    i2c_ev_err_enable(obj, i2c_get_irq_handler(obj));

    ret = HAL_I2C_Master_Seq_Transmit_IT(handle, address, (uint8_t *) data, length, obj_s->XferOperation);

    if (ret == HAL_OK) {
        timeout = BYTE_TIMEOUT_US * (length + 1);
        /*  transfer started : wait completion or timeout */
        while (!(obj_s->event & I2C_EVENT_ALL) && (--timeout != 0)) {
            wait_us(1);
        }

        i2c_ev_err_disable(obj);

        if ((timeout == 0) || (obj_s->event != I2C_EVENT_TRANSFER_COMPLETE)) {
            DEBUG_PRINTF(" TIMEOUT or error in i2c_write\r\n");
            /* re-init IP to try and get back in a working state */
            i2c_init_internal(obj, NULL);
        } else {
            count = length;
        }
    } else {
        DEBUG_PRINTF("ERROR in i2c_write\r\n");
    }

    return count;
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);

#if DEVICE_I2C_ASYNCH
    /* Handle potential Tx/Rx use case */
    if ((obj->tx_buff.length) && (obj->rx_buff.length)) {
#if defined(I2C_IP_VERSION_V1)
        if (obj_s->stop) {
            obj_s->XferOperation = I2C_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_NEXT_FRAME;
        }
#elif defined(I2C_IP_VERSION_V2)
        if (obj_s->stop) {
            obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
        } else {
            obj_s->XferOperation = I2C_FIRST_FRAME;
        }
#endif

        HAL_I2C_Master_Seq_Receive_IT(hi2c, obj_s->address, (uint8_t *)obj->rx_buff.buffer, obj->rx_buff.length, obj_s->XferOperation);
    } else
#endif
    {
        /* Set event flag */
        obj_s->event = I2C_EVENT_TRANSFER_COMPLETE;
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);
#ifdef I2C_IP_VERSION_V1
    hi2c->PreviousState = I2C_STATE_NONE;
#endif
    /* Set event flag */
    obj_s->event = I2C_EVENT_TRANSFER_COMPLETE;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    uint32_t event_code = 0;

#if DEVICE_I2CSLAVE
    uint32_t address = 0;
    /*  Store address to handle it after reset */
    if (obj_s->slave) {
        address = handle->Init.OwnAddress1;
    }
#endif


    if ((handle->ErrorCode & HAL_I2C_ERROR_AF) == HAL_I2C_ERROR_AF) {
        /* Keep Set event flag */
        event_code = (I2C_EVENT_TRANSFER_EARLY_NACK) | (I2C_EVENT_ERROR_NO_SLAVE);
    }
    DEBUG_PRINTF("HAL_I2C_ErrorCallback:%d, index=%d\r\n", (int) hi2c->ErrorCode, obj_s->index);

    /* re-init IP to try and get back in a working state */
    i2c_init_internal(obj, NULL);

#if DEVICE_I2CSLAVE
    /*  restore slave address */
    if (address != 0) {
        obj_s->slave = DEFAULT_SLAVE_MODE;
        i2c_slave_address(obj, 0, address, 0);
    }
#endif

    /* Keep Set event flag */
    obj_s->event = event_code | I2C_EVENT_ERROR;
}

const PinMap *i2c_master_sda_pinmap()
{
    return PinMap_I2C_SDA;
}

const PinMap *i2c_master_scl_pinmap()
{
    return PinMap_I2C_SCL;
}

const PinMap *i2c_slave_sda_pinmap()
{
    return PinMap_I2C_SDA;
}

const PinMap *i2c_slave_scl_pinmap()
{
    return PinMap_I2C_SCL;
}

#if DEVICE_I2CSLAVE
/* SLAVE API FUNCTIONS */
void i2c_slave_address(i2c_t *obj, int idx, uint32_t address, uint32_t mask)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    // I2C configuration
    handle->Init.OwnAddress1     = address;
    HAL_I2C_Init(handle);

    i2c_ev_err_enable(obj, i2c_get_irq_handler(obj));

    HAL_I2C_EnableListen_IT(handle);
}

void i2c_slave_mode(i2c_t *obj, int enable_slave)
{

    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    if (enable_slave) {
        obj_s->slave = DEFAULT_SLAVE_MODE;
        HAL_I2C_EnableListen_IT(handle);
    } else {
        obj_s->slave = 0;
        HAL_I2C_DisableListen_IT(handle);
    }
}

// See I2CSlave.h
#define NoData         0 // the slave has not been addressed
#define ReadAddressed  1 // the master has requested a read from this slave (slave = transmitter)
#define WriteGeneral   2 // the master is writing to all slave
#define WriteAddressed 3 // the master is writing to this slave (slave = receiver)


void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);

    /*  Transfer direction in HAL is from Master point of view */
    if (TransferDirection == I2C_DIRECTION_RECEIVE) {
        obj_s->pending_slave_tx_master_rx = 1;
    }

    if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
        obj_s->pending_slave_rx_maxter_tx = 1;
    }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(I2cHandle);
    struct i2c_s *obj_s = I2C_S(obj);
    obj_s->pending_slave_tx_master_rx = 0;
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(I2cHandle);
    struct i2c_s *obj_s = I2C_S(obj);

    if (obj_s->slave == SLAVE_MODE_LISTEN) {
        obj_s->slave_rx_count++;
        if (obj_s->slave_rx_count < obj_s->slave_rx_buffer_size){
            HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &(obj_s->slave_rx_buffer[obj_s->slave_rx_count]), 1, I2C_NEXT_FRAME);
        } else {
            obj_s->pending_slave_rx_maxter_tx = 0;
        }
    } else {
        obj_s->pending_slave_rx_maxter_tx = 0;
    }
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);

    if (obj_s->slave == SLAVE_MODE_LISTEN) {
        obj_s->pending_slave_rx_maxter_tx = 0;
    }

    /* restart listening for master requests */
    HAL_I2C_EnableListen_IT(hi2c);
}

int i2c_slave_receive(i2c_t *obj)
{

    struct i2c_s *obj_s = I2C_S(obj);
    int retValue = NoData;

    if (obj_s->pending_slave_rx_maxter_tx) {
        retValue = WriteAddressed;
    }

    if (obj_s->pending_slave_tx_master_rx) {
        retValue = ReadAddressed;
    }

    return (retValue);
}

int i2c_slave_read(i2c_t *obj, char *data, int length)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int count = 0;
    int ret = 0;
    uint32_t timeout = 0;
    int _length = 0;

    if (obj_s->slave == SLAVE_MODE_LISTEN) {
       /*  We don't know in advance how many bytes will be sent by master so
        *  we'll fetch one by one until master ends the sequence */
        _length = 1;
        obj_s->slave_rx_buffer_size = length;
        obj_s->slave_rx_count = 0;
        obj_s->slave_rx_buffer = (uint8_t*)data;
    } else {
        _length = length;
    }

    /*  Always use I2C_NEXT_FRAME as slave will just adapt to master requests */
    ret = HAL_I2C_Slave_Seq_Receive_IT(handle, (uint8_t *) data, _length, I2C_NEXT_FRAME);

    if (ret == HAL_OK) {
        timeout = BYTE_TIMEOUT_US * (_length + 1);
        while (obj_s->pending_slave_rx_maxter_tx && (--timeout != 0)) {
            wait_us(1);
        }

        if (timeout != 0) {
            if (obj_s->slave == SLAVE_MODE_LISTEN) {
                count = obj_s->slave_rx_count;
            } else {
                count = _length;
            }
        } else {
            DEBUG_PRINTF("TIMEOUT or error in i2c_slave_read\r\n");
        }
    }
    return count;
}

int i2c_slave_write(i2c_t *obj, const char *data, int length)
{
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int count = 0;
    int ret = 0;
    uint32_t timeout = 0;

    /*  Always use I2C_NEXT_FRAME as slave will just adapt to master requests */
    ret = HAL_I2C_Slave_Seq_Transmit_IT(handle, (uint8_t *) data, length, I2C_NEXT_FRAME);

    if (ret == HAL_OK) {
        timeout = BYTE_TIMEOUT_US * (length + 1);
        while (obj_s->pending_slave_tx_master_rx && (--timeout != 0)) {
            wait_us(1);
        }

        if (timeout != 0) {
            count = length;
        } else {
            DEBUG_PRINTF("TIMEOUT or error in i2c_slave_write\r\n");
        }
    }

    return count;
}
#endif // DEVICE_I2CSLAVE

#if DEVICE_I2C_ASYNCH
/* ASYNCH MASTER API FUNCTIONS */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
    /* Get object ptr based on handler ptr */
    i2c_t *obj = get_i2c_obj(hi2c);
    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    /* Disable IT. Not always done before calling macro */
    __HAL_I2C_DISABLE_IT(handle, I2C_IT_ALL);
    i2c_ev_err_disable(obj);

    /* Set event flag */
    obj_s->event = I2C_EVENT_ERROR;
}

void i2c_transfer_asynch(i2c_t *obj, const void *tx, size_t tx_length, void *rx, size_t rx_length, uint32_t address, uint32_t stop, uint32_t handler, uint32_t event, DMAUsage hint)
{

    // TODO: DMA usage is currently ignored by this way
    (void) hint;

    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    /* Update object */
    obj->tx_buff.buffer = (void *)tx;
    obj->tx_buff.length = tx_length;
    obj->tx_buff.pos = 0;
    obj->tx_buff.width = 8;

    obj->rx_buff.buffer = (void *)rx;
    obj->rx_buff.length = rx_length;
    obj->rx_buff.pos = SIZE_MAX;
    obj->rx_buff.width = 8;

    obj_s->available_events = event;
    obj_s->event = 0;
    obj_s->address = address;
    obj_s->stop = stop;

    i2c_ev_err_enable(obj, handler);

    /* Set operation step depending if stop sending required or not */
    if ((tx_length && !rx_length) || (!tx_length && rx_length)) {
#if defined(I2C_IP_VERSION_V1)
        // Trick to remove compiler warning "left and right operands are identical" in some cases
        uint32_t op1 = I2C_FIRST_AND_LAST_FRAME;
        uint32_t op2 = I2C_LAST_FRAME;
        if ((obj_s->XferOperation == op1) || (obj_s->XferOperation == op2)) {
            if (stop) {
                obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
            } else {
                obj_s->XferOperation = I2C_FIRST_FRAME;
            }
        } else if ((obj_s->XferOperation == I2C_FIRST_FRAME) ||
                   (obj_s->XferOperation == I2C_NEXT_FRAME)) {
            if (stop) {
                obj_s->XferOperation = I2C_LAST_FRAME;
            } else {
                obj_s->XferOperation = I2C_NEXT_FRAME;
            }
        }
#elif defined(I2C_IP_VERSION_V2)
        if ((obj_s->XferOperation == I2C_FIRST_FRAME) || (obj_s->XferOperation == I2C_FIRST_AND_LAST_FRAME) || (obj_s->XferOperation == I2C_LAST_FRAME)) {
            if (stop) {
                obj_s->XferOperation = I2C_FIRST_AND_LAST_FRAME;
            } else {
                obj_s->XferOperation = I2C_FIRST_FRAME;
            }
        } else {
            // should not happend
            error("I2C: abnormal case should not happend");
        }
#endif
        if (tx_length > 0) {
            HAL_I2C_Master_Seq_Transmit_IT(handle, address, (uint8_t *)tx, tx_length, obj_s->XferOperation);
        }
        if (rx_length > 0) {
            HAL_I2C_Master_Seq_Receive_IT(handle, address, (uint8_t *)rx, rx_length, obj_s->XferOperation);
        }
    } else if (tx_length && rx_length) {
        /* Two steps operation, don't modify XferOperation, keep it for next step */
#if defined(I2C_IP_VERSION_V1)
        // Trick to remove compiler warning "left and right operands are identical" in some cases
        uint32_t op1 = I2C_FIRST_AND_LAST_FRAME;
        uint32_t op2 = I2C_LAST_FRAME;
        if ((obj_s->XferOperation == op1) || (obj_s->XferOperation == op2)) {
            HAL_I2C_Master_Seq_Transmit_IT(handle, address, (uint8_t *)tx, tx_length, I2C_FIRST_FRAME);
        } else if ((obj_s->XferOperation == I2C_FIRST_FRAME) ||
                   (obj_s->XferOperation == I2C_NEXT_FRAME)) {
            HAL_I2C_Master_Seq_Transmit_IT(handle, address, (uint8_t *)tx, tx_length, I2C_NEXT_FRAME);
        }
#elif defined(I2C_IP_VERSION_V2)
        HAL_I2C_Master_Seq_Transmit_IT(handle, address, (uint8_t *)tx, tx_length, I2C_FIRST_FRAME);
#endif
    }
}


uint32_t i2c_irq_handler_asynch(i2c_t *obj)
{

    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    HAL_I2C_EV_IRQHandler(handle);
    HAL_I2C_ER_IRQHandler(handle);

    /*  Return I2C event status */
    return (obj_s->event & obj_s->available_events);
}

uint8_t i2c_active(i2c_t *obj)
{

    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    if (handle->State == HAL_I2C_STATE_READY) {
        return 0;
    } else {
        return 1;
    }
}

void i2c_abort_asynch(i2c_t *obj)
{

    struct i2c_s *obj_s = I2C_S(obj);
    I2C_HandleTypeDef *handle = &(obj_s->handle);

    /* Abort HAL requires DevAddress, but is not used. Use Dummy */
    uint16_t Dummy_DevAddress = 0x00;

    HAL_I2C_Master_Abort_IT(handle, Dummy_DevAddress);
}

/**
  * @brief  Calculate SCLL and SCLH and find best configuration.
  * @param  clock_src_freq I2C source clock in HZ.
  * @param  I2C_speed I2C frequency (index).
  * @retval config index (0 to I2C_VALID_TIMING_NBR], 0xFFFFFFFF for no valid config.
  */
uint32_t i2c_compute_scll_sclh(uint32_t clock_src_freq, uint32_t I2C_speed)
{
    uint32_t ret = 0xFFFFFFFFU;
    uint32_t ti2cclk;
    uint32_t ti2cspeed;
    uint32_t prev_error;
    uint32_t dnf_delay;
    uint32_t clk_min, clk_max;
    uint32_t scll, sclh;
    uint32_t tafdel_min;

    ti2cclk   = (SEC2NSEC + (clock_src_freq / 2U)) / clock_src_freq;
    ti2cspeed   = (SEC2NSEC + (I2C_Charac[I2C_speed].freq / 2U)) / I2C_Charac[I2C_speed].freq;

    tafdel_min = (I2C_USE_ANALOG_FILTER == 1U) ? I2C_ANALOG_FILTER_DELAY_MIN : 0U;

    /* tDNF = DNF x tI2CCLK */
    dnf_delay = I2C_Charac[I2C_speed].dnf * ti2cclk;

    clk_max = SEC2NSEC / I2C_Charac[I2C_speed].freq_min;
    clk_min = SEC2NSEC / I2C_Charac[I2C_speed].freq_max;

    prev_error = ti2cspeed;

    for (uint32_t count = 0; count < I2c_valid_timing_nbr; count++) {
        /* tPRESC = (PRESC+1) x tI2CCLK*/
        uint32_t tpresc = (I2c_valid_timing[count].presc + 1U) * ti2cclk;

        for (scll = 0; scll < I2C_SCLL_MAX; scll++) {
            /* tLOW(min) <= tAF(min) + tDNF + 2 x tI2CCLK + [(SCLL+1) x tPRESC ] */
            uint32_t tscl_l = tafdel_min + dnf_delay + (2U * ti2cclk) + ((scll + 1U) * tpresc);


            /* The I2CCLK period tI2CCLK must respect the following conditions:
            tI2CCLK < (tLOW - tfilters) / 4 and tI2CCLK < tHIGH */
            if ((tscl_l > I2C_Charac[I2C_speed].lscl_min) && (ti2cclk < ((tscl_l - tafdel_min - dnf_delay) / 4U))) {
                for (sclh = 0; sclh < I2C_SCLH_MAX; sclh++) {
                    /* tHIGH(min) <= tAF(min) + tDNF + 2 x tI2CCLK + [(SCLH+1) x tPRESC] */
                    uint32_t tscl_h = tafdel_min + dnf_delay + (2U * ti2cclk) + ((sclh + 1U) * tpresc);

                    /* tSCL = tf + tLOW + tr + tHIGH */
                    uint32_t tscl = tscl_l + tscl_h + I2C_Charac[I2C_speed].trise + I2C_Charac[I2C_speed].tfall;

                    if ((tscl >= clk_min) && (tscl <= clk_max) && (tscl_h >= I2C_Charac[I2C_speed].hscl_min) && (ti2cclk < tscl_h)) {
                        int32_t error = (int32_t)tscl - (int32_t)ti2cspeed;

                        if (error < 0) {
                            error = -error;
                        }

                        /* look for the timings with the lowest clock error */
                        if ((uint32_t)error < prev_error) {
                            prev_error = (uint32_t)error;
                            I2c_valid_timing[count].scll = scll;
                            I2c_valid_timing[count].sclh = sclh;
                            ret = count;
                        }
                    }
                }
            }
        }
    }

    return ret;
}

/**
  * @brief  Compute PRESC, SCLDEL and SDADEL.
  * @param  clock_src_freq I2C source clock in HZ.
  * @param  I2C_speed I2C frequency (index).
  * @retval None.
  */
void i2c_compute_presc_scldel_sdadel(uint32_t clock_src_freq, uint32_t I2C_speed)
{
    uint32_t prev_presc = I2C_PRESC_MAX;
    uint32_t ti2cclk;
    int32_t  tsdadel_min, tsdadel_max;
    int32_t  tscldel_min;
    uint32_t presc, scldel, sdadel;
    uint32_t tafdel_min, tafdel_max;

    ti2cclk   = (SEC2NSEC + (clock_src_freq / 2U)) / clock_src_freq;

    tafdel_min = (I2C_USE_ANALOG_FILTER == 1U) ? I2C_ANALOG_FILTER_DELAY_MIN : 0U;
    tafdel_max = (I2C_USE_ANALOG_FILTER == 1U) ? I2C_ANALOG_FILTER_DELAY_MAX : 0U;

    /* tDNF = DNF x tI2CCLK
       tPRESC = (PRESC+1) x tI2CCLK
       SDADEL >= {tf +tHD;DAT(min) - tAF(min) - tDNF - [3 x tI2CCLK]} / {tPRESC}
       SDADEL <= {tVD;DAT(max) - tr - tAF(max) - tDNF- [4 x tI2CCLK]} / {tPRESC} */

    tsdadel_min = (int32_t)I2C_Charac[I2C_speed].tfall + (int32_t)I2C_Charac[I2C_speed].hddat_min -
                  (int32_t)tafdel_min - (int32_t)(((int32_t)I2C_Charac[I2C_speed].dnf + 3) * (int32_t)ti2cclk);

    tsdadel_max = (int32_t)I2C_Charac[I2C_speed].vddat_max - (int32_t)I2C_Charac[I2C_speed].trise -
                  (int32_t)tafdel_max - (int32_t)(((int32_t)I2C_Charac[I2C_speed].dnf + 4) * (int32_t)ti2cclk);


    /* {[tr+ tSU;DAT(min)] / [tPRESC]} - 1 <= SCLDEL */
    tscldel_min = (int32_t)I2C_Charac[I2C_speed].trise + (int32_t)I2C_Charac[I2C_speed].sudat_min;

    if (tsdadel_min <= 0) {
        tsdadel_min = 0;
    }

    if (tsdadel_max <= 0) {
        tsdadel_max = 0;
    }

    for (presc = 0; presc < I2C_PRESC_MAX; presc++) {
        for (scldel = 0; scldel < I2C_SCLDEL_MAX; scldel++) {
            /* TSCLDEL = (SCLDEL+1) * (PRESC+1) * TI2CCLK */
            uint32_t tscldel = (scldel + 1U) * (presc + 1U) * ti2cclk;

            if (tscldel >= (uint32_t)tscldel_min) {
                for (sdadel = 0; sdadel < I2C_SDADEL_MAX; sdadel++) {
                    /* TSDADEL = SDADEL * (PRESC+1) * TI2CCLK */
                    uint32_t tsdadel = (sdadel * (presc + 1U)) * ti2cclk;

                    if ((tsdadel >= (uint32_t)tsdadel_min) && (tsdadel <= (uint32_t)tsdadel_max)) {
                        if (presc != prev_presc) {
                            I2c_valid_timing[I2c_valid_timing_nbr].presc = presc;
                            I2c_valid_timing[I2c_valid_timing_nbr].tscldel = scldel;
                            I2c_valid_timing[I2c_valid_timing_nbr].tsdadel = sdadel;
                            prev_presc = presc;
                            I2c_valid_timing_nbr ++;

                            if (I2c_valid_timing_nbr >= I2C_VALID_TIMING_NBR) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
  * @brief  Compute I2C timing according current I2C clock source and required I2C clock.
  * @param  clock_src_freq I2C clock source in Hz.
  * @param  i2c_freq Required I2C clock in Hz.
  * @retval I2C timing or 0 in case of error.
  */
uint32_t i2c_compute_timing(uint32_t clock_src_freq, uint32_t i2c_freq)
{
    uint32_t ret = 0;
    uint32_t speed;
    uint32_t idx;

    if ((clock_src_freq != 0U) && (i2c_freq != 0U)) {
        for (speed = 0 ; speed <= (uint32_t)I2C_SPEED_FREQ_FAST_PLUS ; speed++) {
            if ((i2c_freq >= I2C_Charac[speed].freq_min) &&
                    (i2c_freq <= I2C_Charac[speed].freq_max)) {
                i2c_compute_presc_scldel_sdadel(clock_src_freq, speed);
                idx = i2c_compute_scll_sclh(clock_src_freq, speed);

                if (idx < I2C_VALID_TIMING_NBR) {
                    ret = ((I2c_valid_timing[idx].presc  & 0x0FU) << 28) | \
                          ((I2c_valid_timing[idx].tscldel & 0x0FU) << 20) | \
                          ((I2c_valid_timing[idx].tsdadel & 0x0FU) << 16) | \
                          ((I2c_valid_timing[idx].sclh & 0xFFU) << 8) | \
                          ((I2c_valid_timing[idx].scll & 0xFFU) << 0);
                }
                break;
            }
        }
    }

    return ret;
}

#endif // DEVICE_I2C_ASYNCH

#endif // DEVICE_I2C
