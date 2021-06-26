/*
 * Copyright (c) 2019 ARM Limited
 * Copyright (c) 2019 STMicroelectronics
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

#include <stdio.h>
#include "ble/common/blecommon.h"
#include "ble/driver/CordioHCIDriver.h"
#include "ble/driver/CordioHCITransportDriver.h"
#include "rtos/Semaphore.h"
#include "hci_api.h"
#include "hci_cmd.h"
#include "hci_core.h"
#include "dm_api.h"
#include "bstream.h"
#include "hci_mbed_os_adaptation.h"
#include "mbed_trace.h"
#include "platform/mbed_error.h"

/* STM32WB include files */
#include "stm32wbxx_ll_ipcc.h"
#include "stm32wbxx_ll_system.h"
#include "ble_bufsize.h"
#include "tl.h"
#include "shci.h"
#include "shci_tl.h"
#include "hw.h"
#include "app_conf.h"
#include "otp.h"

/* mbed trace feature is supported */
/* ex in mbed_app.json */
/*   "mbed-trace.enable": "1" */
#define TRACE_GROUP "BLWB"

/******************************************************************************
 * BLE config parameters
 ******************************************************************************/
/*  Defined from WB Cube reference SW */
#define CFG_TLBLE_EVT_QUEUE_LENGTH 5
#define CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE 255   /**< Set to 255 with the memory manager and the mailbox */
#define TL_BLE_EVENT_FRAME_SIZE ( TL_EVT_HDR_SIZE + CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE )
#define POOL_SIZE (CFG_TLBLE_EVT_QUEUE_LENGTH*4*DIVC(( sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE ), 4))

#define CONFIG_DATA_PUBADDR_OFFSET          (0x00) /**< Bluetooth public address */
#define CONFIG_DATA_PUBADDR_LEN             (6)

/* HCI related defines */
#define HCI_RESET_RAND_CNT        4
#define VENDOR_SPECIFIC_EVENT     0xFF
#define ACI_HAL_SET_TX_POWER_LEVEL 0xFC0F
#define ACI_WRITE_CONFIG_DATA_OPCODE 0xFC0C
#define ACI_READ_CONFIG_DATA_OPCODE 0xFC0D
#define MAX_HCI_ACL_PACKET_SIZE (sizeof(TL_PacketHeader_t) + 5 + 251)
#define MAX_HACI_EVT_SIZE (255+5)

/* mbed_trace: debug traces (tr_debug) can be disabled here with no change in mbed_app.json */
// #undef TRACE_LEVEL_DEBUG
// #define TRACE_LEVEL_DEBUG 0

/******************************************************************************
 * BLE config parameters
 ******************************************************************************/
static void evt_received(TL_EvtPacket_t *hcievt);
static void syscmd_status_not(SHCI_TL_CmdStatus_t status);
static void sysevt_received(void *pdata);
static void acl_data_ack(void);
static bool acl_data_wait(void);
static void init_debug(void);
static bool get_bd_address(uint8_t *bd_addr);
static bool sysevt_wait(void);
static bool sysevt_check(void);

#if DEVICE_FLASH
extern int BLE_inited;
#endif

namespace ble {
namespace vendor {
namespace stm32wb {

/**
 * stm32wb HCI driver implementation
 * @see CordioHCIDriver
 */
class HCIDriver : public CordioHCIDriver {
public:
    /**
     * Construction of the HCIDriver.
     * @param transport: Transport of the HCI commands.
     * @param rst: Name of the reset pin
     */
    HCIDriver(
        CordioHCITransportDriver &transport_driver
    ) : CordioHCIDriver(transport_driver) { }

    virtual buf_pool_desc_t get_buffer_pool_description();
    /**
     * @see CordioHCIDriver::do_initialize
     */
    virtual void do_initialize()
    {
        // Nothig needed, init is only at transpot layer level
    }

    /**
     * @see CordioHCIDriver::do_terminate
     */
    virtual void do_terminate()
    {
        // Nothig needed, init is only at transpot layer level
    }

    /**
    * @see CordioHCIDriver::start_reset_sequence
    */
    virtual void start_reset_sequence()
    {
        /* send an HCI Reset command to start the sequence */
        HciResetCmd();
    }

