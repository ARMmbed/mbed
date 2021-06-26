/*
 * Copyright (c) 2017-2020 ARM Limited
 * Copyright (c) 2017-2020 STMicroelectronics
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

// drivers
#include "drivers/DigitalOut.h"
#include "drivers/SPI.h"
#include "drivers/InterruptIn.h"
#include "drivers/Timer.h"

// platform
#include "platform/mbed_wait_api.h"

// FEATURE_BLE/targets/TARGET_CORDIO
#include "CordioHCIDriver.h"
#include "CordioHCITransportDriver.h"
#include "hci_api.h"
#include "hci_cmd.h"
#include "hci_core.h"
#include "dm_api.h"
#include "bstream.h"
#include "hci_mbed_os_adaptation.h"

// rtos
#include "rtos/Thread.h"
#include "rtos/Semaphore.h"
#include "rtos/Mutex.h"

#include "mbed-trace/mbed_trace.h"
#include "common/ble_trace_helpers.h"

#define TRACE_GROUP "BLHC"

#define HCI_RESET_RAND_CNT              4

#define VENDOR_SPECIFIC_EVENT           0xFF
#define EVT_BLUENRG_2_INITIALIZED       0x0001
#define ACI_READ_CONFIG_DATA_OPCODE     0xFC0D
#define ACI_WRITE_CONFIG_DATA_OPCODE    0xFC0C
#define ACI_GATT_INIT_OPCODE            0xFD01
#define ACI_GAP_INIT_OPCODE             0xFC8A

#define PUBLIC_ADDRESS_OFFSET           0x00
#define RANDOM_STATIC_ADDRESS_OFFSET    0x80
#define LL_WITHOUT_HOST_OFFSET          0x2C

#ifdef NDEBUG
#define SPI_STACK_SIZE                  1024
#else
#define SPI_STACK_SIZE                  2048
#endif

#define IRQ_TIMEOUT_DURATION            100 //ms

namespace ble {
namespace vendor {
namespace bluenrg_2 {

/**
 * BlueNRG_2 HCI driver implementation.
 * @see CordioHCIDriver
 */
class HCIDriver : public CordioHCIDriver {
public:
    /**
     * Construction of the BlueNRG_2 HCIDriver.
     * @param transport: Transport of the HCI commands.
     * @param rst: Name of the reset pin
     */
    HCIDriver(CordioHCITransportDriver &transport_driver, PinName rst) :
        CordioHCIDriver(transport_driver), rst(rst) { }

    /**
     * @see CordioHCIDriver::do_initialize
     */
    virtual void do_initialize()
    {
        bluenrg_2_reset();
    }

    /**
     * @see CordioHCIDriver::get_buffer_pool_description
     */
    ble::buf_pool_desc_t get_buffer_pool_description()
    {
        // Use default buffer pool
        return ble::CordioHCIDriver::get_default_buffer_pool_description();
    }

    /**
     * @see CordioHCIDriver::start_reset_sequence
     */
    virtual void start_reset_sequence()
    {
        reset_received = false;
        bluenrg_2_initialized = false;
        enable_link_layer_mode_ongoing = false;
        /* send an HCI Reset command to start the sequence */
        HciResetCmd();
    }

