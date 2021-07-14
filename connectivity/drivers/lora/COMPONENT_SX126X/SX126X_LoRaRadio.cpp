/**
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2015 Semtech
 ___ _____ _   ___ _  _____ ___  ___  ___ ___
/ __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
\__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
|___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
embedded.connectivity.solutions===============

Description: LoRaWAN stack layer that controls both MAC and PHY underneath

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian & Gilbert Menth

Copyright (c) 2019, Arm Limited and affiliates.

SPDX-License-Identifier: BSD-3-Clause
*/

#if DEVICE_SPI

#include <math.h>
#include "ThisThread.h"
#include "mbed_wait_api.h"
#include "Timer.h"
#include "SX126X_LoRaRadio.h"

#ifdef MBED_CONF_SX126X_LORA_DRIVER_SPI_FREQUENCY
#define SPI_FREQUENCY    MBED_CONF_SX126X_LORA_DRIVER_SPI_FREQUENCY
#else
#define SPI_FREQUENCY    16000000
#endif

using namespace mbed;
using namespace rtos;

#ifdef MBED_CONF_RTOS_PRESENT
/**
 * Signals
 */
#define SIG_INTERRUPT        0x02
#endif

/*!
 * FSK bandwidth definition
 */
typedef struct {
    uint32_t bandwidth;
    uint8_t  register_value;
} fsk_bw_t;

static const fsk_bw_t fsk_bandwidths[] = {
    { 4800, 0x1F },
    { 5800, 0x17 },
    { 7300, 0x0F },
    { 9700, 0x1E },
    { 11700, 0x16 },
    { 14600, 0x0E },
    { 19500, 0x1D },
    { 23400, 0x15 },
    { 29300, 0x0D },
    { 39000, 0x1C },
    { 46900, 0x14 },
    { 58600, 0x0C },
    { 78200, 0x1B },
    { 93800, 0x13 },
    { 117300, 0x0B },
    { 156200, 0x1A },
    { 187200, 0x12 },
    { 234300, 0x0A },
    { 312000, 0x19 },
    { 373600, 0x11 },
    { 467000, 0x09 },
    { 500000, 0x00 }, // Invalid Bandwidth
};

const uint8_t sync_word[] = {0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00};

// in ms                                 SF12    SF11    SF10    SF9    SF8    SF7
const float lora_symbol_time[3][6] = {{ 32.768, 16.384, 8.192, 4.096, 2.048, 1.024 },  // 125 KHz
    { 16.384, 8.192,  4.096, 2.048, 1.024, 0.512 },  // 250 KHz
    { 8.192,  4.096,  2.048, 1.024, 0.512, 0.256 }
}; // 500 KHz

SX126X_LoRaRadio::SX126X_LoRaRadio(PinName mosi,
                                   PinName miso,
                                   PinName sclk,
                                   PinName nss,
                                   PinName reset,
                                   PinName dio1,
                                   PinName busy,
                                   PinName freq_select,
                                   PinName device_select,
                                   PinName crystal_select,
                                   PinName ant_switch)
    : _spi(mosi, miso, sclk),
      _chip_select(nss, 1),
      _reset_ctl(reset),
      _dio1_ctl(dio1, PullNone),
      _busy(busy, PullNone),
      _freq_select(freq_select),
      _dev_select(device_select),
      _crystal_select(crystal_select, PullDown),
      _ant_switch(ant_switch, PIN_INPUT, PullUp, 0)
#ifdef MBED_CONF_RTOS_PRESENT
    , irq_thread(osPriorityRealtime, 1024, NULL, "LR-SX126X")
#endif
{
    _radio_events = NULL;
    _reset_ctl = 1;
    _image_calibrated = false;
    _force_image_calibration = false;
    _active_modem = MODEM_LORA;

#ifdef MBED_CONF_RTOS_PRESENT
    irq_thread.start(callback(this, &SX126X_LoRaRadio::rf_irq_task));
#endif
}

SX126X_LoRaRadio::~SX126X_LoRaRadio()
{

}

/**
 * Acquire lock
 */
void SX126X_LoRaRadio::lock(void)
{
    mutex.lock();
}

/**
 * Release lock
 */
void SX126X_LoRaRadio::unlock(void)
{
    mutex.unlock();
}

#ifdef MBED_CONF_RTOS_PRESENT
/**
 * Thread task handling IRQs
 */
void SX126X_LoRaRadio::rf_irq_task(void)
{
    for (;;) {
        uint32_t flags = ThisThread::flags_wait_any(0x7FFFFFFF);

        lock();
        if (flags & SIG_INTERRUPT) {
            handle_dio1_irq();
        }
        unlock();
    }
}
#endif

void SX126X_LoRaRadio::dio1_irq_isr()
{
#ifdef MBED_CONF_RTOS_PRESENT
    irq_thread.flags_set(SIG_INTERRUPT);
#else
    handle_dio1_irq();
#endif
}

uint16_t SX126X_LoRaRadio::get_irq_status(void)
{
    uint8_t status[2];

    read_opmode_command((uint8_t) RADIO_GET_IRQSTATUS, status, 2);
    return (status[0] << 8) | status[1];
}

void SX126X_LoRaRadio::clear_irq_status(uint16_t irq)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t) irq >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t) irq & 0x00FF);
    write_opmode_command((uint8_t) RADIO_CLR_IRQSTATUS, buf, 2);
}

//TODO - Better do CAD here. CAD code is already part of the driver
//       It needs to be hooked up to the stack (this API will need change
//       and the stack will need changes too)
bool SX126X_LoRaRadio::perform_carrier_sense(radio_modems_t modem,
                                             uint32_t freq,
                                             int16_t rssi_threshold,
                                             uint32_t max_carrier_sense_time)
{
    bool status = true;
    int16_t rssi = 0;

    set_modem(modem);
    set_channel(freq);
    _reception_mode = RECEPTION_MODE_OTHER;
    _rx_timeout = 0x00000000;
    receive();

    // hold on a bit, radio turn-around time
    ThisThread::sleep_for(1);

    Timer elapsed_time;
    elapsed_time.start();

    // Perform carrier sense for maxCarrierSenseTime
    while (elapsed_time.read_ms() < (int) max_carrier_sense_time) {
        rssi = get_rssi();

        if (rssi > rssi_threshold) {
            status = false;
            break;
        }
    }

    sleep();
    return status;
}

