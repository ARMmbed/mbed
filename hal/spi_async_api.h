/** \ingroup hal_spi */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 ARM Limited
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
#ifndef MBED_SPI_API_ASYNC_H
#define MBED_SPI_API_ASYNC_H

#include "hal/spi_api.h"

#if DEVICE_SPI
#if DEVICE_SPI_ASYNCH

/** \defgroup hal_spi_async SPI: Asynchronous API.
 * Asynchronous extension to the SPI HAL API.
 *
 * # Programming model
 * This is an OOP like API. spi_async_transfer_t is used a "this".<br/>
 * Methods not having this type as their first parameter are **class** methods.<br/>
 * Methods with an spi_t object as their first paremeter are **instance** methods.<br/>
 * `spi_async_transfer_new()` acts as the "constructor" of this *class*.<br/>
 * `spi_async_transfer_free()` acts as the "desctructor" of this *class*.
 * 
 * This API extends the hal_spi API.
 * 
 * # Defined behaviours

## Defined behaviours
 * - A repeating transfer must not restart until the invokation of the completion callback has not returned.
 * - The implementation may use interrupt or 
 * 
 * - spi_async_transfer_new(..)
 *   - enqueues a transfer request that will processed as the device become available.
 *   - on successfull enqueueing returns a counted reference to the related hal implementation
 *     resource.
 *     @warning if `repeat` is `false` then any element passed to this function must live at least
 *     until the callback is invoked.
 *     @warning if `repeat` is `true` then any element passed to this function must live at least
 *     until the transfer is freed.
 * - spi_async_transfer_free(..)
 *   - aborts/cancels a transaction if not completed yet and release the associated memory.
 *   - it does **NOT** free the spi_transfer_args_t **nor** the referenced buffers.
 * - spi_event_f(..)
 *   - may update the spi_transfer_args_t structure initialy given to `spi_async_transfer_new(..)`
 *     on a repeating transfer.
 *   - does not block/busy wait/perform heavy calculation.
 *   - does not perfom any rtos unsafe operation.
 *
 * # Undefined behaviours
 * - Calling any method on a `spi_async_tranfer_t` after calling `spi_async_free_handle()`.
 * - Passing invalid pointers to any method.
 * - Freeing buffers before freeing the associated transaction.
 *
 * # Lexicon
 * ## `Completion`/`Cancelletion`/`Abortion`
 * The `completion` of a transfer is reached when one of the following condition is verified :
 * - the transfer has sent and received the amount of symbol specified in `tx_count` and `rx_count`.
 * - an error has occured during the transfer.
 * - an abortion was triggered during the transfer.
 * 
 * The `cancellation` refers to an `abortion request` occuring before the transfer is actually initiated.
 * 
 * The `abortion` refers to `abortion request` occuring during the processing of the transfer.
 * 
 * @{
 */
 
/**
 * This enumerates the possible event types generated by the SPI ASYNC api.
 */
typedef enum spi_event_type_t {
    SPI_EVENT_TYPE_COMPLETION, /**< The operation has completed successfully. */
    SPI_EVENT_TYPE_ERROR /**< An error occured. */
} spi_event_type_t;

/**
 * This gives informations on the transaction state when the completion callback is invoked.
 */
typedef struct spi_transfer_state_s {
    spi_event_type_t event_type; /**< Even type triggering this invokation. */
    uint32_t received; /**< Actual count of received symbols at the time being. */
    uint32_t transmitted; /**< Actual count of transmitted symbols at the time being. */
} spi_transfer_state_t;

/**
 * SPI asynchronous transfer type.
 * This has to be defined by the low-level implementation.
 * This is only manipulated by pointer and thus can be an opaque type.
 */
typedef struct spi_async_transfer_s spi_async_transfer_t;

/**
 * Signature for a SPI async completion event.
 *
 * As this may be executed from an interrupt context it is highly adviced to restrict this callback
 * to signaling completion to a thread.
 *
 * @note This method may update the spi_transfer_args_t initialy passed to spi_async_transfer_new().
 *       This is very handful for chaining transfer saving time from reallocation.
 *
 * @warning This method may be called from an interrupt context and thus shall not call any interrupt
 *          unsafe function. It should also be as quick as possible in order to avoid any latency on
 *          the rest of the system.
 *
 * @param[in,out] context        Any object that was passed to spi_async_transfer_new().
 * @param[in]     transfer       The handle that was returned by spi_async_transfer_new().
 * @param[in]     transfer_state Transfer state at the moment of the call. This state will be valid
 *                               until this method returns.
 */
typedef void (*spi_event_f)(void *context, spi_async_transfer_t* transfer, spi_transfer_state_t transfer_state);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Schedules a transfer using the given parameters.
 *
 * - enqueues a transfer request that will processed as the device become available.
 * - on successfull enqueueing returns a reference to the related hal implementation resource.
 *
 * @warning if `repeat` is `false` then any element passed to this function must live at least
 *          until the callback is invoked.
 * @warning if `repeat` is `true` then any element passed to this function must live at least until
 *          the callback is invoked with the an event type not being `SPI_EVENT_TYPE_ON_DONE`.
 *
 * @param[in,out] obj       A pointer to a spi_t object.
 * @param[in,out] args      A pointer to a spi_transfer_args_t object.
 * @param[in]     context   A context to be used by the callback.
 * @param[in]     cb        A callback invoked upon completion of this transaction.
 * @param[in]     repeat    If true, the transaction will be repeated until it is cancelled.
 *
 * @return A reference counted handle to the transaction.
 */
spi_async_transfer_t* spi_async_transfer_new(
    spi_t *obj,
    spi_transfer_args_t *args,
    void *context,
    spi_event_f cb,
    bool repeat);

/**
 * Tells the low-level driver that the upper layer is no longer keeping this handle.
 *
 * - aborts/cancels a transaction if not completed yet and release the associated memory.
 * - it does **NOT** free the spi_transfer_args_t **nor** the referenced buffers.
 *
 * @param[in] transfer A spi_async_tranfer_t object.
 */
void spi_async_transfer_free(spi_async_tranfer_t* transfer);
#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* DEVICE_SPI_ASYNCH */
#endif /* DEVICE_SPI */

#endif /* MBED_SPI_API_ASYNC_H */
/**
 * @}
 */