    /**
     * @see CordioHCIDriver::do_terminate
     */
    virtual void do_terminate()
    {

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
            /* parse parameters */
            pMsg += HCI_EVT_HDR_LEN;
            pMsg++;                   /* skip num packets */
            BSTREAM_TO_UINT16(opcode, pMsg);
            pMsg++;                   /* skip status */

            /* decode opcode */
            switch (opcode) {
                case HCI_OPCODE_RESET: {
                    /* initialize rand command count */
                    randCnt = 0;
                    reset_received = true;
                    // bluenrg_2_initialized event has to come after the hci reset event
                    bluenrg_2_initialized = false;
                }
                break;

                // ACL packet
                case ACI_WRITE_CONFIG_DATA_OPCODE:
                    if (enable_link_layer_mode_ongoing) {
                        enable_link_layer_mode_ongoing = false;
                        aciGattInit();
                    }
                    break;

                case ACI_GATT_INIT_OPCODE:
                    aciGapInit();
                    break;

                case ACI_GAP_INIT_OPCODE:
                    aciReadConfigParameter(RANDOM_STATIC_ADDRESS_OFFSET);
                    break;

                case ACI_READ_CONFIG_DATA_OPCODE:
                    // note: will send the HCI command to send the random address
                    pMsg++; /* skip address length */
                    set_random_static_address(pMsg);
                    break;

                case HCI_OPCODE_LE_SET_RAND_ADDR:
                    HciSetEventMaskCmd((uint8_t *) hciEventMask);
                    break;

                case HCI_OPCODE_SET_EVENT_MASK:
                    /* send next command in sequence */
                    HciLeSetEventMaskCmd((uint8_t *) hciLeEventMask);
                    break;

                case HCI_OPCODE_LE_SET_EVENT_MASK:
// Note: the public address is not read because there is no valid public address provisioned by default on the target
// You can enable it thanks to json "valid-public-bd-address" config value
#if MBED_CONF_BLUENRG_2_VALID_PUBLIC_BD_ADDRESS == 1
                    /* send next command in sequence */
                    HciReadBdAddrCmd();
                    break;

                case HCI_OPCODE_READ_BD_ADDR:
                    /* parse and store event parameters */
                    BdaCpy(hciCoreCb.bdAddr, pMsg);

                    /* send next command in sequence */
#endif
                    HciLeReadBufSizeCmd();
                    break;

                case HCI_OPCODE_LE_READ_BUF_SIZE:
                    /* parse and store event parameters */
                    BSTREAM_TO_UINT16(hciCoreCb.bufSize, pMsg);
                    BSTREAM_TO_UINT8(hciCoreCb.numBufs, pMsg);

                    /* initialize ACL buffer accounting */
                    hciCoreCb.availBufs = hciCoreCb.numBufs;

                    /* send next command in sequence */
                    HciLeReadSupStatesCmd();
                    break;

                case HCI_OPCODE_LE_READ_SUP_STATES:
                    /* parse and store event parameters */
                    memcpy(hciCoreCb.leStates, pMsg, HCI_LE_STATES_LEN);

                    /* send next command in sequence */
                    HciLeReadWhiteListSizeCmd();
                    break;

                case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
                    /* parse and store event parameters */
                    BSTREAM_TO_UINT8(hciCoreCb.whiteListSize, pMsg);

                    /* send next command in sequence */
                    HciLeReadLocalSupFeatCmd();
                    break;

                case HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT:
                    /* parse and store event parameters */
                    BSTREAM_TO_UINT16(hciCoreCb.leSupFeat, pMsg);

                    /* send next command in sequence */
                    hciCoreReadResolvingListSize();
                    break;

                case HCI_OPCODE_LE_READ_RES_LIST_SIZE:
                    /* parse and store event parameters */
                    BSTREAM_TO_UINT8(hciCoreCb.resListSize, pMsg);

                    /* send next command in sequence */
                    hciCoreReadMaxDataLen();
                    break;

                case HCI_OPCODE_LE_READ_MAX_DATA_LEN: {
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
                        /* send first extended command */
                        (*hciCoreCb.extResetSeq)(pMsg, opcode);
                    } else {
                        /* initialize extended parameters */
                        hciCoreCb.maxAdvDataLen = 0;
                        hciCoreCb.numSupAdvSets = 0;
                        hciCoreCb.perAdvListSize = 0;

                        /* send next command in sequence */
                        HciLeRandCmd();
                    }
                    break;

                case HCI_OPCODE_LE_READ_MAX_ADV_DATA_LEN:
                case HCI_OPCODE_LE_READ_NUM_SUP_ADV_SETS:
                case HCI_OPCODE_LE_READ_PER_ADV_LIST_SIZE:
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
                        signal_reset_sequence_done();
                    }
                    break;

                default:
                    break;
            }
        } else {
            /**
             * vendor specific event
             */
            if (pMsg[0] == VENDOR_SPECIFIC_EVENT) {
                /* parse parameters */
                pMsg += HCI_EVT_HDR_LEN;
                BSTREAM_TO_UINT16(opcode, pMsg);

                if (opcode == EVT_BLUENRG_2_INITIALIZED) {
                    if (bluenrg_2_initialized) {
                        return;
                    }
                    bluenrg_2_initialized = true;
                    if (reset_received) {
                        aciEnableLinkLayerModeOnly();
                    }
                }

            }
        }
    }