void SX126X_LoRaRadio::start_cad(void)
{
    // TODO: CAD is more advanced in SX126X. We will need API change in LoRaRadio
    //       for this to act properly
}

/**
 * TODO: The purpose of this API is unclear.
 *       Need to start an internal discussion.
 */
bool SX126X_LoRaRadio::check_rf_frequency(uint32_t frequency)
{
    // Implement check. Currently all frequencies are supported ? What band ?
    return true;
}

void SX126X_LoRaRadio::set_tx_continuous_wave(uint32_t freq, int8_t power,
                                              uint16_t time)
{
    // This is useless. We even removed the support from our MAC layer.
}

void SX126X_LoRaRadio::handle_dio1_irq()
{
    uint16_t irq_status = get_irq_status();
    clear_irq_status(IRQ_RADIO_ALL);

    if ((irq_status & IRQ_TX_DONE) == IRQ_TX_DONE) {
        if (_radio_events->tx_done) {
            _radio_events->tx_done();
        }
    }

    if ((irq_status & IRQ_RX_DONE) == IRQ_RX_DONE) {
        if ((irq_status & IRQ_CRC_ERROR) == IRQ_CRC_ERROR) {
            if (_radio_events && _radio_events->rx_error) {
                _radio_events->rx_error();
            }
        } else {
            if (_radio_events->rx_done) {
                uint8_t offset = 0;
                uint8_t payload_len = 0;
                int16_t rssi = 0;
                int8_t snr = 0;
                packet_status_t pkt_status;

                get_rx_buffer_status(&payload_len, &offset);
                read_fifo(_data_buffer, payload_len, offset);
                get_packet_status(&pkt_status);
                if (pkt_status.modem_type == MODEM_FSK) {
                    rssi = pkt_status.params.gfsk.rssi_sync;
                } else {
                    rssi = pkt_status.params.lora.rssi_pkt;
                    snr = pkt_status.params.lora.snr_pkt;
                }

                _radio_events->rx_done(_data_buffer, payload_len, rssi, snr);
            }
        }
    }

    if ((irq_status & IRQ_CAD_DONE) == IRQ_CAD_DONE) {
        if (_radio_events->cad_done) {
            _radio_events->cad_done((irq_status & IRQ_CAD_ACTIVITY_DETECTED)
                                    == IRQ_CAD_ACTIVITY_DETECTED);
        }
    }

    if ((irq_status & IRQ_RX_TX_TIMEOUT) == IRQ_RX_TX_TIMEOUT) {
        if ((_radio_events->tx_timeout) && (_operation_mode == MODE_TX)) {
            _radio_events->tx_timeout();
        } else if ((_radio_events && _radio_events->rx_timeout) && (_operation_mode == MODE_RX)) {
            _radio_events->rx_timeout();
        }
    }
}

void SX126X_LoRaRadio::set_device_ready(void)
{
    if (_operation_mode == MODE_SLEEP) {
        wakeup();
    }
}

void SX126X_LoRaRadio::calibrate_image(uint32_t freq)
{
    uint8_t cal_freq[2];

    if (freq > 900000000) {
        cal_freq[0] = 0xE1;
        cal_freq[1] = 0xE9;
    } else if (freq > 850000000) {
        cal_freq[0] = 0xD7;
        cal_freq[1] = 0xD8;
    } else if (freq > 770000000) {
        cal_freq[0] = 0xC1;
        cal_freq[1] = 0xC5;
    } else if (freq > 460000000) {
        cal_freq[0] = 0x75;
        cal_freq[1] = 0x81;
    } else if (freq > 425000000) {
        cal_freq[0] = 0x6B;
        cal_freq[1] = 0x6F;
    }

    write_opmode_command((uint8_t) RADIO_CALIBRATEIMAGE, cal_freq, 2);

    _image_calibrated = true;
}

void SX126X_LoRaRadio::set_channel(uint32_t frequency)
{
#if MBED_CONF_SX126X_LORA_DRIVER_SLEEP_MODE == 1
    // At this point, we are not sure what is the Modem type, set both
    _mod_params.params.lora.operational_frequency = frequency;
    _mod_params.params.gfsk.operational_frequency = frequency;
#endif

    uint8_t buf[4];
    uint32_t freq = 0;

    if (_force_image_calibration || !_image_calibrated) {
        calibrate_image(frequency);
        _image_calibrated = true;
    }

    freq = (uint32_t) ceil(((float) frequency / (float) FREQ_STEP));
    buf[0] = (uint8_t)((freq >> 24) & 0xFF);
    buf[1] = (uint8_t)((freq >> 16) & 0xFF);
    buf[2] = (uint8_t)((freq >> 8) & 0xFF);
    buf[3] = (uint8_t)(freq & 0xFF);

    write_opmode_command((uint8_t) RADIO_SET_RFFREQUENCY, buf, 4);
}

/**
 * Put radio in Standby mode
 */
void SX126X_LoRaRadio::standby(void)
{
    if (_operation_mode == MODE_STDBY_RC) {
        return;
    }

    set_device_ready();
    uint8_t standby_mode = MBED_CONF_SX126X_LORA_DRIVER_STANDBY_MODE;
    write_opmode_command((uint8_t) RADIO_SET_STANDBY, &standby_mode, 1);

    if (standby_mode == STDBY_RC) {
        _operation_mode = MODE_STDBY_RC;
    } else {
        _operation_mode = MODE_STDBY_XOSC;
    }
}

