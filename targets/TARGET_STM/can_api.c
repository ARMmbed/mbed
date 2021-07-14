/* mbed Microcontroller Library
 * Copyright (c) 2006-2017 ARM Limited
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
#include "can_api.h"

#if DEVICE_CAN

#ifdef FDCAN1

#include "pinmap.h"
#include "PeripheralPins.h"
#include "mbed_error.h"

static uint32_t can_irq_ids[2] = {0};
static can_irq_handler irq_handler;

/** Call all the init functions
 *
 *  @returns
 *    0 if mode change failed or unsupported,
 *    1 if mode change was successful
 */
int can_internal_init(can_t *obj)
{
    if (HAL_FDCAN_Init(&obj->CanHandle) != HAL_OK) {
        error("HAL_FDCAN_Init error\n");
    }

    if (can_filter(obj, 0, 0, CANStandard, 0) == 0) {
        error("can_filter error\n");
    }

    if (can_filter(obj, 0, 0, CANExtended, 0) == 0) {
        error("can_filter error\n");
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&obj->CanHandle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
        error("HAL_FDCAN_ConfigGlobalFilter error\n");
    }

    if (HAL_FDCAN_Start(&obj->CanHandle) != HAL_OK) {
        error("HAL_FDCAN_Start error\n");
    }

    return 1;
}

#if STATIC_PINMAP_READY
#define CAN_INIT_FREQ_DIRECT can_init_freq_direct
void can_init_freq_direct(can_t *obj, const can_pinmap_t *pinmap, int hz)
#else
#define CAN_INIT_FREQ_DIRECT _can_init_freq_direct
static void _can_init_freq_direct(can_t *obj, const can_pinmap_t *pinmap, int hz)
#endif
{
    MBED_ASSERT((int)pinmap->peripheral != NC);

    __HAL_RCC_FDCAN_CLK_ENABLE();

    if (pinmap->peripheral == CAN_1) {
        obj->index = 0;
    }
#if defined(FDCAN2_BASE)
    else if (pinmap->peripheral == CAN_2) {
        obj->index = 1;
    }
#endif
#if defined(FDCAN3_BASE)
    else if (pinmap->peripheral == CAN_3) {
        obj->index = 2;
    }
#endif
    else {
        error("can_init wrong instance\n");
        return;
    }

    // Select PLL1Q as source of FDCAN clock
    RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit;
    RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    RCC_PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL; // 10 MHz (RCC_OscInitStruct.PLL.PLLQ = 80)
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit) != HAL_OK) {
        error("HAL_RCCEx_PeriphCLKConfig error\n");
    }
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
    // Configure CAN pins
    pin_function(pinmap->rd_pin, pinmap->rd_function);
    pin_function(pinmap->td_pin, pinmap->td_function);
    // Add pull-ups
    pin_mode(pinmap->rd_pin, PullUp);
    pin_mode(pinmap->td_pin, PullUp);

    // Default values
    obj->CanHandle.Instance = (FDCAN_GlobalTypeDef *)pinmap->peripheral;

    /* Bit time parameter
                                ex with 100 kHz   requested frequency hz
    fdcan_ker_ck               | 10 MHz         | 10 MHz
    Prescaler                  | 1              | 1
    Time_quantum (tq)          | 100 ns         | 100 ns
    Bit_rate                   | 0.1 MBit/s     | <hz>
    Bit_length                 | 10 µs = 100 tq | <n_tq> = 10 000 000 / <hz>
    Synchronization_segment    | 1 tq           | 1 tq
    Phase_segment_1            | 69 tq          | <nts1> = <n_tq> * 0.75
    Phase_segment_2            | 30 tq          | <nts2> = <n_tq> - 1 - <nts1>
    Synchronization_Jump_width | 30 tq          | <nsjw> = <nts2>
    */

    // !Attention Not all bitrates can be covered with all fdcan-core-clk values. When a clk
    // does not work for the desired bitrate, change system_clock settings for FDCAN_CLK
    // (default FDCAN_CLK is PLLQ)
#ifdef TARGET_STM32G4
    int ntq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN) / hz;
#else
    // STM32H7 doesn't support yet HAL_RCCEx_GetPeriphCLKFreq for FDCAN
    // We use PLL1.Q clock right now so get its frequency
    PLL1_ClocksTypeDef pll1_clocks;
    HAL_RCCEx_GetPLL1ClockFreq(&pll1_clocks);
    int ntq = pll1_clocks.PLL1_Q_Frequency / hz;
#endif

    int nominalPrescaler = 1;
    // !When the sample point should be lower than 50%, this must be changed to
    // !IS_FDCAN_NOMINAL_TSEG2(ntq/nominalPrescaler), since
    // NTSEG2 and SJW max values are lower. For now the sample point is fix @75%
    while (!IS_FDCAN_NOMINAL_TSEG1(ntq / nominalPrescaler)) {
        nominalPrescaler ++;
        if (!IS_FDCAN_NOMINAL_PRESCALER(nominalPrescaler)) {
            error("Could not determine good nominalPrescaler. Bad clock value\n");
        }
    }
    ntq = ntq / nominalPrescaler;

    obj->CanHandle.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    obj->CanHandle.Init.Mode = FDCAN_MODE_NORMAL;
    obj->CanHandle.Init.AutoRetransmission = ENABLE;
    obj->CanHandle.Init.TransmitPause = DISABLE;
    obj->CanHandle.Init.ProtocolException = ENABLE;
    obj->CanHandle.Init.NominalPrescaler = nominalPrescaler;      // Prescaler
    obj->CanHandle.Init.NominalTimeSeg1 = ntq * 0.75;      // Phase_segment_1
    obj->CanHandle.Init.NominalTimeSeg2 = ntq - 1 - obj->CanHandle.Init.NominalTimeSeg1;      // Phase_segment_2
    obj->CanHandle.Init.NominalSyncJumpWidth = obj->CanHandle.Init.NominalTimeSeg2; // Synchronization_Jump_width
    obj->CanHandle.Init.DataPrescaler = 0x1;       // Not used - only in FDCAN
    obj->CanHandle.Init.DataSyncJumpWidth = 0x1;   // Not used - only in FDCAN
    obj->CanHandle.Init.DataTimeSeg1 = 0x1;        // Not used - only in FDCAN
    obj->CanHandle.Init.DataTimeSeg2 = 0x1;        // Not used - only in FDCAN