    static uint8_t convert_db_to_tx_power_index(int8_t level_db)
    {
        const int8_t conversion[] = {
            -40, -21, -20, -19,
            -18, -16, -15, -14,
            -13, -12, -11, -10,
             -9,  -8,  -7,  -6,
             -5,  -4,  -3,  -2,
             -1,  -1,  -1,  -1,
              0,   0,   1,   2,
              3,   4,   5,   6
        };

        uint8_t index;
        for (index = 0; index < sizeof(conversion); ++index) {
            if (level_db <= conversion[index]) {
                break;
            }
        }
        return index;
    }

    virtual ble_error_t set_tx_power(int8_t level_db)
    {


        uint8_t buf[2];
        buf[0] = 0x1; // Enable high power mode - deprecated and ignored on STM32WB
        buf[1] = convert_db_to_tx_power_index(level_db);

        HciVendorSpecificCmd(ACI_HAL_SET_TX_POWER_LEVEL, 2, buf);

        return BLE_ERROR_NONE;
    }

    /**
     * @see CordioHCIDriver::handle_reset_sequence
     */
    virtual void handle_reset_sequence(uint8_t *pMsg)
    {
        uint16_t       opcode;
        static uint8_t randCnt;

        /* if event is a command complete event */
        if (*pMsg == HCI_CMD_CMPL_EVT) {
            tr_debug("Command Complete Event Command");
            /* parse parameters */
            tr_debug("  HCI_EVT_HDR_LEN=%d", HCI_EVT_HDR_LEN);
            pMsg += HCI_EVT_HDR_LEN;
            pMsg++;                   /* skip num packets */
            BSTREAM_TO_UINT16(opcode, pMsg);
            pMsg++;                   /* skip status */

            /* decode opcode */
            tr_debug("  opcode = %#06x", opcode);
            switch (opcode) {
                case HCI_OPCODE_RESET:
                    /* initialize rand command count */
                    randCnt = 0;
                    tr_info("WB Reset Received");
                    /* Once reset complete evet is received we need
                     * to send a few more commands:
                     * Tx power and set bd addr
                     */
                    if (get_bd_address(bd_addr)) {
                        aciWriteConfigData(CONFIG_DATA_PUBADDR_OFFSET, bd_addr);
                        tr_info("Setting Bdaddr: %02x:%02x:%02x:%02x:%02x:%02x",
                                bd_addr[0],
                                bd_addr[1],
                                bd_addr[2],
                                bd_addr[3],
                                bd_addr[4],
                                bd_addr[5]);
                    } else {
                        tr_info("could not find BDaddr");
                        /*  Skip to next step */
                        set_tx_power(MBED_CONF_CORDIO_PREFERRED_TX_POWER);
                    }
                    break;

                case ACI_WRITE_CONFIG_DATA_OPCODE:
                    tr_debug("Bluetooth Device address set");
                    /* set the event mask to control which events are generated by the
                     * controller for the host */
                    set_tx_power(MBED_CONF_CORDIO_PREFERRED_TX_POWER);
                    break;


                case ACI_HAL_SET_TX_POWER_LEVEL:
                    tr_debug("Tx Power Level set");
                    //signal_reset_sequence_done();
                    HciSetEventMaskCmd((uint8_t *) hciEventMask);
                    break;

                case HCI_OPCODE_SET_EVENT_MASK:
                    // set the event mask to control which LE events are generated by
                    // the controller for the host
                    HciLeSetEventMaskCmd((uint8_t *) hciLeEventMask);
                    break;

                case HCI_OPCODE_LE_SET_EVENT_MASK:
                    /* below command is not supported */
#if COMMAND_NOT_SUPPORTED_SKIP_STEP
                    // set the event mask to control which events are generated by the
                    // controller for the host (2nd page of flags )
                    HciSetEventMaskPage2Cmd((uint8_t *) hciEventMaskPage2);
                    break;

                case HCI_OPCODE_SET_EVENT_MASK_PAGE2:
#endif
                    // Ask the Bluetooth address of the controller
                    HciReadBdAddrCmd();
                    break;

                case HCI_OPCODE_READ_BD_ADDR:
                    // Store the Bluetooth address in the stack runtime parameter
                    BdaCpy(hciCoreCb.bdAddr, pMsg);
                    // Read the size of the buffer of the controller
                    HciLeReadBufSizeCmd();
                    break;

                case HCI_OPCODE_LE_READ_BUF_SIZE:
                    // Store the buffer parameters in the stack runtime parameters
                    BSTREAM_TO_UINT16(hciCoreCb.bufSize, pMsg);
                    BSTREAM_TO_UINT8(hciCoreCb.numBufs, pMsg);
                    /* initialize ACL buffer accounting */
                    hciCoreCb.availBufs = hciCoreCb.numBufs;
                    // read the states and state combinations supported by the link
                    // layer of the controller
                    HciLeReadSupStatesCmd();
                    break;

                case HCI_OPCODE_LE_READ_SUP_STATES:
                    // store supported state and combination in the runtime parameters
                    // of the stack
                    memcpy(hciCoreCb.leStates, pMsg, HCI_LE_STATES_LEN);
                    // read the total of whitelist entries that can be stored in the
                    // controller.
                    HciLeReadWhiteListSizeCmd();
                    break;

                case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
                    // store the number of whitelist entries in the stack runtime
                    // parameters
                    BSTREAM_TO_UINT8(hciCoreCb.whiteListSize, pMsg);

                    // Read the LE features supported by the controller
                    HciLeReadLocalSupFeatCmd();
                    break;

                case HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT:
                    // Store the set of LE features supported by the controller
                    BSTREAM_TO_UINT16(hciCoreCb.leSupFeat, pMsg);
                    // read the total number of address translation entries which can be
                    // stored in the controller resolving list.
                    hciCoreReadResolvingListSize();
                    break;

                case HCI_OPCODE_LE_READ_RES_LIST_SIZE:
                    // store the number of address translation entries in the stack
                    // runtime parameter
                    BSTREAM_TO_UINT8(hciCoreCb.resListSize, pMsg);

                    // read the Controller's maximum supported payload octets and packet
                    // duration times for transmission and reception
                    hciCoreReadMaxDataLen();
                    break;

                case HCI_OPCODE_LE_READ_MAX_DATA_LEN: {
                    // store payload definition in the runtime stack parameters.
                    uint16_t maxTxOctets;
                    uint16_t maxTxTime;

                    BSTREAM_TO_UINT16(maxTxOctets, pMsg);
                    BSTREAM_TO_UINT16(maxTxTime, pMsg);

                    /* use Controller's maximum supported payload octets and packet duration times
                    * for transmission as Host's suggested values for maximum transmission number
                    * of payload octets and maximum packet transmission time for new connections.
                    */
                    HciLeWriteDefDataLen(maxTxOctets, maxTxTime);
                }
                break;

                case HCI_OPCODE_LE_WRITE_DEF_DATA_LEN:
                    if (hciCoreCb.extResetSeq) {
                        HciReadLocalVerInfoCmd();
                    } else {
                        /* initialize extended parameters */
                        hciCoreCb.maxAdvDataLen = 0;
                        hciCoreCb.numSupAdvSets = 0;
                        hciCoreCb.perAdvListSize = 0;
                        /* send next command in sequence */
                        HciLeRandCmd();
                    }
                    break;

                case HCI_OPCODE_READ_LOCAL_VER_INFO:
                case HCI_OPCODE_LE_READ_MAX_ADV_DATA_LEN:
                case HCI_OPCODE_LE_READ_NUM_SUP_ADV_SETS:
                case HCI_OPCODE_LE_READ_PER_ADV_LIST_SIZE:
                    // handle extended command
                    if (hciCoreCb.extResetSeq) {
                        /* send next extended command in sequence */
                        (*hciCoreCb.extResetSeq)(pMsg, opcode);
                    }
                    break;

                case HCI_OPCODE_LE_RAND:
                    /* check if need to send second rand command */
                    if (randCnt < (HCI_RESET_RAND_CNT - 1)) {
                        randCnt++;
                        HciLeRandCmd();
                    } else {
                        uint8_t addr[6] = { 0 };
                        memcpy(addr, pMsg, sizeof(addr));
                        DM_RAND_ADDR_SET(addr, DM_RAND_ADDR_STATIC);
                        // note: will invoke set rand address
                        set_random_static_address(addr);
                    }
                    break;

                case HCI_OPCODE_LE_SET_RAND_ADDR:
                    /* send next command in sequence */
                    signal_reset_sequence_done();
                    break;

                default:
                    tr_info("Complete Event in reset seq with unknown opcode =0x%4X", opcode);
                    break;
            }
        } else if (*pMsg == HCI_CMD_STATUS_EVT) {
            uint8_t status;
            /* get status */
            /* parse parameters */
            pMsg += HCI_EVT_HDR_LEN;
            status = *pMsg;
            pMsg++;
            pMsg++;                   /* skip num packets */
            BSTREAM_TO_UINT16(opcode, pMsg);
            tr_info("Command Status event, status:%d, opcode=0x%4X", status, opcode);
        } else {
            /**
             * vendor specific event
             */
            if (pMsg[0] == VENDOR_SPECIFIC_EVENT) {
                /* parse parameters */
                pMsg += HCI_EVT_HDR_LEN;
                BSTREAM_TO_UINT16(opcode, pMsg);
                tr_debug("Vendor specific event, opcode=0x%4X", opcode);
            } else {
                tr_info("Unknown event %d!", pMsg[0]);
            }
        }
    }

private:
    uint8_t bd_addr[6];