private:
    void aciEnableLinkLayerModeOnly()
    {
        uint8_t data[1] = { 0x01 };
        enable_link_layer_mode_ongoing = true;
        aciWriteConfigData(LL_WITHOUT_HOST_OFFSET, data);
    }

    void aciGattInit()
    {
        uint8_t *pBuf = hciCmdAlloc(ACI_GATT_INIT_OPCODE, 0);
        if (!pBuf) {
            return;
        }
        hciCmdSend(pBuf);
    }

    void aciGapInit()
    {
        uint8_t *pBuf = hciCmdAlloc(ACI_GAP_INIT_OPCODE, 3);
        if (!pBuf) {
            return;
        }
        pBuf[3] = 0xF;
        pBuf[4] = 0;
        pBuf[5] = 0;
        hciCmdSend(pBuf);
    }

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

    void bluenrg_2_reset()
    {
        /* Reset BlueNRG_2 SPI interface. Hold reset line to 0 for 1500us */
        rst = 0;
        wait_us(1500);
        rst = 1;

        /* Wait for the radio to come back up */
        wait_us(100000);
    }

    mbed::DigitalOut rst;
    bool reset_received;
    bool bluenrg_2_initialized;
    bool enable_link_layer_mode_ongoing;
};

/**
 * Transport driver of the ST BlueNRG_2 shield.
 * @important: With that driver, it is assumed that the SPI bus used is not shared
 * with other SPI peripherals. The reasons behind this choice are simplicity and
 * performance:
 *   - Reading from the peripheral SPI can be challenging especially if other
 *     threads access the same SPI bus. Indeed it is common that the function
 *     spiRead yield nothings even if the chip has signaled data with the irq
 *     line. Sharing would make the situation worse and increase the risk of
 *     timeout of HCI commands / response.
 *   - This driver can be used even if the RTOS is disabled or not present it may
 *     may be usefull for some targets.
 *
 * If The SPI is shared with other peripherals then the best option would be to
 * handle SPI read in a real time thread woken up by an event flag.
 *
 * Other mechanisms might also be added in the future to handle data read as an
 * event from the stack. This might not be the best solution for all BLE chip;
 * especially this one.
 */
class TransportDriver : public CordioHCITransportDriver {
public:
    /**
     * Construct the transport driver required by a BlueNRG_2 module.
     * @param mosi Pin of the SPI mosi
     * @param miso Pin of the SPI miso
     * @param sclk Pin of the SPI clock
     * @param irq Pin used by the module to signal data are available.
     */
    TransportDriver(PinName mosi, PinName miso, PinName sclk, PinName ncs, PinName irq)
        : spi(mosi, miso, sclk), nCS(ncs), irq(irq), _spi_thread(osPriorityNormal, SPI_STACK_SIZE, _spi_thread_stack)
    {
        _spi_thread.start(mbed::callback(this, &TransportDriver::spi_read_cb));
    }

    virtual ~TransportDriver() { }