#ifdef TARGET_STM32H7
    /* Message RAM offset is only supported in STM32H7 platforms of supported FDCAN platforms */
    obj->CanHandle.Init.MessageRAMOffset = 0;

    /* The number of Standard and Extended ID filters are initialized to the maximum possile extent
     * for STM32H7 platforms
     */
    obj->CanHandle.Init.StdFiltersNbr = 128; // to be aligned with the handle parameter in can_filter
    obj->CanHandle.Init.ExtFiltersNbr = 128; // to be aligned with the handle parameter in can_filter
#else
    /* The number of Standard and Extended ID filters are initialized to the maximum possile extent 
     * for STM32G0x1, STM32G4 and STM32L5  platforms
    */
    obj->CanHandle.Init.StdFiltersNbr = 28; // to be aligned with the handle parameter in can_filter
    obj->CanHandle.Init.ExtFiltersNbr = 8; // to be aligned with the handle parameter in can_filter
#endif
#ifdef TARGET_STM32H7
    obj->CanHandle.Init.RxFifo0ElmtsNbr = 8;
    obj->CanHandle.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    obj->CanHandle.Init.RxFifo1ElmtsNbr = 0;
    obj->CanHandle.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
    obj->CanHandle.Init.RxBuffersNbr = 0;
    obj->CanHandle.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    obj->CanHandle.Init.TxEventsNbr = 3;
    obj->CanHandle.Init.TxBuffersNbr = 0;
    obj->CanHandle.Init.TxFifoQueueElmtsNbr = 3;
#endif
    obj->CanHandle.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
#ifdef TARGET_STM32H7
    obj->CanHandle.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
#endif
    can_internal_init(obj);
}

void can_init_direct(can_t *obj, const can_pinmap_t *pinmap)
{
    /* default frequency is 100 kHz */
    CAN_INIT_FREQ_DIRECT(obj, pinmap, 100000);
}

void can_init_freq(can_t *obj, PinName rd, PinName td, int hz)
{
    CANName can_rd = (CANName)pinmap_peripheral(rd, PinMap_CAN_RD);
    CANName can_td = (CANName)pinmap_peripheral(td, PinMap_CAN_TD);
    int peripheral = (int) pinmap_merge(can_rd, can_td);

    int function_rd = (int)pinmap_find_function(rd, PinMap_CAN_RD);
    int function_td = (int)pinmap_find_function(td, PinMap_CAN_TD);

    const can_pinmap_t static_pinmap = {peripheral, rd, function_rd, td, function_td};

    CAN_INIT_FREQ_DIRECT(obj, &static_pinmap, hz);
}

void can_init(can_t *obj, PinName rd, PinName td)
{
    can_init_freq(obj, rd, td, 100000);
}

void can_irq_init(can_t *obj, can_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    can_irq_ids[obj->index] = id;
}

void can_irq_free(can_t *obj)
{
    CANName can = (CANName)obj->CanHandle.Instance;
    if (can == CAN_1) {
        HAL_NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
        HAL_NVIC_DisableIRQ(FDCAN1_IT1_IRQn);
    }
#if defined(FDCAN2_BASE)
    else if (can == CAN_2) {
        HAL_NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
        HAL_NVIC_DisableIRQ(FDCAN2_IT1_IRQn);
    }
#endif
#if defined(FDCAN3_BASE)
    else if (can == CAN_3) {
        HAL_NVIC_DisableIRQ(FDCAN3_IT0_IRQn);
        HAL_NVIC_DisableIRQ(FDCAN3_IT1_IRQn);
    }
#endif
    else {
        return;
    }
#ifndef TARGET_STM32G4
    HAL_NVIC_DisableIRQ(FDCAN_CAL_IRQn);
#endif
    can_irq_ids[obj->index] = 0;
}

void can_free(can_t *obj)
{
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
    __HAL_RCC_FDCAN_FORCE_RESET();
    __HAL_RCC_FDCAN_RELEASE_RESET();
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
    __HAL_RCC_FDCAN_CLK_DISABLE();
}


/** Reset CAN interface.
 *
 * To use after error overflow.
 */
void can_reset(can_t *obj)
{
    can_mode(obj, MODE_RESET);
    HAL_FDCAN_ResetTimeoutCounter(&obj->CanHandle);
    HAL_FDCAN_ResetTimestampCounter(&obj->CanHandle);
}


int can_frequency(can_t *obj, int f)
{
    if (HAL_FDCAN_Stop(&obj->CanHandle) != HAL_OK) {
        error("HAL_FDCAN_Stop error\n");
    }


    /* See can_init_freq function for calculation details
     *
     * !Attention Not all bitrates can be covered with all fdcan-core-clk values. When a clk
     * does not work for the desired bitrate, change system_clock settings for FDCAN_CLK
     * (default FDCAN_CLK is PLLQ)
     */
#ifdef TARGET_STM32G4
    int ntq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN) / f;
#else
    // STM32H7 doesn't support yet HAL_RCCEx_GetPeriphCLKFreq for FDCAN
    PLL1_ClocksTypeDef pll1_clocks;
    HAL_RCCEx_GetPLL1ClockFreq(&pll1_clocks);
    int ntq = pll1_clocks.PLL1_Q_Frequency / f;
#endif

    int nominalPrescaler = 1;
    // !When the sample point should be lower than 50%, this must be changed to
    // !IS_FDCAN_DATA_TSEG2(ntq/nominalPrescaler), since
    // NTSEG2 and SJW max values are lower. For now the sample point is fix @75%
    while (!IS_FDCAN_DATA_TSEG1(ntq / nominalPrescaler)) {
        nominalPrescaler ++;
        if (!IS_FDCAN_NOMINAL_PRESCALER(nominalPrescaler)) {
            error("Could not determine good nominalPrescaler. Bad clock value\n");
        }
    }
    ntq = ntq / nominalPrescaler;

    obj->CanHandle.Init.NominalPrescaler = nominalPrescaler;
    obj->CanHandle.Init.NominalTimeSeg1 = ntq * 0.75;      // Phase_segment_1
    obj->CanHandle.Init.NominalTimeSeg2 = ntq - 1 - obj->CanHandle.Init.NominalTimeSeg1;      // Phase_segment_2
    obj->CanHandle.Init.NominalSyncJumpWidth = obj->CanHandle.Init.NominalTimeSeg2; // Synchronization_Jump_width

    return can_internal_init(obj);
}


/** Filter out incoming messages
 *
 *  @param obj CAN object
 *  @param id the id to filter on
 *  @param mask the mask applied to the id
 *  @param format format to filter on
 *  @param handle message filter handle (not supported yet)
 *
 *  @returns
 *    0 if filter change failed or unsupported,
 *    new filter handle if successful (not supported yet => returns 1)
 */
