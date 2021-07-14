/*!
 * @file      lr1110_radio.h
 *
 * @brief     Radio driver definition for LR1110
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LR1110_RADIO_H
#define LR1110_RADIO_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include "lr1110_radio_types.h"
#include "lr1110_types.h"

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- RADIO CLASS ------------------------------------------------------------
 */
 
class lr1110_LoRaRadio: public LoRaRadio {
public:
	/**
     * Use this constructor if pin definitions are provided manually.
     * The pins that are marked NC are optional. It is assumed that these
     * pins are not connected until/unless configured otherwise.
     */
    lr1110_LoRaRadio(PinName mosi     		= MBED_CONF_lr1110_LORA_DRIVER_SPI_MOSI,
                     PinName miso           = MBED_CONF_lr1110_LORA_DRIVER_SPI_MISO
                     PinName sclk           = MBED_CONF_lr1110_LORA_DRIVER_SPI_SCLK,
                     PinName nss            = MBED_CONF_lr1110_LORA_DRIVER_SPI_CS,
                     PinName nreset         = MBED_CONF_lr1110_LORA_DRIVER_NRESET,
                     PinName dio7           = MBED_CONF_lr1110_LORA_DRIVER_DIO7,
                     PinName dio8           = MBED_CONF_lr1110_LORA_DRIVER_DIO8,
                     PinName dio9           = MBED_CONF_lr1110_LORA_DRIVER_DIO9,
                     PinName busy 			= MBED_CONF_lr1110_LORA_DRIVER_BUSY,
                     PinName lnactrlon		= MBED_CONF_lr1110_LORA_DRIVER_LNACTRLON
                    );