void SX126X_LoRaRadio::set_dio2_as_rfswitch_ctrl(uint8_t enable)
{
    write_opmode_command(RADIO_SET_RFSWITCHMODE, &enable, 1);
}

void SX126X_LoRaRadio::set_dio3_as_tcxo_ctrl(radio_TCXO_ctrl_voltage_t voltage,
                                             uint32_t timeout)
{
    uint8_t buf[4];

    buf[0] = voltage & 0x07;
    buf[1] = (uint8_t)((timeout >> 16) & 0xFF);
    buf[2] = (uint8_t)((timeout >> 8) & 0xFF);
    buf[3] = (uint8_t)(timeout & 0xFF);

    write_opmode_command(RADIO_SET_TCXOMODE, buf, 4);
}

void SX126X_LoRaRadio::init_radio(radio_events_t *events)
{
    _radio_events = events;

    // attach DIO1 interrupt line to its respective ISR
    _dio1_ctl.rise(callback(this, &SX126X_LoRaRadio::dio1_irq_isr));

    uint8_t freq_support = get_frequency_support();

    // Hold chip-select high
    _chip_select = 1;
    _spi.format(8, 0);
    _spi.frequency(SPI_FREQUENCY);
    // 100 us wait to settle down
    wait_us(100);

    radio_reset();

#if MBED_CONF_LORA_PUBLIC_NETWORK
    _network_mode_public = true;
#else
    _network_mode_public = false;
#endif

    // this is a POR sequence
    cold_start_wakeup();
}