int can_filter(can_t *obj, uint32_t id, uint32_t mask, CANFormat format, int32_t handle)
{
    FDCAN_FilterTypeDef sFilterConfig = {0};

    if (format == CANStandard) {
        sFilterConfig.IdType = FDCAN_STANDARD_ID;
        sFilterConfig.FilterIndex = handle;
        sFilterConfig.FilterType = FDCAN_FILTER_MASK;
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
        sFilterConfig.FilterID1 = id;
        sFilterConfig.FilterID2 = mask;
    } else if (format == CANExtended) {
        sFilterConfig.IdType = FDCAN_EXTENDED_ID;
        sFilterConfig.FilterIndex = handle;
        sFilterConfig.FilterType = FDCAN_FILTER_MASK;
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
        sFilterConfig.FilterID1 = id;
        sFilterConfig.FilterID2 = mask;
    } else { // Filter for CANAny format cannot be configured for STM32
        return 0;
    }

    if (HAL_FDCAN_ConfigFilter(&obj->CanHandle, &sFilterConfig) != HAL_OK) {
        return 0;
    }

    return 1;
}


int can_write(can_t *obj, CAN_Message msg, int cc)
{
    FDCAN_TxHeaderTypeDef TxHeader = {0};

    UNUSED(cc);

    // Configure Tx buffer message
    TxHeader.Identifier = msg.id;
    if (msg.format == CANStandard) {
        TxHeader.IdType = FDCAN_STANDARD_ID;
    } else {
        TxHeader.IdType = FDCAN_EXTENDED_ID;
    }

    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = msg.len << 16;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl = FDCAN_STORE_TX_EVENTS;
    TxHeader.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&obj->CanHandle, &TxHeader, msg.data) != HAL_OK) {
        // Note for debug: you can get the error code calling HAL_FDCAN_GetError(&obj->CanHandle)
        return 0;
    }

    return 1;
}

int can_read(can_t *obj, CAN_Message *msg, int handle)
{
    UNUSED(handle); // Not supported, RXFIFO0 is set default by can_filter and cannot be changed.

    if (HAL_FDCAN_GetRxFifoFillLevel(&obj->CanHandle, FDCAN_RX_FIFO0) == 0) {
        return 0; // No message arrived
    }

    FDCAN_RxHeaderTypeDef RxHeader = {0};
    if (HAL_FDCAN_GetRxMessage(&obj->CanHandle, FDCAN_RX_FIFO0, &RxHeader, msg->data) != HAL_OK) {
        error("HAL_FDCAN_GetRxMessage error\n"); // Should not occur as previous HAL_FDCAN_GetRxFifoFillLevel call reported some data
        return 0;
    }

    if (RxHeader.IdType == FDCAN_STANDARD_ID) {
        msg->format = CANStandard;
    } else {
        msg->format = CANExtended;
    }
    msg->id   = RxHeader.Identifier;
    msg->type = (RxHeader.RxFrameType == FDCAN_DATA_FRAME) ? CANData : CANRemote;
    msg->len  = RxHeader.DataLength >> 16; // see FDCAN_data_length_code value

    return 1;
}

unsigned char can_rderror(can_t *obj)
{
    FDCAN_ErrorCountersTypeDef ErrorCounters;

    HAL_FDCAN_GetErrorCounters(&obj->CanHandle, &ErrorCounters);

    return (unsigned char)ErrorCounters.RxErrorCnt;
}

unsigned char can_tderror(can_t *obj)
{
    FDCAN_ErrorCountersTypeDef ErrorCounters;

    HAL_FDCAN_GetErrorCounters(&obj->CanHandle, &ErrorCounters);

    return (unsigned char)ErrorCounters.TxErrorCnt;
}

void can_monitor(can_t *obj, int silent)
{
    CanMode mode = MODE_NORMAL;
    if (silent) {
        switch (obj->CanHandle.Init.Mode) {
            case FDCAN_MODE_INTERNAL_LOOPBACK:
                mode = MODE_TEST_SILENT;
                break;
            default:
                mode = MODE_SILENT;
                break;
        }
    } else {
        switch (obj->CanHandle.Init.Mode) {
            case FDCAN_MODE_INTERNAL_LOOPBACK:
            case FDCAN_MODE_EXTERNAL_LOOPBACK:
                mode = MODE_TEST_LOCAL;
                break;
            default:
                mode = MODE_NORMAL;
                break;
        }
    }

    can_mode(obj, mode);
}

/** Change CAN operation to the specified mode
 *
 *  @param mode The new operation mode (MODE_RESET, MODE_NORMAL, MODE_SILENT, MODE_TEST_LOCAL, MODE_TEST_GLOBAL, MODE_TEST_SILENT)
 *
 *  @returns
 *    0 if mode change failed or unsupported,
 *    1 if mode change was successful
 */
int can_mode(can_t *obj, CanMode mode)
{
    if (HAL_FDCAN_Stop(&obj->CanHandle) != HAL_OK) {
        error("HAL_FDCAN_Stop error\n");
    }

    switch (mode) {
        case MODE_RESET:
            break;
        case MODE_NORMAL:
            obj->CanHandle.Init.Mode = FDCAN_MODE_NORMAL;
            // obj->CanHandle.Init.NominalPrescaler = 100;   // Prescaler
            break;
        case MODE_SILENT: // Bus Monitoring
            obj->CanHandle.Init.Mode = FDCAN_MODE_BUS_MONITORING;
            break;
        case MODE_TEST_GLOBAL: // External LoopBack
        case MODE_TEST_LOCAL:
            obj->CanHandle.Init.Mode = FDCAN_MODE_EXTERNAL_LOOPBACK;
            break;
        case MODE_TEST_SILENT: // Internal LoopBack
            obj->CanHandle.Init.Mode = FDCAN_MODE_INTERNAL_LOOPBACK;
            // obj->CanHandle.Init.NominalPrescaler = 1;   // Prescaler
            break;
        default:
            return 0;
    }

    return can_internal_init(obj);
}


static void can_irq(CANName name, int id)
{
    FDCAN_HandleTypeDef CanHandle;
    CanHandle.Instance = (FDCAN_GlobalTypeDef *)name;

    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_TX_COMPLETE)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_FLAG_TX_COMPLETE)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_FLAG_TX_COMPLETE);
            irq_handler(can_irq_ids[id], IRQ_TX);
        }
    }
#ifndef TARGET_STM32G4
    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_RX_BUFFER_NEW_MESSAGE)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_IT_RX_BUFFER_NEW_MESSAGE)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_IT_RX_BUFFER_NEW_MESSAGE);
            irq_handler(can_irq_ids[id], IRQ_RX);
        }
    }