    void aciReadConfigParameter(uint8_t offset)
    {
        uint8_t *pBuf = hciCmdAlloc(ACI_READ_CONFIG_DATA_OPCODE, 1);
        if (!pBuf) {
            return;
        }

        pBuf[3] = offset;
        hciCmdSend(pBuf);
    }

    template<size_t N>
    void aciWriteConfigData(uint8_t offset, uint8_t (&buf)[N])
    {
        uint8_t *pBuf = hciCmdAlloc(ACI_WRITE_CONFIG_DATA_OPCODE, 2 + N);
        if (!pBuf) {
            return;
        }

        pBuf[3] = offset;
        pBuf[4] = N;
        memcpy(pBuf + 5, buf, N);
        hciCmdSend(pBuf);
    }

    void hciCoreReadResolvingListSize(void)
    {
        /* if LL Privacy is supported by Controller and included */
        if ((hciCoreCb.leSupFeat & HCI_LE_SUP_FEAT_PRIVACY) &&
                (hciLeSupFeatCfg & HCI_LE_SUP_FEAT_PRIVACY)) {
            /* send next command in sequence */
            HciLeReadResolvingListSize();
        } else {
            hciCoreCb.resListSize = 0;

            /* send next command in sequence */
            hciCoreReadMaxDataLen();
        }
    }