    /**
     * @see CordioHCITransportDriver::initialize
     */
    virtual void initialize()
    {
        // Setup the spi for 8 bit data, low clock polarity,
        // 2-edge phase, with an 1MHz clock rate
        spi.format(8, 1);
        spi.frequency(1000000);

        // Deselect the BlueNRG_2 chip by keeping its nCS signal high
        nCS = 1;

        wait_us(500);

        // Set the interrupt handler for the device
        irq.mode(PullDown); // set irq mode
        irq.rise(mbed::callback(this, &TransportDriver::HCI_Isr));
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
        // repeat write until successfull. A number of attempt or timeout might
        // be useful
        while (spiWrite(type, pData, len) == 0) { }
        return len;
    }

private:
    uint16_t spiWrite(uint8_t type, const uint8_t *data, uint16_t data_length)
    {
        static const uint8_t header_master[] = { 0x0a, 0x00, 0x00, 0x00, 0x00 };
        uint8_t header_slave[5];
        uint16_t data_written = 0;
        uint16_t write_buffer_size = 0;

        _spi_mutex.lock();

        irq.disable_irq();

        /* CS reset */
        nCS = 0;

        // Wait until BlueNRG_2 is ready.
        // When ready it will raise the IRQ pin.
        _irq_timer.start();
        while (!dataPresent()) {
            auto us = _irq_timer.elapsed_time().count();
            if (us > IRQ_TIMEOUT_DURATION * 1000) {
                goto exit;
            }
        }

        /* Exchange header */
        for (uint8_t i = 0; i < sizeof(header_master); ++i) {
            header_slave[i] = spi.write(header_master[i]);
        }

        write_buffer_size = header_slave[2] << 8 | header_slave[1];

        if (write_buffer_size == 0 || write_buffer_size < (data_length + 1)) {
            goto exit;
        }

        spi.write(type);

        data_written = data_length;
        for (uint16_t i = 0; i < data_length; ++i) {
            spi.write(data[i]);
        }

exit:
        nCS = 1;

        irq.enable_irq();

        _irq_timer.stop();

        _spi_mutex.unlock();

        return data_written;
    }

    bool dataPresent()
    {
        return (irq == 1);
    }

    uint16_t spiRead(uint8_t *data_buffer, const uint16_t buffer_size)
    {
        static const uint8_t header_master[] = {0x0b, 0x00, 0x00, 0x00, 0x00};
        uint8_t header_slave[5];
        uint16_t read_length = 0;
        uint16_t data_available = 0;

        irq.disable_irq();

        nCS = 0;

        /* Read the header */
        for (size_t i = 0; i < sizeof(header_master); i++) {
            header_slave[i] = spi.write(header_master[i]);
        }

        data_available = (header_slave[4] << 8) | header_slave[3];
        read_length = data_available > buffer_size ? buffer_size : data_available;

        for (uint16_t i = 0; i < read_length; ++i) {
            data_buffer[i] = spi.write(0x00);
        }

        nCS = 1;

        irq.enable_irq();

        return read_length;
    }

    /*
     * might be split into two parts: the IRQ signaling a real time thread and
     * the real time thread reading data from the SPI.
     */
    void HCI_Isr(void)
    {
        _spi_read_sem.release();
    }

    void spi_read_cb()
    {
        uint8_t data_buffer[256];
        while (true) {
            _spi_read_sem.acquire();

            _spi_mutex.lock();
            while (irq == 1) {
                uint16_t data_read = spiRead(data_buffer, sizeof(data_buffer));
                on_data_received(data_buffer, data_read);
            }
            _spi_mutex.unlock();
        }
    }

    /**
     * Unsafe SPI, does not lock when SPI access happens.
     */
    ::mbed::SPI spi;
    mbed::DigitalOut nCS;
    mbed::InterruptIn irq;
    rtos::Thread _spi_thread;
    uint8_t _spi_thread_stack[SPI_STACK_SIZE];
    rtos::Semaphore _spi_read_sem;
    rtos::Mutex _spi_mutex;
    mbed::Timer _irq_timer;
};

} // namespace bluenrg_2
} // namespace vendor
} // namespace ble

/**
 * Cordio HCI driver factory
 */
ble::CordioHCIDriver &ble_cordio_get_hci_driver()
{
    static ble::vendor::bluenrg_2::TransportDriver transport_driver(
        MBED_CONF_BLUENRG_2_SPI_MOSI,
        MBED_CONF_BLUENRG_2_SPI_MISO,
        MBED_CONF_BLUENRG_2_SPI_SCK,
        MBED_CONF_BLUENRG_2_SPI_NCS,
        MBED_CONF_BLUENRG_2_SPI_IRQ
    );
    static ble::vendor::bluenrg_2::HCIDriver hci_driver(
        transport_driver,
        MBED_CONF_BLUENRG_2_SPI_RESET
    );
    return hci_driver;
}