#else
    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE);
            irq_handler(can_irq_ids[id], IRQ_RX);
        }
    }
#endif
    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_ERROR_WARNING)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_FLAG_ERROR_WARNING)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_FLAG_ERROR_WARNING);
            irq_handler(can_irq_ids[id], IRQ_ERROR);
        }
    }

    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_ERROR_PASSIVE)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_FLAG_ERROR_PASSIVE)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_FLAG_ERROR_PASSIVE);
            irq_handler(can_irq_ids[id], IRQ_PASSIVE);
        }
    }

    if (__HAL_FDCAN_GET_IT_SOURCE(&CanHandle, FDCAN_IT_BUS_OFF)) {
        if (__HAL_FDCAN_GET_FLAG(&CanHandle, FDCAN_FLAG_BUS_OFF)) {
            __HAL_FDCAN_CLEAR_FLAG(&CanHandle, FDCAN_FLAG_BUS_OFF);
            irq_handler(can_irq_ids[id], IRQ_BUS);
        }
    }
}

void FDCAN1_IT0_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}

void FDCAN1_IT1_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}

#if defined(FDCAN2_BASE)
void FDCAN2_IT0_IRQHandler(void)
{
    can_irq(CAN_2, 1);
}

void FDCAN2_IT1_IRQHandler(void)
{
    can_irq(CAN_2, 1);
}
#endif //FDCAN2_BASE

#if defined(FDCAN3_BASE)
void FDCAN3_IT0_IRQHandler(void)
{
    can_irq(CAN_3, 2);
}

void FDCAN3_IT1_IRQHandler(void)
{
    can_irq(CAN_3, 2);
}
#endif //FDCAN3_BASE


// TODO Add other interrupts ?
void can_irq_set(can_t *obj, CanIrqType type, uint32_t enable)
{
    uint32_t interrupts = 0;

    switch (type) {
        case IRQ_TX:
            interrupts = FDCAN_IT_TX_COMPLETE;
            break;
        case IRQ_RX:
#ifndef TARGET_STM32G4
            interrupts = FDCAN_IT_RX_BUFFER_NEW_MESSAGE;
#else
            interrupts = FDCAN_IT_RX_FIFO0_NEW_MESSAGE;
#endif
            break;
        case IRQ_ERROR:
            interrupts = FDCAN_IT_ERROR_WARNING;
            break;
        case IRQ_PASSIVE:
            interrupts = FDCAN_IT_ERROR_PASSIVE;
            break;
        case IRQ_BUS:
            interrupts = FDCAN_IT_BUS_OFF;
        default:
            return;
    }

    if (enable) {
        /* The TXBTIE register controls the TX complete interrupt in FDCAN 
         * and is only used in case of TX interrupts, Hence in case of enabling the 
         * TX interrupts the bufferIndexes of TXBTIE are to be set  */
#ifdef TARGET_STM32H7
        // TXBTIE for STM32H7 is 2 bytes long
        HAL_FDCAN_ActivateNotification(&obj->CanHandle, interrupts, 0xFFFF);
#else
        //TXBTIE for rest supported FDCAN Platforms(STM32G0x1, STM32G4 and STM32L5) is 3 bits.
        HAL_FDCAN_ActivateNotification(&obj->CanHandle, interrupts, 0x07);
#endif
    } else {
        HAL_FDCAN_DeactivateNotification(&obj->CanHandle, interrupts);
    }

    NVIC_SetVector(FDCAN1_IT0_IRQn, (uint32_t)&FDCAN1_IT0_IRQHandler);
    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    NVIC_SetVector(FDCAN1_IT1_IRQn, (uint32_t)&FDCAN1_IT1_IRQHandler);
    NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
#if defined(FDCAN2_BASE)
    NVIC_SetVector(FDCAN2_IT0_IRQn, (uint32_t)&FDCAN2_IT0_IRQHandler);
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    NVIC_SetVector(FDCAN2_IT1_IRQn, (uint32_t)&FDCAN2_IT1_IRQHandler);
    NVIC_EnableIRQ(FDCAN2_IT1_IRQn);
#endif
#if defined(FDCAN3_BASE)
    NVIC_SetVector(FDCAN3_IT0_IRQn, (uint32_t)&FDCAN3_IT0_IRQHandler);
    NVIC_EnableIRQ(FDCAN3_IT0_IRQn);
    NVIC_SetVector(FDCAN3_IT1_IRQn, (uint32_t)&FDCAN3_IT1_IRQHandler);
    NVIC_EnableIRQ(FDCAN3_IT1_IRQn);
#endif
}

#else /* FDCAN1 */

#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "mbed_error.h"
#include "can_device.h" // Specific to STM32 serie
#include <math.h>
#include <string.h>
#include <inttypes.h>

#define DEFAULT_RXFIFO    0 // default rx fifo for can by hardware is FIFO0

static uint32_t can_irq_ids[CAN_NUM] = {0};
static can_irq_handler irq_handler;

static void can_registers_init(can_t *obj)
{
    if (HAL_CAN_Init(&obj->CanHandle) != HAL_OK) {
        error("Cannot initialize CAN");
    }

    // Set initial CAN frequency to specified frequency
    if (can_frequency(obj, obj->hz) != 1) {
        error("Can frequency could not be set\n");
    }
}