    void hciCoreReadMaxDataLen(void)
    {
        /* if LE Data Packet Length Extensions is supported by Controller and included */
        if ((hciCoreCb.leSupFeat & HCI_LE_SUP_FEAT_DATA_LEN_EXT) &&
                (hciLeSupFeatCfg & HCI_LE_SUP_FEAT_DATA_LEN_EXT)) {
            /* send next command in sequence */
            HciLeReadMaxDataLen();
        } else {
            /* send next command in sequence */
            HciLeRandCmd();
        }
    }
};

ble::buf_pool_desc_t ble::vendor::stm32wb::HCIDriver::get_buffer_pool_description()
{
    // Use default buffer pool
    return ble::CordioHCIDriver::get_default_buffer_pool_description();
}



class TransportDriver : public CordioHCITransportDriver {
public:
    TransportDriver(TL_CmdPacket_t *BleCmdBuffer, TL_CmdPacket_t *SystemCmdBuffer, uint8_t *EvtPool, uint8_t *SystemSpareEvtBuffer, uint8_t *BleSpareEvtBuffer, uint8_t *HciAclDataBuffer)
    {

        bleCmdBuf = BleCmdBuffer;
        sysCmdBuf = SystemCmdBuffer;
        evtPool = EvtPool;
        sysSpareEvtBuf = SystemSpareEvtBuffer;
        bleSpareEvtBuf = BleSpareEvtBuffer;
        aclDataBuffer = HciAclDataBuffer;
    }

    virtual ~TransportDriver() { }