void SX126X_LoRaRadio::cold_start_wakeup()
{
    uint8_t regulator_mode = MBED_CONF_SX126X_LORA_DRIVER_REGULATOR_MODE;
    write_opmode_command(RADIO_SET_REGULATORMODE, &regulator_mode, 1);
    set_buffer_base_addr(0x00, 0x00);

#if MBED_CONF_SX126X_LORA_DRIVER_XTAL == -1
    if (_crystal_select.is_connected() && _crystal_select == 0) {
#else
    if (MBED_CONF_SX126X_LORA_DRIVER_XTAL == 0) {
#endif
        caliberation_params_t calib_param;
        set_dio3_as_tcxo_ctrl(TCXO_CTRL_1_7V, 320); //5 ms
        calib_param.value = 0x7F;
        write_opmode_command(RADIO_CALIBRATE, &calib_param.value, 1);
    }

    set_dio2_as_rfswitch_ctrl(true);

    _operation_mode = MODE_STDBY_RC;

    set_modem(_active_modem);

    if (_active_modem == MODEM_LORA) {
        set_public_network(_network_mode_public);
    }
}

void SX126X_LoRaRadio::set_public_network(bool enable)
{
    if (enable) {
        // Change LoRa modem SyncWord
        write_to_register(REG_LR_SYNCWORD, (LORA_MAC_PUBLIC_SYNCWORD >> 8) & 0xFF);
        write_to_register(REG_LR_SYNCWORD + 1, LORA_MAC_PUBLIC_SYNCWORD & 0xFF);
    } else {
        // Change LoRa modem SyncWord
        write_to_register(REG_LR_SYNCWORD, (LORA_MAC_PRIVATE_SYNCWORD >> 8) & 0xFF);
        write_to_register(REG_LR_SYNCWORD + 1, LORA_MAC_PRIVATE_SYNCWORD & 0xFF);
    }
}

uint32_t SX126X_LoRaRadio::time_on_air(radio_modems_t modem, uint8_t pkt_len)
{
    uint32_t air_time = 0;

    switch (modem) {
        case MODEM_FSK: {
            air_time = rint((8 * (_packet_params.params.gfsk.preamble_length
                                  + (_packet_params.params.gfsk.syncword_length >> 3)
                                  + ((_packet_params.params.gfsk.header_type
                                      == RADIO_PACKET_FIXED_LENGTH) ? 0.0f : 1.0f) + pkt_len
                                  + ((_packet_params.params.gfsk.crc_length == RADIO_CRC_2_BYTES) ? 2.0f : 0.0f))
                             / _mod_params.params.gfsk.bit_rate) * 1000);
        }
        break;
        case MODEM_LORA: {
            float ts = lora_symbol_time[_mod_params.params.lora.bandwidth - 4][12
                                                                               - _mod_params.params.lora.spreading_factor];
            // time of preamble
            float t_preamble = (_packet_params.params.lora.preamble_length + 4.25f) * ts;
            // Symbol length of payload and time
            float tmp = ceil((8 * pkt_len - 4 * _mod_params.params.lora.spreading_factor
                              + 28 + 16 * _packet_params.params.lora.crc_mode
                              - ((_packet_params.params.lora.header_type == LORA_PACKET_FIXED_LENGTH) ? 20 : 0))
                             / (float)(4 * (_mod_params.params.lora.spreading_factor
                                            - ((_mod_params.params.lora.low_datarate_optimization > 0) ? 2 : 0))))
                        * ((_mod_params.params.lora.coding_rate % 4) + 4);
            float n_payload = 8 + ((tmp > 0) ? tmp : 0);
            float t_payload = n_payload * ts;
            // Time on air
            float tOnAir = t_preamble + t_payload;
            // return milliseconds (as ts is in milliseconds)
            air_time = floor(tOnAir + 0.999);
        }
        break;
    }

    return air_time;
}

void SX126X_LoRaRadio::radio_reset()
{
    _reset_ctl.output();
    _reset_ctl = 0;
    // should be enough, required is 50-100 us
    ThisThread::sleep_for(2);
    _reset_ctl.input();
    // give some time for automatic image calibration
    ThisThread::sleep_for(6);
}

void SX126X_LoRaRadio::wakeup()
{
    // hold the NSS low, this should wakeup the chip.
    // now we should wait for the _busy line to go low
    if (_operation_mode == MODE_SLEEP) {
        _chip_select = 0;
        wait_us(100);
        _chip_select = 1;
        //wait_us(100);
#if MBED_CONF_SX126X_LORA_DRIVER_SLEEP_MODE == 1
        wait_us(3500);
        // whenever we wakeup from Cold sleep state, we need to perform
        // image calibration
        _force_image_calibration = true;
        cold_start_wakeup();
#endif
    }
}

void SX126X_LoRaRadio::sleep(void)
{
    // warm start, power consumption 600 nA
    uint8_t sleep_state = 0x04;
    _operation_mode = MODE_SLEEP;

#if MBED_CONF_SX126X_LORA_DRIVER_SLEEP_MODE == 1
    // cold start, power consumption 160 nA
    sleep_state = 0x00;
#endif

    write_opmode_command(RADIO_SET_SLEEP, &sleep_state, 1);
    ThisThread::sleep_for(2);
}

uint32_t SX126X_LoRaRadio::random(void)
{
    set_modem(MODEM_LORA);
    uint8_t buf[] = {0, 0, 0, 0};

    // Set radio in continuous reception
    _reception_mode = RECEPTION_MODE_OTHER;
    _rx_timeout = 0xFFFFFFFF;
    receive();
    ThisThread::sleep_for(1);
    read_register(RANDOM_NUMBER_GENERATORBASEADDR, buf, 4);
    standby();

    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

void SX126X_LoRaRadio::write_opmode_command(uint8_t cmd, uint8_t *buffer, uint16_t size)
{
    _chip_select = 0;

    while (_busy) {
        // do nothing
    }

    _spi.write(cmd);

    for (int i = 0; i < size; i++) {
        _spi.write(buffer[i]);
    }

    _chip_select = 1;
}

void SX126X_LoRaRadio::read_opmode_command(uint8_t cmd,
                                           uint8_t *buffer, uint16_t size)
{
    _chip_select = 0;

    while (_busy) {
        // do nothing
    }

    _spi.write(cmd);
    _spi.write(0);

    for (int i = 0; i < size; i++) {
        buffer[i] = _spi.write(0);
    }

    _chip_select = 1;
}

void SX126X_LoRaRadio::write_to_register(uint16_t addr, uint8_t data)
{
    write_to_register(addr, &data, 1);
}

void SX126X_LoRaRadio::write_to_register(uint16_t addr, uint8_t *data,
                                         uint8_t size)
{
    _chip_select = 0;

    _spi.write(RADIO_WRITE_REGISTER);
    _spi.write((addr & 0xFF00) >> 8);
    _spi.write(addr & 0x00FF);

    for (int i = 0; i < size; i++) {
        _spi.write(data[i]);
    }

    _chip_select = 1;
}

uint8_t SX126X_LoRaRadio::read_register(uint16_t addr)
{
    uint8_t data;
    read_register(addr, &data, 1);
    return data;

}

void SX126X_LoRaRadio::read_register(uint16_t addr, uint8_t *buffer,
                                     uint8_t size)
{
    _chip_select = 0;

    _spi.write(RADIO_READ_REGISTER);
    _spi.write((addr & 0xFF00) >> 8);
    _spi.write(addr & 0x00FF);
    _spi.write(0);

    for (int i = 0; i < size; i++) {
        buffer[i] = _spi.write(0);
    }

    _chip_select = 1;
}

void SX126X_LoRaRadio::write_fifo(uint8_t *buffer, uint8_t size)
{
    _chip_select = 0;

    _spi.write(RADIO_WRITE_BUFFER);
    _spi.write(0);

    for (int i = 0; i < size; i++) {
        _spi.write(buffer[i]);
    }

    _chip_select = 1;
}

void SX126X_LoRaRadio::set_modem(uint8_t modem)
{
    _active_modem = modem;

    // setting modem type must happen in stnadby mode
    if (_operation_mode != MODE_STDBY_RC) {
        standby();
    }

    write_opmode_command(RADIO_SET_PACKETTYPE, &_active_modem, 1);
}

uint8_t SX126X_LoRaRadio::get_modem()
{
    return _active_modem;
}

void SX126X_LoRaRadio::read_fifo(uint8_t *buffer, uint8_t size, uint8_t offset)
{
    _chip_select = 0;

    _spi.write(RADIO_READ_BUFFER);
    _spi.write(offset);
    _spi.write(0);

    for (int i = 0; i < size; i++) {
        buffer[i] = _spi.write(0);
    }

    _chip_select = 1;
}

uint8_t SX126X_LoRaRadio::get_device_variant(void)
{
#if MBED_CONF_SX126X_LORA_DRIVER_DEVICE_VARIANT == -1
    uint16_t val = 0;
    val = _dev_select.read_u16();

    if (val <= 0x2000) {
        return SX1262;
    } else if (val <= 0xA000) {
        return SX1268;
    } else {
        return SX1261;
    }
#else
    return MBED_CONF_SX126X_LORA_DRIVER_DEVICE_VARIANT;
#endif
}

uint8_t SX126X_LoRaRadio::get_frequency_support(void)
{
#if MBED_CONF_SX126X_LORA_DRIVER_FREQ_SUPPORT == -1
    uint16_t val = 0;
    val = _freq_select.read_u16();

    if (val < 100) {
        return (MATCHING_FREQ_915);
    } else if (val <= 0x3000) {
        return (MATCHING_FREQ_780);
    } else if (val <= 0x4900) {      // 0x4724
        return (MATCHING_FREQ_490);
    } else if (val <= 1) {
        return (MATCHING_FREQ_434);
    } else if (val <= 1) {
        return (MATCHING_FREQ_280);
    } else if (val <= 0xF000) {
        return (MATCHING_FREQ_169);
    } else {
        return (MATCHING_FREQ_868);
    }
#else
    return MBED_CONF_SX126X_LORA_DRIVER_FREQ_SUPPORT;
#endif
}

uint8_t SX126X_LoRaRadio::get_fsk_bw_reg_val(uint32_t bandwidth)
{
    uint8_t i;

    for (i = 0; i < (sizeof(fsk_bandwidths) / sizeof(fsk_bw_t)) - 1; i++) {
        if ((bandwidth >= fsk_bandwidths[i].bandwidth)
                && (bandwidth < fsk_bandwidths[i + 1].bandwidth)) {
            return fsk_bandwidths[i].register_value;
        }
    }
    // ERROR: Value not found
    // This should never happen
    while (1);
}

void SX126X_LoRaRadio::set_max_payload_length(radio_modems_t modem, uint8_t max)
{
    if (modem == MODEM_LORA) {
        _packet_params.params.lora.payload_length = max;
    } else {
        _packet_params.params.gfsk.payload_length = max;
    }
}

void SX126X_LoRaRadio::set_tx_config(radio_modems_t modem,
                                     int8_t power,
                                     uint32_t fdev,
                                     uint32_t bandwidth,
                                     uint32_t datarate,
                                     uint8_t coderate,
                                     uint16_t preamble_len,
                                     bool fix_len,
                                     bool crc_on,
                                     bool freq_hop_on,
                                     uint8_t hop_period,
                                     bool iq_inverted,
                                     uint32_t timeout)
{

    uint8_t modem_type = (uint8_t) modem;
    switch (modem_type) {
        case MODEM_FSK:
            _mod_params.modem_type = MODEM_FSK;
            _mod_params.params.gfsk.bit_rate = datarate;

            _mod_params.params.gfsk.modulation_shaping = MOD_SHAPING_G_BT_1;
            _mod_params.params.gfsk.bandwidth = get_fsk_bw_reg_val(bandwidth);
            _mod_params.params.gfsk.fdev = fdev;

            _packet_params.modem_type = MODEM_FSK;
            _packet_params.params.gfsk.preamble_length = (preamble_len << 3); // convert byte into bit
            _packet_params.params.gfsk.preamble_min_detect = RADIO_PREAMBLE_DETECTOR_08_BITS;
            _packet_params.params.gfsk.syncword_length = 3 << 3; // convert byte into bit
            _packet_params.params.gfsk.addr_comp = RADIO_ADDRESSCOMP_FILT_OFF;
            _packet_params.params.gfsk.header_type = (fix_len == true) ?
                                                     RADIO_PACKET_FIXED_LENGTH :
                                                     RADIO_PACKET_VARIABLE_LENGTH;

            if (crc_on) {
                _packet_params.params.gfsk.crc_length = RADIO_CRC_2_BYTES_CCIT;
            } else {
                _packet_params.params.gfsk.crc_length = RADIO_CRC_OFF;
            }
            _packet_params.params.gfsk.whitening_mode = RADIO_DC_FREEWHITENING;

            set_modem(MODEM_FSK);

            write_to_register(REG_LR_SYNCWORDBASEADDRESS, (uint8_t *) sync_word, 8);
            set_whitening_seed(0x01FF);
            break;

        case MODEM_LORA:
            _mod_params.modem_type = MODEM_LORA;
            _mod_params.params.lora.spreading_factor = (lora_spread_factors_t) datarate;
            _mod_params.params.lora.bandwidth = (lora_bandwidths_t) lora_bandwidhts[bandwidth];
            _mod_params.params.lora.coding_rate = (lora_coding_tates_t) coderate;

            if (((bandwidth == 0) && ((datarate == 11) || (datarate == 12)))
                    || ((bandwidth == 1) && (datarate == 12))) {
                _mod_params.params.lora.low_datarate_optimization = 0x01;
            } else {
                _mod_params.params.lora.low_datarate_optimization = 0x00;
            }

            _packet_params.modem_type = MODEM_LORA;

            if ((_mod_params.params.lora.spreading_factor == LORA_SF5)
                    || (_mod_params.params.lora.spreading_factor == LORA_SF6)) {
                if (preamble_len < 12) {
                    _packet_params.params.lora.preamble_length = 12;
                } else {
                    _packet_params.params.lora.preamble_length = preamble_len;
                }
            } else {
                _packet_params.params.lora.preamble_length = preamble_len;
            }

            _packet_params.params.lora.header_type = (lora_pkt_length_t) fix_len;
            _packet_params.params.lora.crc_mode = (lora_crc_mode_t) crc_on;
            _packet_params.params.lora.invert_IQ = (lora_IQ_mode_t) iq_inverted;

            set_modem(MODEM_LORA);

            break;
    }

    _tx_power = power;
    _tx_timeout = timeout;
}

void SX126X_LoRaRadio::set_rx_config(radio_modems_t modem,
                                     uint32_t bandwidth,
                                     uint32_t datarate,
                                     uint8_t coderate,
                                     uint32_t bandwidthAfc,
                                     uint16_t preamble_len,
                                     uint16_t symb_timeout,
                                     bool fix_len,
                                     uint8_t payload_len,
                                     bool crc_on,
                                     bool freq_hop_on,
                                     uint8_t hop_period,
                                     bool iq_inverted,
                                     bool rx_continuous)
{
    uint8_t max_payload_len;
    (void) freq_hop_on;
    (void) hop_period;

    if (rx_continuous) {
        _reception_mode = RECEPTION_MODE_CONTINUOUS;
        symb_timeout = 0;
    }  else {
        _reception_mode = RECEPTION_MODE_SINGLE;
    }

    if (fix_len == true) {
        max_payload_len = payload_len;
    } else {
        max_payload_len = 0xFF;
    }

    uint8_t modem_type = (uint8_t) modem;

    switch (modem_type) {
        case MODEM_FSK: {
            _mod_params.modem_type = MODEM_FSK;
            _mod_params.params.gfsk.bit_rate = datarate;
            _mod_params.params.gfsk.modulation_shaping = MOD_SHAPING_G_BT_1;
            _mod_params.params.gfsk.bandwidth = get_fsk_bw_reg_val(bandwidth);

            _packet_params.modem_type = MODEM_FSK;
            _packet_params.params.gfsk.preamble_length = (preamble_len << 3); // convert byte into bit
            _packet_params.params.gfsk.preamble_min_detect =
                RADIO_PREAMBLE_DETECTOR_08_BITS;
            _packet_params.params.gfsk.syncword_length = 3 << 3; // convert byte into bit
            _packet_params.params.gfsk.addr_comp = RADIO_ADDRESSCOMP_FILT_OFF;
            _packet_params.params.gfsk.header_type =
                (fix_len == true) ?
                RADIO_PACKET_FIXED_LENGTH :
                RADIO_PACKET_VARIABLE_LENGTH;
            _packet_params.params.gfsk.payload_length = max_payload_len;

            if (crc_on) {
                _packet_params.params.gfsk.crc_length = RADIO_CRC_2_BYTES_CCIT;
            } else {
                _packet_params.params.gfsk.crc_length = RADIO_CRC_OFF;
            }

            _packet_params.params.gfsk.whitening_mode = RADIO_DC_FREEWHITENING;

            set_modem(MODEM_FSK);

            write_to_register(REG_LR_SYNCWORDBASEADDRESS, (uint8_t *) sync_word, 8);
            set_whitening_seed(0x01FF);

            _rx_timeout = (uint32_t)(symb_timeout
                                     * ((1.0 / (float) datarate) * 8.0) * 1000);

            break;
        }

        case MODEM_LORA: {
            _rx_timeout_in_symbols = symb_timeout;
            _mod_params.modem_type = MODEM_LORA;
            _mod_params.params.lora.spreading_factor =
                (lora_spread_factors_t) datarate;
            _mod_params.params.lora.bandwidth = (lora_bandwidths_t) lora_bandwidhts[bandwidth];
            _mod_params.params.lora.coding_rate =
                (lora_coding_tates_t) coderate;

            if (((bandwidth == 0) && ((datarate == 11) || (datarate == 12)))
                    || ((bandwidth == 1) && (datarate == 12))) {
                _mod_params.params.lora.low_datarate_optimization = 0x01;
            } else {
                _mod_params.params.lora.low_datarate_optimization = 0x00;
            }

            _packet_params.modem_type = MODEM_LORA;

            if ((_mod_params.params.lora.spreading_factor == LORA_SF5)
                    || (_mod_params.params.lora.spreading_factor == LORA_SF6)) {
                if (preamble_len < 12) {
                    _packet_params.params.lora.preamble_length = 12;
                } else {
                    _packet_params.params.lora.preamble_length = preamble_len;
                }
            } else {
                _packet_params.params.lora.preamble_length = preamble_len;
            }

            _packet_params.params.lora.header_type = (lora_pkt_length_t) fix_len;
            _packet_params.params.lora.payload_length = max_payload_len;
            _packet_params.params.lora.crc_mode = (lora_crc_mode_t) crc_on;
            _packet_params.params.lora.invert_IQ = (lora_IQ_mode_t) iq_inverted;

            set_modem(MODEM_LORA);

            if (_reception_mode == RECEPTION_MODE_CONTINUOUS) {
                _rx_timeout = 0xFFFFFFFF;
            } else {
                _rx_timeout = 0x00000000;
            }

            break;
        }

        default:
            break;
    }
}

void SX126X_LoRaRadio::configure_dio_irq(uint16_t irq_mask, uint16_t dio1_mask,
                                         uint16_t dio2_mask, uint16_t dio3_mask)
{
    uint8_t buf[8];

    buf[0] = (uint8_t)((irq_mask >> 8) & 0x00FF);
    buf[1] = (uint8_t)(irq_mask & 0x00FF);
    buf[2] = (uint8_t)((dio1_mask >> 8) & 0x00FF);
    buf[3] = (uint8_t)(dio1_mask & 0x00FF);
    buf[4] = (uint8_t)((dio2_mask >> 8) & 0x00FF);
    buf[5] = (uint8_t)(dio2_mask & 0x00FF);
    buf[6] = (uint8_t)((dio3_mask >> 8) & 0x00FF);
    buf[7] = (uint8_t)(dio3_mask & 0x00FF);

    write_opmode_command((uint8_t) RADIO_CFG_DIOIRQ, buf, 8);
}

void SX126X_LoRaRadio::send(uint8_t *buffer, uint8_t size)
{
    set_tx_power(_tx_power);
    configure_dio_irq(IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                      IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                      IRQ_RADIO_NONE,
                      IRQ_RADIO_NONE);

    set_modulation_params(&_mod_params);
    set_packet_params(&_packet_params);

    write_fifo(buffer, size);
    uint8_t buf[3];

    // _tx_timeout in ms should be converted to us and then divided by
    // 15.625 us. Check data-sheet 13.1.4 SetTX() section.
    uint32_t timeout_scalled = ceil((_tx_timeout * 1000) / 15.625);

    buf[0] = (uint8_t)((timeout_scalled >> 16) & 0xFF);
    buf[1] = (uint8_t)((timeout_scalled >> 8) & 0xFF);
    buf[2] = (uint8_t)(timeout_scalled & 0xFF);

    write_opmode_command(RADIO_SET_TX, buf, 3);

    _operation_mode = MODE_TX;
}


void SX126X_LoRaRadio::receive(void)
{
    if (get_modem() == MODEM_LORA) {
        if (_reception_mode != RECEPTION_MODE_CONTINUOUS) {
            // Data-sheet Table 13-11: StopOnPreambParam
            // We will use radio's internal timer to mark no reception. This behaviour
            // is different from SX1272/SX1276 where we are relying on radio to stop
            // at preamble detection.
            // 0x00 means Timer will be stopped on SyncWord(FSK) or Header (LoRa) detection
            // 0x01 means Timer is stopped on preamble detection
            uint8_t stop_at_preamble = 0x01;
            write_opmode_command(RADIO_SET_STOPRXTIMERONPREAMBLE, &stop_at_preamble, 1);
        }

        // Data-sheet 13.4.9 SetLoRaSymbNumTimeout
        write_opmode_command(RADIO_SET_LORASYMBTIMEOUT, &_rx_timeout_in_symbols, 1);
    }

    if (_reception_mode != RECEPTION_MODE_OTHER) {
        configure_dio_irq(IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR,
                          IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR,
                          IRQ_RADIO_NONE,
                          IRQ_RADIO_NONE);
        set_modulation_params(&_mod_params);
        set_packet_params(&_packet_params);
    }

    uint8_t buf[3];

#if MBED_CONF_SX126X_LORA_DRIVER_BOOST_RX
    write_to_register(REG_RX_GAIN, 0x96);
#endif

    buf[0] = (uint8_t)((_rx_timeout >> 16) & 0xFF);
    buf[1] = (uint8_t)((_rx_timeout >> 8) & 0xFF);
    buf[2] = (uint8_t)(_rx_timeout & 0xFF);

    write_opmode_command(RADIO_SET_RX, buf, 3);

    _operation_mode = MODE_RX;
}

// check data-sheet 13.1.14.1 PA optimal settings
void SX126X_LoRaRadio::set_tx_power(int8_t power)
{
    uint8_t buf[2];

    if (get_device_variant() == SX1261) {
        if (power >= 14) {
            set_pa_config(0x04, 0x00, 0x01, 0x01);
            power = 14;
        } else if (power < 14) {
            set_pa_config(0x01, 0x00, 0x01, 0x01);
        }

        if (power < -3) {
            power = -3;
        }
        write_to_register(REG_OCP, 0x18); // current max is 80 mA for the whole device
    } else {
        // sx1262 or sx1268
        if (power > 22) {
            power = 22;
        } else if (power < -3) {
            power = -3;
        }

        if (power <= 14) {
            set_pa_config(0x02, 0x02, 0x00, 0x01);
        } else {
            set_pa_config(0x04, 0x07, 0x00, 0x01);
        }

        write_to_register(REG_OCP, 0x38); // current max 160mA for the whole device
    }

    buf[0] = power;

    if (_crystal_select.is_connected() && _crystal_select == 0) {
        // TCXO
        buf[1] = RADIO_RAMP_200_US;
    } else {
        // XTAL
        buf[1] = RADIO_RAMP_20_US;
    }

    write_opmode_command(RADIO_SET_TXPARAMS, buf, 2);
}

void SX126X_LoRaRadio::set_modulation_params(modulation_params_t *params)
{
    uint8_t n;
    uint32_t temp = 0;
    uint8_t buf[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if (_active_modem != params->modem_type) {
        set_modem(params->modem_type);
    }

    switch (params->modem_type) {
        case MODEM_FSK:
            n = 8;
            temp = (uint32_t)(32 * ((float) XTAL_FREQ / (float) params->params.gfsk.bit_rate));
            buf[0] = (temp >> 16) & 0xFF;
            buf[1] = (temp >> 8) & 0xFF;
            buf[2] = temp & 0xFF;
            buf[3] = params->params.gfsk.modulation_shaping;
            buf[4] = params->params.gfsk.bandwidth;
            temp = (uint32_t)((float) params->params.gfsk.fdev / (float) FREQ_STEP);
            buf[5] = (temp >> 16) & 0xFF;
            buf[6] = (temp >> 8) & 0xFF;
            buf[7] = (temp & 0xFF);
            write_opmode_command(RADIO_SET_MODULATIONPARAMS, buf, n);
            break;

        case MODEM_LORA:
            n = 4;
            buf[0] = params->params.lora.spreading_factor;
            buf[1] = params->params.lora.bandwidth;
            buf[2] = params->params.lora.coding_rate;
            buf[3] = params->params.lora.low_datarate_optimization;

            write_opmode_command(RADIO_SET_MODULATIONPARAMS, buf, n);
            break;

        default:
            return;
    }
}

void SX126X_LoRaRadio::set_pa_config(uint8_t pa_DC, uint8_t hp_max,
                                     uint8_t device_type, uint8_t pa_LUT)
{
    uint8_t buf[4];

    buf[0] = pa_DC;
    buf[1] = hp_max;
    buf[2] = device_type;
    buf[3] = pa_LUT;
    write_opmode_command(RADIO_SET_PACONFIG, buf, 4);
}

void SX126X_LoRaRadio::set_crc_seed(uint16_t seed)
{
    if (_active_modem == MODEM_FSK) {
        uint8_t buf[2];
        buf[0] = (uint8_t)((seed >> 8) & 0xFF);
        buf[1] = (uint8_t)(seed & 0xFF);
        write_to_register(REG_LR_CRCSEEDBASEADDR, buf, 2);
    }
}

void SX126X_LoRaRadio::set_crc_polynomial(uint16_t polynomial)
{
    if (_active_modem == MODEM_FSK) {
        uint8_t buf[2];
        buf[0] = (uint8_t)((polynomial >> 8) & 0xFF);
        buf[1] = (uint8_t)(polynomial & 0xFF);
        write_to_register(REG_LR_CRCPOLYBASEADDR, buf, 2);
    }
}

void SX126X_LoRaRadio::set_whitening_seed(uint16_t seed)
{
    if (_active_modem == MODEM_FSK) {
        uint8_t reg_value = read_register(REG_LR_WHITSEEDBASEADDR_MSB) & 0xFE;
        reg_value = ((seed >> 8) & 0x01) | reg_value;
        write_to_register(REG_LR_WHITSEEDBASEADDR_MSB, reg_value); // only 1 bit.
        write_to_register(REG_LR_WHITSEEDBASEADDR_LSB, (uint8_t) seed);
    }
}

void SX126X_LoRaRadio::set_packet_params(packet_params_t *packet_params)
{
    uint8_t n;
    uint8_t crc_val = 0;
    uint8_t buf[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if (_active_modem != packet_params->modem_type) {
        set_modem(packet_params->modem_type);
    }

    switch (packet_params->modem_type) {
        case MODEM_FSK:
            if (packet_params->params.gfsk.crc_length == RADIO_CRC_2_BYTES_IBM) {
                set_crc_seed(CRC_IBM_SEED);
                set_crc_polynomial(CRC_POLYNOMIAL_IBM);
                crc_val = RADIO_CRC_2_BYTES;
            } else if (packet_params->params.gfsk.crc_length == RADIO_CRC_2_BYTES_CCIT) {
                set_crc_seed(CRC_CCITT_SEED);
                set_crc_polynomial(CRC_POLYNOMIAL_CCITT);
                crc_val = RADIO_CRC_2_BYTES_INV;
            } else {
                crc_val = packet_params->params.gfsk.crc_length;
            }
            n = 9;
            buf[0] = (packet_params->params.gfsk.preamble_length >> 8) & 0xFF;
            buf[1] = packet_params->params.gfsk.preamble_length;
            buf[2] = packet_params->params.gfsk.preamble_min_detect;
            buf[3] = (packet_params->params.gfsk.syncword_length /*<< 3*/); // convert from byte to bit
            buf[4] = packet_params->params.gfsk.addr_comp;
            buf[5] = packet_params->params.gfsk.header_type;
            buf[6] = packet_params->params.gfsk.payload_length;
            buf[7] = crc_val;
            buf[8] = packet_params->params.gfsk.whitening_mode;
            break;

        case MODEM_LORA:
            n = 6;
            buf[0] = (packet_params->params.lora.preamble_length >> 8) & 0xFF;
            buf[1] = packet_params->params.lora.preamble_length;
            buf[2] = packet_params->params.lora.header_type;
            buf[3] = packet_params->params.lora.payload_length;
            buf[4] = packet_params->params.lora.crc_mode;
            buf[5] = packet_params->params.lora.invert_IQ;
            break;
        default:
            return;
    }
    write_opmode_command(RADIO_SET_PACKETPARAMS, buf, n);
}

void SX126X_LoRaRadio::set_cad_params(lora_cad_symbols_t nb_symbols,
                                      uint8_t det_peak, uint8_t det_min,
                                      cad_exit_modes_t exit_mode,
                                      uint32_t timeout)
{
    uint8_t buf[7];

    buf[0] = (uint8_t) nb_symbols;
    buf[1] = det_peak;
    buf[2] = det_min;
    buf[3] = (uint8_t) exit_mode;
    buf[4] = (uint8_t)((timeout >> 16) & 0xFF);
    buf[5] = (uint8_t)((timeout >> 8) & 0xFF);
    buf[6] = (uint8_t)(timeout & 0xFF);
    write_opmode_command((uint8_t) RADIO_SET_CADPARAMS, buf, 7);

    _operation_mode = MODE_CAD;
}

void SX126X_LoRaRadio::set_buffer_base_addr(uint8_t tx_base_addr, uint8_t rx_base_addr)
{
    uint8_t buf[2];

    buf[0] = tx_base_addr;
    buf[1] = rx_base_addr;
    write_opmode_command((uint8_t) RADIO_SET_BUFFERBASEADDRESS, buf, 2);
}

uint8_t SX126X_LoRaRadio::get_status(void)
{
    if (_operation_mode != MODE_STDBY_RC || _operation_mode != MODE_SLEEP) {
        return 0;
    }

    return 0xFF;
}

int8_t SX126X_LoRaRadio::get_rssi()
{
    uint8_t buf[1];
    int8_t rssi = 0;

    read_opmode_command((uint8_t) RADIO_GET_RSSIINST, buf, 1);
    rssi = -buf[0] >> 1;
    return rssi;
}

void SX126X_LoRaRadio::get_rx_buffer_status(uint8_t *payload_len,
                                            uint8_t *start_ptr)
{
    uint8_t status[2];

    read_opmode_command((uint8_t) RADIO_GET_RXBUFFERSTATUS, status, 2);

    // In case of LORA fixed header, the payloadLength is obtained by reading
    // the register REG_LR_PAYLOADLENGTH
    if ((get_modem() == MODEM_LORA) &&
            (read_register(REG_LR_PACKETPARAMS) >> 7 == 1)) {
        *payload_len = read_register(REG_LR_PAYLOADLENGTH);
    } else {
        *payload_len = status[0];
    }

    *start_ptr = status[1];
}

void SX126X_LoRaRadio::get_packet_status(packet_status_t *pkt_status)
{
    uint8_t status[3];

    read_opmode_command((uint8_t) RADIO_GET_PACKETSTATUS, status, 3);

    pkt_status->modem_type = (radio_modems_t) get_modem();
    switch (pkt_status->modem_type) {
        case MODEM_FSK:
            pkt_status->params.gfsk.rx_status = status[0];
            pkt_status->params.gfsk.rssi_sync = -status[1] >> 1;
            pkt_status->params.gfsk.rssi_avg = -status[2] >> 1;
            pkt_status->params.gfsk.freq_error = 0;
            break;

        case MODEM_LORA:
            pkt_status->params.lora.rssi_pkt = -status[0] >> 1;
            // Returns SNR value [dB] rounded to the nearest integer value
            pkt_status->params.lora.snr_pkt = (((int8_t) status[1]) + 2) >> 2;
            pkt_status->params.lora.signal_rssi_pkt = -status[2] >> 1;
            break;

        default:
            // In that specific case, we set everything in the pkt_status to zeros
            // and reset the packet type accordingly
            memset(pkt_status, 0, sizeof(packet_status_t));
            break;
    }
}

radio_error_t SX126X_LoRaRadio::get_device_errors(void)
{
    radio_error_t error;

    read_opmode_command((uint8_t) RADIO_GET_ERROR, (uint8_t *)&error, 2);
    return error;
}

void SX126X_LoRaRadio::clear_device_errors(void)
{
    uint8_t buf[2] = {0x00, 0x00};
    write_opmode_command((uint8_t) RADIO_CLR_ERROR, buf, 2);
}

#endif // DEVICE_SPI