#if STATIC_PINMAP_READY
#define CAN_INIT_FREQ_DIRECT can_init_freq_direct
void can_init_freq_direct(can_t *obj, const can_pinmap_t *pinmap, int hz)
#else
#define CAN_INIT_FREQ_DIRECT _can_init_freq_direct
static void _can_init_freq_direct(can_t *obj, const can_pinmap_t *pinmap, int hz)
#endif
{

    MBED_ASSERT((int)pinmap->peripheral != NC);

    if (pinmap->peripheral == CAN_1) {
        __HAL_RCC_CAN1_CLK_ENABLE();
        obj->index = 0;
    }
#if defined(CAN2_BASE) && (CAN_NUM > 1)
    else if (pinmap->peripheral == CAN_2) {
        __HAL_RCC_CAN1_CLK_ENABLE(); // needed to set filters
        __HAL_RCC_CAN2_CLK_ENABLE();
        obj->index = 1;
    }
#endif
#if defined(CAN3_BASE) && (CAN_NUM > 2)
    else if (pinmap->peripheral == CAN_3) {
        __HAL_RCC_CAN3_CLK_ENABLE();
        obj->index = 2;
    }
#endif
    else {
        return;
    }

    // Configure CAN pins
    pin_function(pinmap->rd_pin, pinmap->rd_function);
    pin_function(pinmap->td_pin, pinmap->td_function);
    // Add pull-ups
    pin_mode(pinmap->rd_pin, PullUp);
    pin_mode(pinmap->td_pin, PullUp);

    /*  Use default values for rist init */
    obj->CanHandle.Instance = (CAN_TypeDef *)pinmap->peripheral;
    obj->CanHandle.Init.TimeTriggeredMode = DISABLE;
    obj->CanHandle.Init.AutoBusOff = DISABLE;
    obj->CanHandle.Init.AutoWakeUp = DISABLE;
    obj->CanHandle.Init.AutoRetransmission = DISABLE;
    obj->CanHandle.Init.ReceiveFifoLocked = DISABLE;
    obj->CanHandle.Init.TransmitFifoPriority = DISABLE;
    obj->CanHandle.Init.Mode = CAN_MODE_NORMAL;
    obj->CanHandle.Init.SyncJumpWidth = CAN_SJW_1TQ;
    obj->CanHandle.Init.TimeSeg1 = CAN_BS1_6TQ;
    obj->CanHandle.Init.TimeSeg2 = CAN_BS2_8TQ;
    obj->CanHandle.Init.Prescaler = 2;

    /*  Store frequency to be restored in case of reset */
    obj->hz = hz;

    can_registers_init(obj);

    /* Bits 27:14 are available for dual CAN configuration and are reserved for
       single CAN configuration: */
#if defined(CAN3_BASE) && (CAN_NUM > 2)
    uint32_t filter_number = (pinmap->peripheral == CAN_1 || pinmap->peripheral == CAN_3) ? 0 : 14;
#else
    uint32_t filter_number = (pinmap->peripheral == CAN_1) ? 0 : 14;
#endif
    can_filter(obj, 0, 0, CANStandard, filter_number);
}

void can_init_direct(can_t *obj, const can_pinmap_t *pinmap)
{
    /* default frequency is 100 kHz */
    CAN_INIT_FREQ_DIRECT(obj, pinmap, 100000);
}

void can_init_freq(can_t *obj, PinName rd, PinName td, int hz)
{
    CANName can_rd = (CANName)pinmap_peripheral(rd, PinMap_CAN_RD);
    CANName can_td = (CANName)pinmap_peripheral(td, PinMap_CAN_TD);
    int peripheral = (int) pinmap_merge(can_rd, can_td);

    int function_rd = (int)pinmap_find_function(rd, PinMap_CAN_RD);
    int function_td = (int)pinmap_find_function(td, PinMap_CAN_TD);

    const can_pinmap_t static_pinmap = {peripheral, rd, function_rd, td, function_td};

    CAN_INIT_FREQ_DIRECT(obj, &static_pinmap, hz);
}

void can_init(can_t *obj, PinName rd, PinName td)
{
    can_init_freq(obj, rd, td, 100000);
}

void can_irq_init(can_t *obj, can_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    can_irq_ids[obj->index] = id;
    obj->rxIrqEnabled = false;
}

void can_irq_free(can_t *obj)
{
    CAN_TypeDef *can = obj->CanHandle.Instance;
    can->IER &= ~(CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY | \
                  CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF);

    can_irq_ids[obj->index] = 0;
    obj->rxIrqEnabled = false;
}

void can_free(can_t *obj)
{
    CANName can = (CANName) obj->CanHandle.Instance;
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
    // Reset CAN and disable clock
    if (can == CAN_1) {
        __HAL_RCC_CAN1_FORCE_RESET();
        __HAL_RCC_CAN1_RELEASE_RESET();
        __HAL_RCC_CAN1_CLK_DISABLE();
    }
#if defined(CAN2_BASE) && (CAN_NUM > 1)
    if (can == CAN_2) {
        __HAL_RCC_CAN2_FORCE_RESET();
        __HAL_RCC_CAN2_RELEASE_RESET();
        __HAL_RCC_CAN2_CLK_DISABLE();
    }
#endif
#if defined(CAN3_BASE) && (CAN_NUM > 2)
    if (can == CAN_3) {
        __HAL_RCC_CAN3_FORCE_RESET();
        __HAL_RCC_CAN3_RELEASE_RESET();
        __HAL_RCC_CAN3_CLK_DISABLE();
    }
#endif
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
}

// The following table is used to program bit_timing. It is an adjustment of the sample
// point by synchronizing on the start-bit edge and resynchronizing on the following edges.
// This table has the sampling points as close to 75% as possible (most commonly used).
// The first value is TSEG1, the second TSEG2.
static const int timing_pts[23][2] = {
    {0x0, 0x0},      // 2,  67%
    {0x1, 0x0},      // 3,  75%
    {0x2, 0x0},      // 4,  80%
    {0x2, 0x1},      // 5,  67%
    {0x3, 0x1},      // 6,  71%
    {0x4, 0x1},      // 7,  75%
    {0x4, 0x2},      // 8,  67%
    {0x5, 0x2},      // 9,  70%
    {0x6, 0x2},      // 10, 73%
    {0x7, 0x2},      // 11, 75%
    {0x7, 0x3},      // 12, 69%
    {0x8, 0x3},      // 13, 71%
    {0x9, 0x3},      // 14, 73%
    {0xA, 0x3},      // 15, 75%
    {0xA, 0x4},      // 16, 71%
    {0xB, 0x4},      // 17, 72%
    {0xC, 0x4},      // 18, 74%
    {0xD, 0x4},      // 19, 75%
    {0xD, 0x5},      // 20, 71%
    {0xE, 0x5},      // 21, 73%
    {0xF, 0x5},      // 22, 74%
    {0xF, 0x6},      // 23, 71%
    {0xF, 0x7},      // 24, 68%
};

static unsigned int can_speed(unsigned int pclk, unsigned int cclk, unsigned char psjw)
{
    uint32_t    btr;
    uint16_t    brp = 0;
    uint32_t    calcbit;
    uint32_t    bitwidth;
    int         hit = 0;
    int         bits;

    bitwidth = (pclk / cclk);

    brp = bitwidth / 0x18;
    while ((!hit) && (brp < bitwidth / 4)) {
        brp++;
        for (bits = 22; bits > 0; bits--) {
            calcbit = (bits + 3) * (brp + 1);
            if (calcbit == bitwidth) {
                hit = 1;
                break;
            }
        }
    }

    if (hit) {
        btr = ((timing_pts[bits][1] << CAN_BTR_TS2_Pos) & CAN_BTR_TS2) |
              ((timing_pts[bits][0] << CAN_BTR_TS1_Pos) & CAN_BTR_TS1) |
              ((psjw                << CAN_BTR_SJW_Pos) & CAN_BTR_SJW) |
              ((brp                 << CAN_BTR_BRP_Pos) & CAN_BTR_BRP);
    } else {
        btr = 0xFFFFFFFF;
    }

    return btr;

}