    /**
     * @see CordioHCITransportDriver::initialize
     */
    virtual void initialize()
    {
        /*  Check whether M0 sub-system was started already by
         *  checking if the system event was already received
         *  before. If it was not, then go thru all init. */
        if (!sysevt_check()) {
            init_debug();
            stm32wb_reset();
            transport_init();

            WirelessFwInfo_t wireless_info_instance;
            WirelessFwInfo_t *p_wireless_info = &wireless_info_instance;
            if (SHCI_GetWirelessFwInfo(p_wireless_info) != SHCI_Success) {
                tr_error("SHCI_GetWirelessFwInfo error");
            } else {
                // https://github.com/STMicroelectronics/STM32CubeWB/tree/master/Projects/STM32WB_Copro_Wireless_Binaries
                // Be sure that you are using the latest BLE FW version
                tr_info("WIRELESS COPROCESSOR FW VERSION ID = %d.%d.%d", p_wireless_info->VersionMajor, p_wireless_info->VersionMinor, p_wireless_info->VersionSub);
                tr_info("WIRELESS COPROCESSOR FW STACK TYPE = %d (ROM size 0x%x)", p_wireless_info->StackType, MBED_ROM_SIZE);

#if STM32WB15xx
                switch (p_wireless_info->StackType) {
                    case INFO_STACK_TYPE_BLE_FULL:
                        error("Wrong BLE FW\n");
                        break;
                    case INFO_STACK_TYPE_BLE_HCI:
                        if (MBED_ROM_SIZE > 0x32800)  {
                            error("Wrong MBED_ROM_SIZE with HCI FW\n");
                        }
                        break;
                    default:
                        tr_error("StackType %u not expected\n", p_wireless_info->StackType);
                }
#endif
#if STM32WB55xx
                switch (p_wireless_info->StackType) {
                    case INFO_STACK_TYPE_BLE_FULL:
                        if (MBED_ROM_SIZE > 0xCA000)  {
                            error("Wrong MBED_ROM_SIZE with BLE FW\n");
                        }
                        break;
                    case INFO_STACK_TYPE_BLE_HCI:
                        if (MBED_ROM_SIZE > 0xE0000)  {
                            error("Wrong MBED_ROM_SIZE with HCI FW\n");
                        }
                        break;
                    default:
                        tr_error("StackType %u not expected\n", p_wireless_info->StackType);
                }
#endif
            }
        }
    }

    /**
     * @see CordioHCITransportDriver::terminate
     */
    virtual void terminate() { }

    /**
     * @see CordioHCITransportDriver::write
     */
    virtual uint16_t write(uint8_t type, uint16_t len, uint8_t *pData)
    {
        return mbox_write(type, len, pData);
    }

private:
    void transport_init(void)
    {
        TL_MM_Config_t tl_mm_config;
        TL_BLE_InitConf_t tl_ble_Config;
        /* STM32WB offers a System Channel HCI interface for
           offering system services, with proprietary commands.
           System Channel must be used as well for starting up
           BLE service so we need to initialize it. */
        SHCI_TL_HciInitConf_t shci_init_config;

        /**< Reference table initialization */
        TL_Init();

        /**< System channel initialization */
        shci_init_config.p_cmdbuffer = (uint8_t *)sysCmdBuf;
        shci_init_config.StatusNotCallBack = syscmd_status_not;
        shci_init(sysevt_received, (void *) &shci_init_config);

        /**< Memory Manager channel initialization */
        tl_mm_config.p_BleSpareEvtBuffer = bleSpareEvtBuf;
        tl_mm_config.p_SystemSpareEvtBuffer = sysSpareEvtBuf;
        tl_mm_config.p_AsynchEvtPool = evtPool;
        tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
        TL_MM_Init(&tl_mm_config);

        TL_Enable();

        /*  At this stage, we'll need to wait for ready event,
         *  passed thru TL_SYS_EvtReceived */
        if (!sysevt_wait()) {
            error("ERROR booting WB controler\n");
            return;
        }

        // TO DO : check if we need to disable LPM
        // requires to import as well all lpm driver
        tl_ble_Config.p_AclDataBuffer = aclDataBuffer;
        tl_ble_Config.IoBusAclDataTxAck = acl_data_ack;
        tl_ble_Config.p_cmdbuffer = (uint8_t *)bleCmdBuf;
        tl_ble_Config.IoBusEvtCallBack = evt_received;
        TL_BLE_Init(&tl_ble_Config);


        /* Now start BLE service on firmware side, using Vendor specific
         * command on the System Channe
         */
        stm32wb_start_ble();
    }

