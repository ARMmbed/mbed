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

#ifndef MBED_LORA_RADIO_DRV_SX126X_LORARADIO_H_
#define MBED_LORA_RADIO_DRV_SX126X_LORARADIO_H_

#if DEVICE_SPI

#include "mbed_critical.h"
#include "PinNames.h"
#include "InterruptIn.h"
#include "DigitalOut.h"
#include "DigitalInOut.h"
#include "DigitalIn.h"
#include "AnalogIn.h"
#include "SPI.h"
#include "platform/PlatformMutex.h"
#ifdef MBED_CONF_RTOS_PRESENT
#include "rtos/Thread.h"
#include "rtos/ThisThread.h"
#endif
#include "sx126x_ds.h"
#include "lorawan/LoRaRadio.h"

#ifdef MBED_CONF_SX126X_LORA_DRIVER_BUFFER_SIZE
#define MAX_DATA_BUFFER_SIZE_SX126X                        MBED_CONF_SX126X_LORA_DRIVER_BUFFER_SIZE
#else
#define MAX_DATA_BUFFER_SIZE_SX126X                        255
#endif

class SX126X_LoRaRadio : public LoRaRadio {

public:
    SX126X_LoRaRadio(PinName mosi,
                     PinName miso,
                     PinName sclk,
                     PinName nss,
                     PinName reset,
                     PinName dio1,
                     PinName busy,
                     PinName freq_select,
                     PinName device_select,
                     PinName crystal_select,
                     PinName ant_switch);

    virtual ~SX126X_LoRaRadio();

    /**
     * Registers radio events with the Mbed LoRaWAN stack and
     * undergoes initialization steps if any
     *
     *  @param events Structure containing the driver callback functions
     */
    virtual void init_radio(radio_events_t *events);

    /**
     * Resets the radio module
     */
    virtual void radio_reset();

    /**
     *  Put the RF module in sleep mode
     */
    virtual void sleep(void);

    /**
     *  Sets the radio in standby mode
     */
    virtual void standby(void);

    /**
     *  Sets the reception parameters
     *
     *  @param modem         Radio modem to be used [0: FSK, 1: LoRa]
     *  @param bandwidth     Sets the bandwidth
     *                          FSK : >= 2600 and <= 250000 Hz
     *                          LoRa: [0: 125 kHz, 1: 250 kHz,
     *                                 2: 500 kHz, 3: Reserved]
     *  @param datarate      Sets the Datarate
     *                          FSK : 600..300000 bits/s
     *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
     *                                10: 1024, 11: 2048, 12: 4096  chips]
     *  @param coderate      Sets the coding rate ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
     *  @param bandwidth_afc Sets the AFC Bandwidth ( FSK only )
     *                          FSK : >= 2600 and <= 250000 Hz
     *                          LoRa: N/A ( set to 0 )
     *  @param preamble_len  Sets the Preamble length ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: Length in symbols ( the hardware adds 4 more symbols )
     *  @param symb_timeout  Sets the RxSingle timeout value
     *                          FSK : timeout number of bytes
     *                          LoRa: timeout in symbols
     *  @param fixLen        Fixed length packets [0: variable, 1: fixed]
     *  @param payload_len   Sets payload length when fixed lenght is used
     *  @param crc_on        Enables/Disables the CRC [0: OFF, 1: ON]
     *  @param freq_hop_on   Enables disables the intra-packet frequency hopping  [0: OFF, 1: ON] (LoRa only)
     *  @param hop_period    Number of symbols bewteen each hop (LoRa only)
     *  @param iq_inverted   Inverts IQ signals ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [0: not inverted, 1: inverted]
     *  @param rx_continuous Sets the reception in continuous mode
     *                          [false: single mode, true: continuous mode]
     */
    virtual void set_rx_config(radio_modems_t modem, uint32_t bandwidth,
                               uint32_t datarate, uint8_t coderate,
                               uint32_t bandwidth_afc, uint16_t preamble_len,
                               uint16_t symb_timeout, bool fix_len,
                               uint8_t payload_len,
                               bool crc_on, bool freq_hop_on, uint8_t hop_period,
                               bool iq_inverted, bool rx_continuous);