int can_frequency(can_t *obj, int f)
{
    int pclk = HAL_RCC_GetPCLK1Freq();
    int btr = can_speed(pclk, (unsigned int)f, 1);
    CAN_TypeDef *can = obj->CanHandle.Instance;
    uint32_t tickstart = 0;
    int status = 1;

    if (btr > 0) {
        can->MCR |= CAN_MCR_INRQ ;
        /* Get tick */
        tickstart = HAL_GetTick();
        while ((can->MSR & CAN_MSR_INAK) != CAN_MSR_INAK) {
            if ((HAL_GetTick() - tickstart) > 2) {
                status = 0;
                break;
            }
        }
        if (status != 0) {
            /*  Do not erase all BTR registers (e.g. silent mode), only the
             *  ones calculated in can_speed */
            can->BTR &= ~(CAN_BTR_TS2 | CAN_BTR_TS1 | CAN_BTR_SJW | CAN_BTR_BRP);
            can->BTR |= btr;

            can->MCR &= ~(uint32_t)CAN_MCR_INRQ;
            /* Get tick */
            tickstart = HAL_GetTick();
            while ((can->MSR & CAN_MSR_INAK) == CAN_MSR_INAK) {
                if ((HAL_GetTick() - tickstart) > 2) {
                    status = 0;
                    break;
                }
            }
            if (status == 0) {
                error("can ESR  0x%04" PRIx32 ".%04" PRIx32 " + timeout status %d", (can->ESR & 0xFFFF0000) >> 16, (can->ESR & 0xFFFF), status);
            }
        } else {
            error("can init request timeout\n");
        }
    } else {
        status = 0;
    }
    return status;
}

int can_write(can_t *obj, CAN_Message msg, int cc)
{
    uint32_t  transmitmailbox;
    CAN_TypeDef *can = obj->CanHandle.Instance;

    /* Select one empty transmit mailbox */
    if ((can->TSR & CAN_TSR_TME0) == CAN_TSR_TME0) {
        transmitmailbox = 0;
    } else if ((can->TSR & CAN_TSR_TME1) == CAN_TSR_TME1) {
        transmitmailbox = 1;
    } else if ((can->TSR & CAN_TSR_TME2) == CAN_TSR_TME2) {
        transmitmailbox = 2;
    } else {
        return 0;
    }

    can->sTxMailBox[transmitmailbox].TIR &= CAN_TI0R_TXRQ;
    if (!(msg.format)) {
        can->sTxMailBox[transmitmailbox].TIR |= ((msg.id << 21) | (msg.type << 1));
    } else {
        can->sTxMailBox[transmitmailbox].TIR |= ((msg.id << 3) | CAN_ID_EXT | (msg.type << 1));
    }

    /* Set up the DLC */
    can->sTxMailBox[transmitmailbox].TDTR &= (uint32_t)0xFFFFFFF0;
    can->sTxMailBox[transmitmailbox].TDTR |= (msg.len & (uint8_t)0x0000000F);

    /* Set up the data field */
    can->sTxMailBox[transmitmailbox].TDLR = (((uint32_t)msg.data[3] << 24) |
                                             ((uint32_t)msg.data[2] << 16) |
                                             ((uint32_t)msg.data[1] << 8) |
                                             ((uint32_t)msg.data[0]));
    can->sTxMailBox[transmitmailbox].TDHR = (((uint32_t)msg.data[7] << 24) |
                                             ((uint32_t)msg.data[6] << 16) |
                                             ((uint32_t)msg.data[5] << 8) |
                                             ((uint32_t)msg.data[4]));
    /* Request transmission */
    can->sTxMailBox[transmitmailbox].TIR |= CAN_TI0R_TXRQ;

    return 1;
}

int can_read(can_t *obj, CAN_Message *msg, int handle)
{
    //FIFO selection cannot be controlled by software for STM32, default FIFO is 0, hence handle is not used
    int rxfifo_default = DEFAULT_RXFIFO; 
    CAN_TypeDef *can = obj->CanHandle.Instance;

    // check FPM0 which holds the pending message count in FIFO 0
    // if no message is pending, return 0
    if ((can->RF0R & CAN_RF0R_FMP0) == 0) {
        return 0;
    }

    /* Get the Id */
    msg->format = (CANFormat)(((uint8_t)0x04 & can->sFIFOMailBox[rxfifo_default].RIR) >> 2);
    if (!msg->format) {
        msg->id = (uint32_t)0x000007FF & (can->sFIFOMailBox[rxfifo_default].RIR >> 21);
    } else {
        msg->id = (uint32_t)0x1FFFFFFF & (can->sFIFOMailBox[rxfifo_default].RIR >> 3);
    }

    msg->type = (CANType)(((uint8_t)0x02 & can->sFIFOMailBox[rxfifo_default].RIR) >> 1);
    /* Get the DLC */
    msg->len = (uint8_t)0x0F & can->sFIFOMailBox[rxfifo_default].RDTR;
    /* Get the FMI */
    // msg->FMI = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDTR >> 8);
    /* Get the data field */
    msg->data[0] = (uint8_t)0xFF & can->sFIFOMailBox[rxfifo_default].RDLR;
    msg->data[1] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDLR >> 8);
    msg->data[2] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDLR >> 16);
    msg->data[3] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDLR >> 24);
    msg->data[4] = (uint8_t)0xFF & can->sFIFOMailBox[rxfifo_default].RDHR;
    msg->data[5] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDHR >> 8);
    msg->data[6] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDHR >> 16);
    msg->data[7] = (uint8_t)0xFF & (can->sFIFOMailBox[rxfifo_default].RDHR >> 24);

    /* Release the FIFO */
    can->RF0R |= CAN_RF0R_RFOM0;

    if(obj->rxIrqEnabled == true) {
        __HAL_CAN_ENABLE_IT(&obj->CanHandle, CAN_IT_RX_FIFO0_MSG_PENDING);
    }

    return 1;
}

void can_reset(can_t *obj)
{
    CAN_TypeDef *can = obj->CanHandle.Instance;

    /* Reset IP and delete errors */
    can->MCR |= CAN_MCR_RESET;
    can->ESR = 0x0;

    /* restore registers state as saved in obj context */
    can_registers_init(obj);
    obj->rxIrqEnabled = false;
}

unsigned char can_rderror(can_t *obj)
{
    CAN_TypeDef *can = obj->CanHandle.Instance;
    return (can->ESR >> 24) & 0xFF;
}