    uint16_t mbox_write(uint8_t type, uint16_t len, uint8_t *pData)
    {
        // Note: Until enum is avalable
        // type 01 Command
        // type 02 ACL DATA
        // type 03 SCO Voice (not supported)
        // type 04 event - uplink (not suported)
        tr_debug("mbox_write type:%d, len:%d", type, len);
        /*  TO DO : MANAGE ACL DATA CASE in separate buffer */
        switch (type) {
            case 1://BLE command
                bleCmdBuf->cmdserial.type = type; // for now this param is overwritten in TL_BLE_SendCmd
                memcpy((void *) &bleCmdBuf->cmdserial.cmd, pData, len);
                /* We're tracing here the command, after copy in shared mem but before
                 * * M0 trigger. */
                tr_info("TX>> BLE CMD");
                /* Trace the buffer including Type (+1 on lngth) */
                tr_debug("  Type %#x", bleCmdBuf->cmdserial.type);
                tr_debug("   Cmd %#x", bleCmdBuf->cmdserial.cmd.cmdcode);
                tr_debug("   Len %#x", bleCmdBuf->cmdserial.cmd.plen);
                TL_BLE_SendCmd(NULL, 0); // unused parameters for now
                break;
            case 2://ACL DATA
                if (!acl_data_wait()) {
                    tr_info("ERROR: previous ACL message not ACK'd");
                    /*  return number of bytes sent, 0 in this error case */
                    return 0;
                }
                TL_AclDataSerial_t *aclDataSerial = (TL_AclDataSerial_t *)(aclDataBuffer + sizeof(TL_PacketHeader_t));
                aclDataSerial->type = type; // for now this param is overwritten in TL_BLE_SendCmd
                memcpy(aclDataBuffer + + sizeof(TL_PacketHeader_t) + sizeof(type), pData, len);
                TL_BLE_SendAclData(NULL, 0); // unused parameters for now
                tr_info("TX>> BLE ACL");
                break;
        }
        return len;
    }