    /**
     * Destructor
     */
    virtual ~lr1110_LoRaRadio();
/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/*!
 * @brief Reset internal statistics of the received packets
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 *
 * @see lr1110_radio_get_gfsk_stats, lr1110_radio_get_lora_stats
 */
lr1110_status_t lr1110_radio_reset_stats( const void* context );

/*!
 * @brief Get the internal statistics of the GFSK received packets
 *
 * Internal statistics are reset on Power on Reset, by entering sleep mode without memory retention, or by calling @ref
 * lr1110_radio_reset_stats.
 *
 * @param [in] context Chip implementation context
 * @param [out] stats The statistics structure of the received packets
 *
 * @returns Operation status
 *
 * @see lr1110_radio_reset_stats
 */
lr1110_status_t lr1110_radio_get_gfsk_stats( const void* context, lr1110_radio_stats_gfsk_t* stats );

/*!
 * @brief Get the internal statistics of the LoRa received packets
 *
 * Internal statistics are reset on Power on Reset, by entering sleep mode without memory retention, or by calling @ref
 * lr1110_radio_reset_stats.
 *
 * @param [in] context Chip implementation context
 * @param [out] stats The statistics structure of the received packets
 *
 * @returns Operation status
 *
 * @see lr1110_radio_reset_stats
 */
lr1110_status_t lr1110_radio_get_lora_stats( const void* context, lr1110_radio_stats_lora_t* stats );

/*!
 * @brief Get the packet type currently configured
 *
 * @param [in] context Chip implementation context
 * @param [out] pkt_type The packet type currently configured
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type
 */
lr1110_status_t lr1110_radio_get_pkt_type( const void* context, lr1110_radio_pkt_type_t* pkt_type );

/*!
 * @brief Get the length of last received packet, and the offset in the RX internal buffer of the first byte of the
 * received payload
 *
 * @param [in] context Chip implementation context
 * @param [out] rx_buffer_status The structure of RX buffer status
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_get_rx_buffer_status( const void*                      context,
                                                   lr1110_radio_rx_buffer_status_t* rx_buffer_status );

/*!
 * @brief Get the status of last GFSK received packet
 *
 * The value depends on the received packet type
 *
 * @param [in] context Chip implementation context
 * @param[out] pkt_status The last received packet status
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_get_gfsk_pkt_status( const void* context, lr1110_radio_pkt_status_gfsk_t* pkt_status );

/*!
 * @brief Get the status of last LoRa received packet
 *
 * The value depends on the received packet type
 *
 * @param [in] context Chip implementation context
 * @param[out] pkt_status The last received packet status
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_get_lora_pkt_status( const void* context, lr1110_radio_pkt_status_lora_t* pkt_status );

/*!
 * @brief Get the instantaneous RSSI.
 *
 * This command can be used during reception of a packet
 *
 * @param [in] context Chip implementation context
 * @param [out] rssi_in_dbm Instantaneous RSSI.
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_get_rssi_inst( const void* context, int8_t* rssi_in_dbm );

/*!
 * @brief Set the GFSK modem sync word
 *
 * This command is used to set the GFSK nodem sync word. By default, the value is 0x9723522556536564
 *
 * @param [in] context Chip implementation context
 * @param [in] gfsk_sync_word The sync word to be configured
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_gfsk_sync_word( const void* context, const uint8_t* gfsk_sync_word );

/*!
 * @brief Set the LoRa modem sync word
 *
 * @param [in] context Chip implementation context
 * @param [in] sync_word The sync word to be configured
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_lora_sync_word( const void* context, const uint8_t sync_word );

/*!
 * @brief Set the LoRa modem sync word to private / public
 *
 * This command is used to select which LoRa network is selected
 *
 * @param [in] context Chip implementation context
 * @param [in] network_type The network type to be configured
 *
 * @returns Operation status
 *
 * @warning This function is deprecated. Use lr1110_radio_set_lora_sync_word for chip firmware equal to or more recent
 * than 0x303.
 */
lr1110_status_t lr1110_radio_set_lora_public_network( const void*                            context,
                                                      const lr1110_radio_lora_network_type_t network_type );

/*!
 * @brief Start RX operations
 *
 * This command sets the LR1110 to RX mode. The radio must have been configured before using this command with @ref
 * lr1110_radio_set_pkt_type
 *
 * By default, the timeout parameter allows to return automatically to standby RC mode if no packets have been received
 * after a certain amount of time. This behavior can be altered by @ref lr1110_radio_set_rx_tx_fallback_mode and @ref
 * lr1110_radio_auto_tx_rx.
 *
 * @param [in] context Chip implementation context
 * @param [in] timeout_in_ms The timeout configuration for RX operation
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_rx_tx_fallback_mode
 */
lr1110_status_t lr1110_radio_set_rx( const void* context, const uint32_t timeout_in_ms );

/*!
 * @brief Start RX operations
 *
 * This command sets the LR1110 to RX mode. The radio must have been configured before using this command with @ref
 * lr1110_radio_set_pkt_type
 *
 * By default, the timeout parameter allows to return automatically to standby RC mode if no packets have been received
 * after a certain amount of time. This behavior can be altered by @ref lr1110_radio_set_rx_tx_fallback_mode and @ref
 * lr1110_radio_auto_tx_rx.
 *
 * The timeout duration is obtained by:
 * \f$ timeout\_duration\_ms = timeout \times \frac{1}{32.768} \f$
 *
 * Maximal timeout value is 0xFFFFFF, which gives a maximal timeout of 511 seconds.
 *
 * The timeout argument can also have the following special values:
 * <table>
 * <tr><th> Special values </th><th> Meaning </th>
 * <tr><td> 0x000000 </td><td> RX single: LR1110 stays in RX mode until a
 * packet is received, then switch to standby RC mode <tr><td> 0xFFFFFF
 * </td><td> RX continuous: LR1110 stays in RX mode even after reception of a
 * packet
 * </table>
 *
 * @param [in] context Chip implementation context
 * @param [in] timeout_in_rtc_step The timeout configuration for RX operation
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_rx_tx_fallback_mode
 */
lr1110_status_t lr1110_radio_set_rx_with_timeout_in_rtc_step( const void* context, const uint32_t timeout_in_rtc_step );

/*!
 * @brief Start TX operations
 *
 * This command sets the LR1110 to TX mode. The radio must have been configured before using this command with @ref
 * lr1110_radio_set_pkt_type
 *
 * By default, the timeout parameter allows to return automatically to standby RC mode if the packet has not been
 * completely transmitted after a certain amount of time. This behavior can be altered by @ref
 * lr1110_radio_set_rx_tx_fallback_mode and @ref lr1110_radio_auto_tx_rx.
 *
 * @param [in] context Chip implementation context
 * @param [in] timeout_in_ms The timeout configuration for TX operation
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_rx_tx_fallback_mode
 */
lr1110_status_t lr1110_radio_set_tx( const void* context, const uint32_t timeout_in_ms );

/*!
 * @brief Start TX operations
 *
 * This command sets the LR1110 to TX mode. The radio must have been configured before using this command with @ref
 * lr1110_radio_set_pkt_type
 *
 * By default, the timeout parameter allows to return automatically to standby RC mode if the packet has not been
 * completely transmitted after a certain amount of time. This behavior can be altered by @ref
 * lr1110_radio_set_rx_tx_fallback_mode and @ref lr1110_radio_auto_tx_rx.
 *
 * The timeout duration is obtained by:
 * \f$ timeout\_duration\_ms = timeout \times \frac{1}{32.768} \f$
 *
 * Maximal value is 0xFFFFFF.
 *
 * If the timeout argument is 0, then no timeout is used.
 *
 * @param [in] context Chip implementation context
 * @param [in] timeout_in_rtc_step The timeout configuration for TX operation
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_rx_tx_fallback_mode
 */
lr1110_status_t lr1110_radio_set_tx_with_timeout_in_rtc_step( const void* context, const uint32_t timeout_in_rtc_step );

/*!
 * @brief Set the frequency for future radio operations.
 *
 * This commands does not set frequency for Wi-Fi and GNSS scan operations.
 *
 * @param [in] context Chip implementation context
 * @param [in] freq_in_hz The frequency in Hz to set for radio operations
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_rf_freq( const void* context, const uint32_t freq_in_hz );

/*!
 * @brief Configure automatic TX after RX or automatic RX after TX
 *
 * After issuing this command, using the command @ref SetTx will make the LR1110 doing the following:
 *   - Enter TX mode as usual
 *   - Enter configurable Intermediary mode during configurable delay
 *   - Enter RX mode
 *
 * Similarly, after a @ref SetRx command, the LR1110 will do the following:
 *   - Enter RX mode as usual
 *   - Enter configurable Intermediary mode during configurable delay
 *   - Enter TX mode
 *
 * In case delay is 0, the LR1110 does not enter Intermediary mode and directly enter the following mode.
 *
 * To disable this behavior, use this function with delay set to 0xFFFFFFFF.
 *
 * @param [in] context Chip implementation context
 * @param [in] delay Time to spend in Intermediary mode expressed as steps of \f$\frac{1}{32.768 KHz}\f$ steps.
 * @param [in] intermediary_mode The mode the LR1110 enters after first mode completion during delay time
 * @param [in] timeout The timeout duration of the automatic RX or TX, expressed as steps of \f$ \frac{1}{32.768KHz} \f$
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_auto_tx_rx( const void* context, const uint32_t delay,
                                         const lr1110_radio_intermediary_mode_t intermediary_mode,
                                         const uint32_t                         timeout );

/*!
 * @brief Set Channel Activity Detection configuration
 *
 * @param [in] context Chip implementation context
 * @param [in] cad_params The structure defining CAD configuration
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_cad_params( const void* context, const lr1110_radio_cad_params_t* cad_params );

/*!
 * @brief Set the packet type
 *
 * @param [in] context Chip implementation context
 * @param [in] pkt_type Packet type to set
 *
 * @returns Operation status
 *
 * @see lr1110_radio_get_pkt_type
 */
lr1110_status_t lr1110_radio_set_pkt_type( const void* context, const lr1110_radio_pkt_type_t pkt_type );

/*!
 * @brief Set the modulation parameters for GFSK packets
 *
 * The command @ref lr1110_radio_set_pkt_type must be called prior this one.
 *
 * @param [in] context Chip implementation context
 * @param [in] mod_params The structure of modulation configuration
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type
 */
lr1110_status_t lr1110_radio_set_gfsk_mod_params( const void*                           context,
                                                  const lr1110_radio_mod_params_gfsk_t* mod_params );

/*!
 * @brief Set the modulation parameters for LoRa packets
 *
 * The command @ref lr1110_radio_set_pkt_type must be called prior this one.
 *
 * @param [in] context Chip implementation context
 * @param [in] mod_params The structure of modulation configuration
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type
 */
lr1110_status_t lr1110_radio_set_lora_mod_params( const void*                           context,
                                                  const lr1110_radio_mod_params_lora_t* mod_params );

/*!
 * @brief Set the packet parameters for GFSK packets
 *
 * The command @ref lr1110_radio_set_pkt_type must be called prior this one.
 *
 * @param [in] context Chip implementation context
 * @param [in] pkt_params The structure of packet configuration
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_gfsk_mod_params
 */
lr1110_status_t lr1110_radio_set_gfsk_pkt_params( const void*                           context,
                                                  const lr1110_radio_pkt_params_gfsk_t* pkt_params );

/*!
 * @brief Set the packet parameters for LoRa packets
 *
 * The command @ref lr1110_radio_set_pkt_type must be called prior this one.
 *
 * @param [in] context Chip implementation context
 * @param [in] pkt_params The structure of packet configuration
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type, lr1110_radio_set_lora_mod_params
 */
lr1110_status_t lr1110_radio_set_lora_pkt_params( const void*                           context,
                                                  const lr1110_radio_pkt_params_lora_t* pkt_params );

/*!
 * @brief Set the parameters for TX power and power amplifier ramp time
 *
 * The command @ref lr1110_radio_set_pa_cfg must be called prior calling
 * lr1110_radio_set_tx_params.
 *
 * The range of possible TX output power values depends on PA selected with @ref lr1110_radio_set_pa_cfg :
 *   - for LPA: power value goes from -17dBm to +14dBm (ie. from 0xEF to 0x0E)
 *   - for HPA: power value goes from -9dBm to +22dBm (ie. from 0xF7 to 0x16)
 *
 * Moreover, to use TX output power value higher than +10dBm, the @ref REGPASUPPLY_VBAT supply must have been selected
 * with @ref lr1110_radio_set_pa_cfg.
 *
 * @param [in] context Chip implementation context
 * @param [in] pwr_in_dbm The TX output power in dBm
 * @param [in] ramp_time The ramping time configuration for the PA
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_tx_params( const void* context, const int8_t pwr_in_dbm,
                                            const lr1110_radio_ramp_time_t ramp_time );

/*!
 * @brief Sets the Node and Broadcast address used for GFSK
 *
 * This setting is used only when filtering is enabled.
 *
 * @param [in] context Chip implementation context
 * @param [in] node_address The node address used as filter
 * @param [in] broadcast_address The broadcast address used as filter
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_pkt_address( const void* context, const uint8_t node_address,
                                              const uint8_t broadcast_address );

/*!
 * @brief Alter the chip mode after successfull transmission or reception operation
 *
 * This setting is not used during Rx Duty Cycle mode or Auto Tx Rx.
 *
 * @param [in] context Chip implementation context
 * @param [in] fallback_mode The chip mode to enter after successfull transmission or reception.
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_rx_tx_fallback_mode( const void*                         context,
                                                      const lr1110_radio_fallback_modes_t fallback_mode );

/*!
 * @brief Configure and start a Rx Duty Cycle operation
 *
 * It executes the following steps:
 *     1. Reception: enters reception state for duration defined by rx_period
 *         - If mode is LR1110_RADIO_RX_DUTY_CYCLE_MODE_RX: it is standard RX mode
 *         - If mode is LR1110_RADIO_RX_DUTY_CYCLE_MODE_CAD (only in LoRa) : it is CAD operation
 *     2. Depending on the over-the-air activity detection:
 *         - In case of positive over-the-air detection, the rx_period timeout is recomputed to the value
 *           \f$2 \times rx\_period + sleep\_period\f$
 *         - If no air activity is detected, the LR1110 goes back to sleep mode with retention for a duration defined by
 * sleep_period
 *     3. On wake-up, the LR1110 restarts the process with the reception state.
 *
 * @remark If mode is configured to @ref LR1110_RADIO_RX_DUTY_CYCLE_MODE_CAD, then the CAD configuration used in step 1.
 * is the one set from the last call to @ref lr1110_radio_set_cad_params.
 *
 * @param [in] context Chip implementation context
 * @param [in] rx_period_in_ms The length of Rx period
 * @param [in] sleep_period_in_ms The length of sleep period
 * @param [in] mode The operation mode during Rx phase
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_cad_params
 */
lr1110_status_t lr1110_radio_set_rx_duty_cycle( const void* context, const uint32_t rx_period_in_ms,
                                                const uint32_t                          sleep_period_in_ms,
                                                const lr1110_radio_rx_duty_cycle_mode_t mode );

/*!
 * @brief Configure and start a Rx Duty Cycle operation
 *
 * It executes the following steps:
 *     1. Reception: enters reception state for duration defined by rx_period
 *         - If mode is LR1110_RADIO_RX_DUTY_CYCLE_MODE_RX: it is standard RX mode
 *         - If mode is LR1110_RADIO_RX_DUTY_CYCLE_MODE_CAD (only in LoRa) : it is CAD operation
 *     2. Depending on the over-the-air activity detection:
 *         - In case of positive over-the-air detection, the rx_period timeout is recomputed to the value
 *           \f$2 \times rx\_period + sleep\_period\f$
 *         - If no air activity is detected, the LR1110 goes back to sleep mode with retention for a duration defined by
 * sleep_period
 *     3. On wake-up, the LR1110 restarts the process with the reception state.
 *
 * @remark If mode is configured to @ref LR1110_RADIO_RX_DUTY_CYCLE_MODE_CAD, then the CAD configuration used in step 1.
 * is the one set from the last call to @ref lr1110_radio_set_cad_params.
 *
 * @param [in] context Chip implementation context
 * @param [in] rx_period_in_rtc_step The length of Rx period
 * @param [in] sleep_period_in_rtc_step The length of sleep period
 * @param [in] mode The operation mode during Rx phase
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_cad_params
 */
lr1110_status_t lr1110_radio_set_rx_duty_cycle_with_timings_in_rtc_step( const void*    context,
                                                                         const uint32_t rx_period_in_rtc_step,
                                                                         const uint32_t sleep_period_in_rtc_step,
                                                                         const lr1110_radio_rx_duty_cycle_mode_t mode );

/*!
 * @brief Set the Power Amplifier configuration
 *
 * It must be called prior using @ref lr1110_radio_set_tx_params.
 *
 * @param [in] context Chip implementation context
 * @param [in] pa_cfg The structure for PA configuration
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_pa_cfg( const void* context, const lr1110_radio_pa_cfg_t* pa_cfg );

/*!
 * @brief Define on which event the Rx timeout shall be stopped
 *
 * The two options are:
 *   - Syncword / Header detection
 *   - Preamble detection
 *
 * @param [in] context Chip implementation context
 * @param [in] stop_timeout_on_preamble The choice of the event to be taken into account
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_stop_timeout_on_preamble( const void* context, const bool stop_timeout_on_preamble );

/*!
 * @brief Start the CAD mode
 *
 * The LoRa packet type shall be selected before this function is called. The fallback mode is configured with
 * lr1110_radio_set_cad_params.
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_cad_params, lr1110_radio_set_pkt_type
 */
lr1110_status_t lr1110_radio_set_cad( const void* context );

/*!
 * @brief Set the device into Tx continuous wave (RF tone).
 *
 * A packet type shall be selected before this function is called.
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 *
 * @see lr1110_radio_set_pkt_type
 */
lr1110_status_t lr1110_radio_set_tx_cw( const void* context );

/*!
 * @brief Set the device into Tx continuous preamble (modulated signal).
 *
 * @param [in] context Chip implementation context
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_tx_infinite_preamble( const void* context );

/*!
 * @brief Configure the LoRa modem to issue a RX timeout after an exact number of symbols given in parameter if no LoRa
 * modulation is detected
 *
 * @param [in] context Chip implementation context
 * @param [in] nb_symbol number of symbols to compute the timeout
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_lora_sync_timeout( const void* context, const uint8_t nb_symbol );

/*!
 * @brief Configure the seed and the polynomial used to compute CRC in GFSK packet
 *
 * @param [in] context Chip implementation context
 * @param [in] seed Seed used to compute the CRC value
 * @param [in] polynomial Polynomial used to compute the CRC value
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_gfsk_crc_params( const void* context, const uint32_t seed, const uint32_t polynomial );

/*!
 * @brief Configure the whitening seed used in GFSK packet
 *
 * @param [in] context Chip implementation context
 * @param [in] seed Whitening seed value
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_set_gfsk_whitening_seed( const void* context, const uint16_t seed );

/*!
 * @brief Configure the boost mode in reception
 *
 * @param [in] context Chip implementation context
 * @param [in] enable_boost_mode Boost mode activation
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_cfg_rx_boosted( const void* context, const bool enable_boost_mode );

/*!
 * @brief Gets the radio bw parameter for a given bandwidth in Hz
 *
 * @param [in] bw_in_hz Requested GFSK Rx bandwidth
 * @param [out] bw_parameter Radio parameter immediately above requested bw_in_hz
 *
 * @returns Operation status
 */
lr1110_status_t lr1110_radio_get_gfsk_rx_bandwidth( uint32_t bw_in_hz, lr1110_radio_gfsk_bw_t* bw_parameter );

/**
 * @brief Compute the numerator for LoRa time-on-air computation.
 *
 * @remark To get the actual time-on-air in seconds, this value has to be divided by the LoRa bandwidth in Hertz.
 *
 * @param [in] pkt_p Pointer to the structure holding the LoRa packet parameters
 * @param [in] mod_p Pointer to the structure holding the LoRa modulation parameters
 *
 * @returns LoRa time-on-air numerator
 */
uint32_t lr1110_radio_get_lora_time_on_air_numerator( const lr1110_radio_pkt_params_lora_t* pkt_p,
                                                      const lr1110_radio_mod_params_lora_t* mod_p );

/**
 * @brief Get the actual value in Hertz of a given LoRa bandwidth
 *
 * @param [in] bw LoRa bandwidth parameter
 *
 * @returns Actual LoRa bandwidth in Hertz
 */
uint32_t lr1110_radio_get_lora_bw_in_hz( lr1110_radio_lora_bw_t bw );

/*!
 * @brief Get the time on air in ms for LoRa transmission
 *
 * @param [in] pkt_p Pointer to a structure holding the LoRa packet parameters
 * @param [in] mod_p Pointer to a structure holding the LoRa modulation parameters
 *
 * @returns Time-on-air value in ms for LoRa transmission
 */
uint32_t lr1110_radio_get_lora_time_on_air_in_ms( const lr1110_radio_pkt_params_lora_t* pkt_p,
                                                  const lr1110_radio_mod_params_lora_t* mod_p );

/**
 * @brief Compute the numerator for GFSK time-on-air computation.
 *
 * @remark To get the actual time-on-air in seconds, this value has to be divided by the GFSK bitrate in bits per
 * second.
 *
 * @param [in] pkt_p Pointer to the structure holding the GFSK packet parameters
 *
 * @returns GFSK time-on-air numerator
 */
uint32_t lr1110_radio_get_gfsk_time_on_air_numerator( const lr1110_radio_pkt_params_gfsk_t* pkt_p );

/**
 * @brief Get the time on air in ms for GFSK transmission
 *
 * @param [in] pkt_p Pointer to a structure holding the GFSK packet parameters
 * @param [in] mod_p Pointer to a structure holding the GFSK modulation parameters
 *
 * @returns Time-on-air value in ms for GFSK transmission
 */
uint32_t lr1110_radio_get_gfsk_time_on_air_in_ms( const lr1110_radio_pkt_params_gfsk_t* pkt_p,
                                                  const lr1110_radio_mod_params_gfsk_t* mod_p );

/**
 * @brief Get the number of RTC steps for a given time in millisecond
 *
 * @param [in] time_in_ms Timeout in millisecond
 *
 * @returns Number of RTC steps
 */
uint32_t lr1110_radio_convert_time_in_ms_to_rtc_step( uint32_t time_in_ms );
};

#ifdef __cplusplus
}
#endif

#endif  // LR1110_RADIO_H

/* --- EOF ------------------------------------------------------------------ */