unsigned char can_tderror(can_t *obj)
{
    CAN_TypeDef *can = obj->CanHandle.Instance;
    return (can->ESR >> 16) & 0xFF;
}

void can_monitor(can_t *obj, int silent)
{
    CanMode mode = MODE_NORMAL;
    /*  Update current state w/ or w/o silent */
    if (silent) {
        switch (obj->CanHandle.Init.Mode) {
            case CAN_MODE_LOOPBACK:
            case CAN_MODE_SILENT_LOOPBACK:
                mode = MODE_TEST_SILENT;
                break;
            default:
                mode = MODE_SILENT;
                break;
        }
    } else {
        switch (obj->CanHandle.Init.Mode) {
            case CAN_MODE_LOOPBACK:
            case CAN_MODE_SILENT_LOOPBACK:
                mode = MODE_TEST_LOCAL;
                break;
            default:
                mode = MODE_NORMAL;
                break;
        }
    }

    can_mode(obj, mode);
}

int can_mode(can_t *obj, CanMode mode)
{
    int success = 0;
    CAN_TypeDef *can = obj->CanHandle.Instance;

    can->MCR |= CAN_MCR_INRQ ;
    while ((can->MSR & CAN_MSR_INAK) != CAN_MSR_INAK) {
    }

    switch (mode) {
        case MODE_NORMAL:
            obj->CanHandle.Init.Mode = CAN_MODE_NORMAL;
            can->BTR &= ~(CAN_BTR_SILM | CAN_BTR_LBKM);
            success = 1;
            break;
        case MODE_SILENT:
            obj->CanHandle.Init.Mode = CAN_MODE_SILENT;
            can->BTR |= CAN_BTR_SILM;
            can->BTR &= ~CAN_BTR_LBKM;
            success = 1;
            break;
        case MODE_TEST_GLOBAL:
        case MODE_TEST_LOCAL:
            obj->CanHandle.Init.Mode = CAN_MODE_LOOPBACK;
            can->BTR |= CAN_BTR_LBKM;
            can->BTR &= ~CAN_BTR_SILM;
            success = 1;
            break;
        case MODE_TEST_SILENT:
            obj->CanHandle.Init.Mode = CAN_MODE_SILENT_LOOPBACK;
            can->BTR |= (CAN_BTR_SILM | CAN_BTR_LBKM);
            success = 1;
            break;
        default:
            success = 0;
            break;
    }

    can->MCR &= ~(uint32_t)CAN_MCR_INRQ;
    while ((can->MSR & CAN_MSR_INAK) == CAN_MSR_INAK) {
    }

    return success;
}

int can_filter(can_t *obj, uint32_t id, uint32_t mask, CANFormat format, int32_t handle)
{
    int success = 0;

    // filter for CANAny format cannot be configured for STM32
    if ((format == CANStandard) || (format == CANExtended)) {
        CAN_FilterTypeDef  sFilterConfig;
        sFilterConfig.FilterBank = handle;
        sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
        sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

        if (format == CANStandard) {
            sFilterConfig.FilterIdHigh = id << 5;
            sFilterConfig.FilterIdLow =  0x0;
            sFilterConfig.FilterMaskIdHigh = mask << 5;
            sFilterConfig.FilterMaskIdLow = 0x0; // allows both remote and data frames
        } else { // format == CANExtended
            sFilterConfig.FilterIdHigh = id >> 13; // EXTID[28:13]
            sFilterConfig.FilterIdLow = (0xFFFF & (id << 3)) | (1 << 2); // EXTID[12:0] + IDE
            sFilterConfig.FilterMaskIdHigh = mask >> 13;
            sFilterConfig.FilterMaskIdLow = (0xFFFF & (mask << 3)) | (1 << 2);
        }

        sFilterConfig.FilterFIFOAssignment = 0;
        sFilterConfig.FilterActivation = ENABLE;
        sFilterConfig.SlaveStartFilterBank = 14;

        if (HAL_CAN_ConfigFilter(&obj->CanHandle, &sFilterConfig) == HAL_OK) {
            success = 1;
        }
    } else if (format == CANAny) {
        success = 0;	// filter for CANAny is not supported by STM32, return a failure
    }

    return success;
}

static void can_irq(CANName name, int id)
{
    uint32_t tmp1 = 0, tmp2 = 0, tmp3 = 0;
    CAN_HandleTypeDef CanHandle;
    CanHandle.Instance = (CAN_TypeDef *)name;

    if (__HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_TX_MAILBOX_EMPTY)) {
        tmp1 = __HAL_CAN_TRANSMIT_STATUS(&CanHandle, CAN_TX_MAILBOX0);
        tmp2 = __HAL_CAN_TRANSMIT_STATUS(&CanHandle, CAN_TX_MAILBOX1);
        tmp3 = __HAL_CAN_TRANSMIT_STATUS(&CanHandle, CAN_TX_MAILBOX2);

        if (tmp1) {
            __HAL_CAN_CLEAR_FLAG(&CanHandle, CAN_FLAG_RQCP0);
        }
        if (tmp2) {
            __HAL_CAN_CLEAR_FLAG(&CanHandle, CAN_FLAG_RQCP1);
        }
        if (tmp3) {
            __HAL_CAN_CLEAR_FLAG(&CanHandle, CAN_FLAG_RQCP2);
        }
        if (tmp1 || tmp2 || tmp3) {
            irq_handler(can_irq_ids[id], IRQ_TX);
        }
    }

    tmp1 = __HAL_CAN_MSG_PENDING(&CanHandle, CAN_RX_FIFO0);
    tmp2 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_RX_FIFO0_MSG_PENDING);

    // In legacy can (bxCAN and earlier), reading is the only way to clear rx interrupt. But can_read has mutex locks
    // since mutexes cannot be used in ISR context, rx interrupt is masked here to temporary disable it
    // rx interrupts will be unamsked in read operation. reads must be deffered to thread context.
    // refer to the CAN receive interrupt problem due to mutex and resolution section of README doc.
    __HAL_CAN_DISABLE_IT(&CanHandle, CAN_IT_RX_FIFO0_MSG_PENDING);

    if ((tmp1 != 0) && tmp2) {
        irq_handler(can_irq_ids[id], IRQ_RX);
    }

    tmp1 = __HAL_CAN_GET_FLAG(&CanHandle, CAN_FLAG_EPV);
    tmp2 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_ERROR_PASSIVE);
    tmp3 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_ERROR_WARNING);

    if (tmp1 && tmp2 && tmp3) {
        irq_handler(can_irq_ids[id], IRQ_PASSIVE);
    }

    tmp1 = __HAL_CAN_GET_FLAG(&CanHandle, CAN_FLAG_BOF);
    tmp2 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_BUSOFF);
    tmp3 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_ERROR_WARNING);
    if (tmp1 && tmp2 && tmp3) {
        irq_handler(can_irq_ids[id], IRQ_BUS);
    }

    tmp3 = __HAL_CAN_GET_IT_SOURCE(&CanHandle, CAN_IT_ERROR_WARNING);
    if (tmp1 && tmp2 && tmp3) {
        irq_handler(can_irq_ids[id], IRQ_ERROR);
    }
}