    void stm32wb_reset(void)
    {
        // Reset IPCC
        LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

        LL_C1_IPCC_ClearFlag_CHx(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        LL_C2_IPCC_ClearFlag_CHx(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        LL_C1_IPCC_DisableTransmitChannel(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        LL_C2_IPCC_DisableTransmitChannel(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        LL_C1_IPCC_DisableReceiveChannel(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        LL_C2_IPCC_DisableReceiveChannel(
            IPCC,
            LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
            | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

        /* Set IPCC default IRQ handlers */
        NVIC_SetVector(IPCC_C1_TX_IRQn, (uint32_t)HW_IPCC_Tx_Handler);
        NVIC_SetVector(IPCC_C1_RX_IRQn, (uint32_t)HW_IPCC_Rx_Handler);

        return;
    } // stm32wb_reset

    void stm32wb_start_ble(void)
    {
        SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet = {
            0, 0, 0,                            /**< Header unused */
            0,                                  /** pBleBufferAddress not used */
            0,                                  /** BleBufferSize not used */
            CFG_BLE_NUM_GATT_ATTRIBUTES,
            CFG_BLE_NUM_GATT_SERVICES,
            CFG_BLE_ATT_VALUE_ARRAY_SIZE,
            CFG_BLE_NUM_LINK,
            CFG_BLE_DATA_LENGTH_EXTENSION,
            CFG_BLE_PREPARE_WRITE_LIST_SIZE,
            CFG_BLE_MBLOCK_COUNT,
            CFG_BLE_MAX_ATT_MTU,
            CFG_BLE_SLAVE_SCA,
            CFG_BLE_MASTER_SCA,
            CFG_BLE_LSE_SOURCE,
            CFG_BLE_MAX_CONN_EVENT_LENGTH,
            CFG_BLE_HSE_STARTUP_TIME,
            CFG_BLE_VITERBI_MODE,
            CFG_BLE_LL_ONLY,
            0                                   /** TODO Should be read from HW */
        };
        /**
         * Starts the BLE Stack on CPU2
         */
        SHCI_C2_BLE_Init(&ble_init_cmd_packet);

#if DEVICE_FLASH
        /* Used in flash_api.c */
        BLE_inited = 1;
#endif
    }

    TL_CmdPacket_t *bleCmdBuf;
    TL_CmdPacket_t *sysCmdBuf;
    uint8_t *evtPool;
    uint8_t *sysSpareEvtBuf;
    uint8_t *aclDataBuffer;
    uint8_t *bleSpareEvtBuf;
}; // class TransportDriver

} // namespace stm32wb
} // namespace vendor
} // namespace ble

/*  There must be only 1 instance of the Transport Driver in STM32WB
 *  and the command buffers needs to be located in correct memory areas
 */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;

PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t HciAclDataBuffer[MAX_HCI_ACL_PACKET_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t     SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t     BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];


/**
 * Cordio HCI driver factory
 */
ble::CordioHCIDriver &ble_cordio_get_hci_driver()
{
    static ble::vendor::stm32wb::TransportDriver transport_driver(
        &BleCmdBuffer,
        &SystemCmdBuffer,
        EvtPool,
        SystemSpareEvtBuffer,
        BleSpareEvtBuffer,
        HciAclDataBuffer
    );

    static ble::vendor::stm32wb::HCIDriver hci_driver(
        transport_driver /* other hci driver parameters */
    );

    return hci_driver;
}

static void evt_received(TL_EvtPacket_t *hcievt)
{
    uint16_t len = 0;

    // We need to memcpy the data before passing to higher layers.
    switch (hcievt->evtserial.type) {
        case TL_BLEEVT_PKT_TYPE:
            len = hcievt->evtserial.evt.plen + TL_EVT_HDR_SIZE;
            ble::vendor::stm32wb::TransportDriver::on_data_received((uint8_t *)&hcievt->evtserial, len);
            break;
        case TL_ACL_DATA_PKT_TYPE: {
            TL_AclDataSerial_t *acl = &(((TL_AclDataPacket_t *)hcievt)->AclDataSerial);
            len = acl->length + 5;
            ble::vendor::stm32wb::TransportDriver::on_data_received((uint8_t *)acl, len);
        }
        break;
        default:
            // should not happen - let's block to check
            tr_error("BLE TL evt_received, wrong type:%d", hcievt->evtserial.type);
            break;
    }

    /*  In case Event belongs to the Evt Pool we need to inform  */
    if (((uint8_t *)hcievt >= EvtPool) && ((uint8_t *)hcievt < (EvtPool + POOL_SIZE))) {
        /*  Free the message from shared memory */
        TL_MM_EvtDone(hcievt);
    }
}

/**
 * TL Mailbox synchronisation means
 */

/* Using Semaphore to implemented blocking cmd/resp on system channel */
static rtos::Semaphore sys_event_sem(0, 1);
static rtos::Semaphore sys_resp_sem(0, 1);
static rtos::Semaphore acl_ack_sem(1, 1);

static void acl_data_ack(void)
{
    /**
     * The current implementation assumes the tackGUI will not send a new HCI ACL DATA packet before this ack is received
     * ( which means the CPU2 has handled the previous packet )
     * In order to implement a secure mechanism, it is required either
     * - a flow control with the stack
     * - a local pool of buffer to store packets received from the stack
     */
    acl_ack_sem.release();
    return;
}

static bool acl_data_wait(void)
{

    /* Wait 10 sec for previous ACL command to be ack'ed by Low Layers
     * before sending the next one */
    if (!acl_ack_sem.try_acquire_for(10000)) {
        return false;
    } else {
        return true;
    }
}

/*  WEAK callbacks from the BLE TL driver - will be called under Interrupt */
static void sysevt_received(void *pdata)
{
    /* For now only READY event is received, so we know this is it */
    sys_event_sem.release();
    /* But later on ... we'll have to parse the answer */
    return;
}

/*  returns true if ssyevt was received, false otherwise */
static bool sysevt_wait(void)
{
    /*  Wait for 10sec max - if not return an error */
    if (!sys_event_sem.try_acquire_for(10000)) {
        return false;
    } else {
        /*  release immmediately, now that M0 runs */
        sys_event_sem.release();
        return true;
    }
}

/*  returns true if ssyevt was already received, which means M0 core is
 *  already up and running */
static bool sysevt_check(void)
{
    /*  Check if system is UP and runing already */
    if (!sys_event_sem.try_acquire_for(10)) {
        return false;
    } else {
        /*  release immmediately as M0 already runs */
        sys_event_sem.release();
        return true;
    }
}

static void syscmd_status_not(SHCI_TL_CmdStatus_t status)
{
    tr_debug("syscmd_status_not, status:%d", status);
    return;
}

void shci_notify_asynch_evt(void *pdata)
{
    /* Need to parse data in future version */
    shci_user_evt_proc();
    return;
}

void shci_cmd_resp_release(uint32_t flag)
{
    sys_resp_sem.release();
    return;
}

void shci_cmd_resp_wait(uint32_t timeout)
{
    /* TO DO: manage timeouts if we can return an error */
    if (!sys_resp_sem.try_acquire_for(timeout)) {
        tr_error("shci_cmd_resp_wait timed out");
    }
}

void shci_register_io_bus(tSHciIO *fops)
{
    /* Register IO bus services */
    fops->Init    = TL_SYS_Init;
    fops->Send    = TL_SYS_SendCmd;
}

/**
 * Few utilities functions
 */
static void init_debug(void)
{
    /* In case of MBED debug profile, configure debugger support */
#if (defined(MBED_DEBUG) || (CFG_DEBUGGER_SUPPORTED == 1))
    tr_info("init_debug ENABLED");
    /**
     * Keep debugger enabled while in any low power mode
     */
    HAL_DBGMCU_EnableDBGSleepMode();
    HAL_DBGMCU_EnableDBGStopMode();
    HAL_DBGMCU_EnableDBGStandbyMode();

    /***************** ENABLE DEBUGGER *************************************/
    LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_48);
    LL_C2_EXTI_EnableIT_32_63(LL_EXTI_LINE_48);

#else
    tr_info("init_debug DISABLED");

    GPIO_InitTypeDef gpio_config = {0};

    gpio_config.Pull = GPIO_NOPULL;
    gpio_config.Mode = GPIO_MODE_ANALOG;

    gpio_config.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_Init(GPIOA, &gpio_config);

    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_3;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_Init(GPIOB, &gpio_config);

    HAL_DBGMCU_DisableDBGSleepMode();
    HAL_DBGMCU_DisableDBGStopMode();
    HAL_DBGMCU_DisableDBGStandbyMode();

#endif /* (CFG_DEBUGGER_SUPPORTED == 1) */

    return;
}

/* This function fills in a BD address table */
bool get_bd_address(uint8_t *bd_addr)
{
    uint8_t *otp_addr;
    uint32_t udn;
    uint32_t company_id;
    uint32_t device_id;
    bool bd_found;

    udn = LL_FLASH_GetUDN();

    if (udn != 0xFFFFFFFF) {
        tr_info("Found Unique Device Number: %#06x", udn);

        company_id = LL_FLASH_GetSTCompanyID();
        device_id = LL_FLASH_GetDeviceID();

        bd_addr[0] = (uint8_t)(udn & 0x000000FF);
        bd_addr[1] = (uint8_t)((udn & 0x0000FF00) >> 8);
        bd_addr[2] = (uint8_t)((udn & 0x00FF0000) >> 16);
        bd_addr[3] = (uint8_t)device_id;
        bd_addr[4] = (uint8_t)(company_id & 0x000000FF);
        bd_addr[5] = (uint8_t)((company_id & 0x0000FF00) >> 8);

        bd_found = true;
    } else {
        otp_addr = OTP_Read(0);
        if (otp_addr) {
            memcpy(bd_addr, ((OTP_ID0_t *)otp_addr)->bd_address, CONFIG_DATA_PUBADDR_LEN);
            bd_found = false;
        } else {
            tr_debug("Cannot find Bluetooth Device ADDRESS to program - will leave hw default");
            bd_found = true;
        }
    }

    return bd_found;
}