    /**
     *  Sets the transmission parameters
     *
     *  @param modem         Radio modem to be used [0: FSK, 1: LoRa]
     *  @param power         Sets the output power [dBm]
     *  @param fdev          Sets the frequency deviation ( FSK only )
     *                          FSK : [Hz]
     *                          LoRa: 0
     *  @param bandwidth     Sets the bandwidth ( LoRa only )
     *                          FSK : 0
     *                          LoRa: [0: 125 kHz, 1: 250 kHz,
     *                                 2: 500 kHz, 3: Reserved]
     *  @param datarate      Sets the Datarate
     *                          FSK : 600..300000 bits/s
     *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
     *                                10: 1024, 11: 2048, 12: 4096  chips]
     *  @param coderate      Sets the coding rate ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
     *  @param preamble_len  Sets the preamble length
     *  @param fix_len       Fixed length packets [0: variable, 1: fixed]
     *  @param crc_on        Enables disables the CRC [0: OFF, 1: ON]
     *  @param freq_hop_on   Enables disables the intra-packet frequency hopping  [0: OFF, 1: ON] (LoRa only)
     *  @param hop_period    Number of symbols bewteen each hop (LoRa only)
     *  @param iq_inverted   Inverts IQ signals ( LoRa only )
     *                          FSK : N/A ( set to 0 )
     *                          LoRa: [0: not inverted, 1: inverted]
     *  @param timeout       Transmission timeout [ms]
     */
    virtual void set_tx_config(radio_modems_t modem, int8_t power, uint32_t fdev,
                               uint32_t bandwidth, uint32_t datarate,
                               uint8_t coderate, uint16_t preamble_len,
                               bool fix_len, bool crc_on, bool freq_hop_on,
                               uint8_t hop_period, bool iq_inverted, uint32_t timeout);

    /**
     *  Sends the buffer of size
     *
     *  Prepares the packet to be sent and sets the radio in transmission
     *
     *  @param buffer        Buffer pointer
     *  @param size          Buffer size
     */
    virtual void send(uint8_t *buffer, uint8_t size);

    /**
     * Sets the radio to receive
     *
     * All necessary configuration options for reception are set in
     * 'set_rx_config(parameters)' API.
     */
    virtual void receive(void);

    /**
     *  Sets the carrier frequency
     *
     *  @param freq          Channel RF frequency
     */
    virtual void set_channel(uint32_t freq);

    /**
     *  Generates a 32 bits random value based on the RSSI readings
     *
     *  Remark this function sets the radio in LoRa modem mode and disables
     *         all interrupts.
     *         After calling this function either Radio.SetRxConfig or
     *         Radio.SetTxConfig functions must be called.
     *
     *  @return             32 bits random value
     */
    virtual uint32_t random(void);

    /**
     *  Get radio status
     *
     *  @param status        Radio status [RF_IDLE, RF_RX_RUNNING, RF_TX_RUNNING]
     *  @return              Return current radio status
     */
    virtual uint8_t get_status(void);

    /**
     *  Sets the maximum payload length
     *
     *  @param modem         Radio modem to be used [0: FSK, 1: LoRa]
     *  @param max           Maximum payload length in bytes
     */
    virtual void set_max_payload_length(radio_modems_t modem, uint8_t max);

    /**
     *  Sets the network to public or private
     *
     *  Updates the sync byte. Applies to LoRa modem only
     *
     *  @param enable        if true, it enables a public network
     */
    virtual void set_public_network(bool enable);

    /**
     *  Computes the packet time on air for the given payload
     *
     *  Remark can only be called once SetRxConfig or SetTxConfig have been called
     *
     *  @param modem         Radio modem to be used [0: FSK, 1: LoRa]
     *  @param pkt_len       Packet payload length
     *  @return              Computed airTime for the given packet payload length
     */
    virtual uint32_t time_on_air(radio_modems_t modem, uint8_t pkt_len);

    /**
     * Perform carrier sensing
     *
     * Checks for a certain time if the RSSI is above a given threshold.
     * This threshold determines if there is already a transmission going on
     * in the channel or not.
     *
     * @param modem                     Type of the radio modem
     * @param freq                      Carrier frequency
     * @param rssi_threshold            Threshold value of RSSI
     * @param max_carrier_sense_time    time to sense the channel
     *
     * @return                          true if there is no active transmission
     *                                  in the channel, false otherwise
     */
    virtual bool perform_carrier_sense(radio_modems_t modem,
                                       uint32_t freq,
                                       int16_t rssi_threshold,
                                       uint32_t max_carrier_sense_time);

    /**
     *  Sets the radio in CAD mode
     *
     */
    virtual void start_cad(void);

    /**
     *  Check if the given RF is in range
     *
     *  @param frequency       frequency needed to be checked
     */
    virtual bool check_rf_frequency(uint32_t frequency);

    /** Sets the radio in continuous wave transmission mode
     *
     *  @param freq          Channel RF frequency
     *  @param power         Sets the output power [dBm]
     *  @param time          Transmission mode timeout [s]
     */
    virtual void set_tx_continuous_wave(uint32_t freq, int8_t power, uint16_t time);

    /**
     * Acquire exclusive access
     */
    virtual void lock(void);

    /**
     * Release exclusive access
     */
    virtual void unlock(void);

private:

    // SPI and chip select control
    mbed::SPI _spi;
    mbed::DigitalOut _chip_select;

    // module rest control
    mbed::DigitalInOut _reset_ctl;

    // Interrupt controls
    mbed::InterruptIn _dio1_ctl;;

    // module busy control
    mbed::DigitalIn _busy;

// module frequency selection
#if MBED_CONF_SX126X_LORA_DRIVER_FREQ_SUPPORT == -1
    mbed::AnalogIn _freq_select;
#else
    mbed::DigitalIn _freq_select; // AnalogIn errors if NC
#endif

// module device variant selection
#if MBED_CONF_SX126X_LORA_DRIVER_DEVICE_VARIANT == -1
    mbed::AnalogIn _dev_select;
#else
    mbed::DigitalIn _dev_select;  // AnalogIn errors if NC
#endif

    // module TCXO/XTAL control
    mbed::DigitalIn _crystal_select;

    // Radio specific controls (TX/RX duplexer switch control)
    mbed::DigitalInOut _ant_switch;

    // Structure containing function pointers to the stack callbacks
    radio_events_t *_radio_events;

    // Data buffer used for both TX and RX
    // Size of this buffer is configurable via Mbed config system
    // Default is 255 bytes
    uint8_t _data_buffer[MAX_DATA_BUFFER_SIZE_SX126X];

#ifdef MBED_CONF_RTOS_PRESENT
    // Thread to handle interrupts
    rtos::Thread irq_thread;
#endif

    // Access protection
    PlatformMutex mutex;

    // helper functions
    void wakeup();
    void read_opmode_command(uint8_t cmd, uint8_t *buffer, uint16_t size);
    void write_opmode_command(uint8_t cmd, uint8_t *buffer, uint16_t size);
    void set_dio2_as_rfswitch_ctrl(uint8_t enable);
    void set_dio3_as_tcxo_ctrl(radio_TCXO_ctrl_voltage_t voltage, uint32_t timeout);
    uint8_t get_device_variant(void);
    void set_device_ready(void);
    int8_t get_rssi();
    uint8_t get_fsk_bw_reg_val(uint32_t bandwidth);
    void write_to_register(uint16_t addr, uint8_t data);
    void write_to_register(uint16_t addr, uint8_t *data, uint8_t size);
    uint8_t read_register(uint16_t addr);
    void read_register(uint16_t addr, uint8_t *buffer, uint8_t size);
    void write_fifo(uint8_t *buffer, uint8_t size);
    void read_fifo(uint8_t *buffer, uint8_t size, uint8_t offset);
    void rf_irq_task(void);
    void set_modem(uint8_t modem);
    uint8_t get_modem();
    uint16_t get_irq_status(void);
    uint8_t get_frequency_support(void);

    // ISR
    void dio1_irq_isr();

    // Handler called by thread in response to signal
    void handle_dio1_irq();

    void set_modulation_params(modulation_params_t *modulationParams);
    void set_packet_params(packet_params_t *packet_params);
    void set_cad_params(lora_cad_symbols_t nb_symbols, uint8_t det_peak,
                        uint8_t det_min, cad_exit_modes_t exit_mode,
                        uint32_t timeout);
    void set_buffer_base_addr(uint8_t tx_base_addr, uint8_t rx_base_addr);
    void get_rx_buffer_status(uint8_t *payload_len, uint8_t *rx_buffer_ptr);
    void get_packet_status(packet_status_t *pkt_status);
    radio_error_t get_device_errors(void);
    void clear_device_errors(void);
    void clear_irq_status(uint16_t irq);
    void set_crc_seed(uint16_t seed);
    void set_crc_polynomial(uint16_t polynomial);
    void set_whitening_seed(uint16_t seed);
    void set_pa_config(uint8_t pa_DC, uint8_t hp_max, uint8_t device_type,
                       uint8_t pa_LUT);
    void set_tx_power(int8_t power);
    void calibrate_image(uint32_t freq);
    void configure_dio_irq(uint16_t irq_mask, uint16_t dio1_mask,
                           uint16_t dio2_mask, uint16_t dio3_mask);
    void cold_start_wakeup();

private:
    uint8_t _active_modem;
    uint8_t _standby_mode;
    uint8_t _operation_mode;
    uint8_t _reception_mode;
    uint32_t _tx_timeout;
    uint32_t _rx_timeout;
    uint8_t _rx_timeout_in_symbols;
    int8_t _tx_power;
    bool _image_calibrated;
    bool _force_image_calibration;
    bool _network_mode_public;

    // Structure containing all user and network specified settings
    // for radio module
    modulation_params_t _mod_params;
    packet_params_t _packet_params;
};

#endif // DEVICE_SPI

#endif /* MBED_LORA_RADIO_DRV_SX126X_LORARADIO_H_ */