#if defined(TARGET_STM32F0)
void CAN_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
#elif defined(TARGET_STM32F3)
void CAN_RX0_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
void CAN_TX_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
void CAN_SCE_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
#else
void CAN1_RX0_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
void CAN1_TX_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
void CAN1_SCE_IRQHandler(void)
{
    can_irq(CAN_1, 0);
}
#if defined(CAN2_BASE) && (CAN_NUM > 1)
void CAN2_RX0_IRQHandler(void)
{
    can_irq(CAN_2, 1);
}
void CAN2_TX_IRQHandler(void)
{
    can_irq(CAN_2, 1);
}
void CAN2_SCE_IRQHandler(void)
{
    can_irq(CAN_2, 1);
}
#endif
#if defined(CAN3_BASE) && (CAN_NUM > 2)
void CAN3_RX0_IRQHandler(void)
{
    can_irq(CAN_3, 2);
}
void CAN3_TX_IRQHandler(void)
{
    can_irq(CAN_3, 2);
}
void CAN3_SCE_IRQHandler(void)
{
    can_irq(CAN_3, 2);
}
#endif
#endif // else

void can_irq_set(can_t *obj, CanIrqType type, uint32_t enable)
{
    CAN_TypeDef *can = obj->CanHandle.Instance;
    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t vector = 0;
    uint32_t ier;

    if ((CANName) can == CAN_1) {
        switch (type) {
            case IRQ_RX:
                ier = CAN_IT_RX_FIFO0_MSG_PENDING;
                irq_n = CAN1_IRQ_RX_IRQN;
                vector = (uint32_t)&CAN1_IRQ_RX_VECT;
                obj->rxIrqEnabled = true;
                break;
            case IRQ_TX:
                ier = CAN_IT_TX_MAILBOX_EMPTY;
                irq_n = CAN1_IRQ_TX_IRQN;
                vector = (uint32_t)&CAN1_IRQ_TX_VECT;
                break;
            case IRQ_ERROR:
                ier = CAN_IT_ERROR_WARNING;
                irq_n = CAN1_IRQ_ERROR_IRQN;
                vector = (uint32_t)&CAN1_IRQ_ERROR_VECT;
                break;
            case IRQ_PASSIVE:
                ier = CAN_IT_ERROR_PASSIVE;
                irq_n = CAN1_IRQ_PASSIVE_IRQN;
                vector = (uint32_t)&CAN1_IRQ_PASSIVE_VECT;
                break;
            case IRQ_BUS:
                ier = CAN_IT_BUSOFF;
                irq_n = CAN1_IRQ_BUS_IRQN;
                vector = (uint32_t)&CAN1_IRQ_BUS_VECT;
                break;
            default:
                return;
        }
    }
#if defined(CAN2_BASE) && (CAN_NUM > 1)
    else if ((CANName) can == CAN_2) {
        switch (type) {
            case IRQ_RX:
                ier = CAN_IT_RX_FIFO0_MSG_PENDING;
                irq_n = CAN2_IRQ_RX_IRQN;
                vector = (uint32_t)&CAN2_IRQ_RX_VECT;
                obj->rxIrqEnabled = true;
                break;
            case IRQ_TX:
                ier = CAN_IT_TX_MAILBOX_EMPTY;
                irq_n = CAN2_IRQ_TX_IRQN;
                vector = (uint32_t)&CAN2_IRQ_TX_VECT;
                break;
            case IRQ_ERROR:
                ier = CAN_IT_ERROR_WARNING;
                irq_n = CAN2_IRQ_ERROR_IRQN;
                vector = (uint32_t)&CAN2_IRQ_ERROR_VECT;
                break;
            case IRQ_PASSIVE:
                ier = CAN_IT_ERROR_PASSIVE;
                irq_n = CAN2_IRQ_PASSIVE_IRQN;
                vector = (uint32_t)&CAN2_IRQ_PASSIVE_VECT;
                break;
            case IRQ_BUS:
                ier = CAN_IT_BUSOFF;
                irq_n = CAN2_IRQ_BUS_IRQN;
                vector = (uint32_t)&CAN2_IRQ_BUS_VECT;
                break;
            default:
                return;
        }
    }
#endif
#if defined(CAN3_BASE) && (CAN_NUM > 2)
    else if ((CANName) can == CAN_3) {
        switch (type) {
            case IRQ_RX:
                ier = CAN_IT_RX_FIFO0_MSG_PENDING;
                irq_n = CAN3_IRQ_RX_IRQN;
                vector = (uint32_t)&CAN3_IRQ_RX_VECT;
                obj->rxIrqEnabled = true;
                break;
            case IRQ_TX:
                ier = CAN_IT_TX_MAILBOX_EMPTY;
                irq_n = CAN3_IRQ_TX_IRQN;
                vector = (uint32_t)&CAN3_IRQ_TX_VECT;
                break;
            case IRQ_ERROR:
                ier = CAN_IT_ERROR_WARNING;
                irq_n = CAN3_IRQ_ERROR_IRQN;
                vector = (uint32_t)&CAN3_IRQ_ERROR_VECT;
                break;
            case IRQ_PASSIVE:
                ier = CAN_IT_ERROR_PASSIVE;
                irq_n = CAN3_IRQ_PASSIVE_IRQN;
                vector = (uint32_t)&CAN3_IRQ_PASSIVE_VECT;
                break;
            case IRQ_BUS:
                ier = CAN_IT_BUSOFF;
                irq_n = CAN3_IRQ_BUS_IRQN;
                vector = (uint32_t)&CAN3_IRQ_BUS_VECT;
                break;
            default:
                return;
        }
    }
#endif
    else {
        return;
    }

    if (enable) {
        can->IER |= ier;
    } else {
        can->IER &= ~ier;
    }

    NVIC_SetVector(irq_n, vector);
    NVIC_EnableIRQ(irq_n);
}

#endif /* FDCAN1 */

const PinMap *can_rd_pinmap()
{
    return PinMap_CAN_TD;
}

const PinMap *can_td_pinmap()
{
    return PinMap_CAN_RD;
}

#endif // DEVICE_CAN
