/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#if DEVICE_I2C
#include "i2c_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "mbed_wait_api.h"
#include "PeripheralPins.h"
#include "s5js100_i2c.h"

/** Initialize the I2C peripheral. It sets the default parameters for I2C
 *  peripheral, and configures its specifieds pins.
 *
 *  @param obj  The I2C object
 *  @param pinmap  Pinmap pointer to structure which holds static pinmap
 */
#if STATIC_PINMAP_READY
#define I2C_INIT_DIRECT i2c_init_direct
void i2c_init_direct(i2c_t *obj, const i2c_pinmap_t *pinmap)
#else
#define I2C_INIT_DIRECT _i2c_init_direct
static void _i2c_init_direct(i2c_t *obj, const i2c_pinmap_t *pinmap)
#endif
{
    /* Determine the I2C to use */
    I2CName i2c_name = pinmap->peripheral;

    switch (i2c_name) {
        case I2C_0:
            up_i2cinitialize(obj);
            break;
        default:
            break;
    }
}

/** Initialize the I2C peripheral. It sets the default parameters for I2C
 *  peripheral, and configures its specifieds pins.
 *
 *  @param obj  The I2C object
 *  @param sda  The sda pin
 *  @param scl  The scl pin
 */
void i2c_init(i2c_t *obj, PinName sda, PinName scl)
{
    uint32_t i2c_sda = pinmap_peripheral(sda, PinMap_I2C_SDA);
    uint32_t i2c_scl = pinmap_peripheral(scl, PinMap_I2C_SCL);

    int peripheral = (int) pinmap_merge(i2c_sda, i2c_scl);

    int sda_function = (int) pinmap_function(sda, PinMap_I2C_SDA);
    int scl_function = (int) pinmap_function(scl, PinMap_I2C_SCL);

    const i2c_pinmap_t explicit_i2c_pinmap = { peripheral, sda, sda_function,
                                               scl, scl_function
                                             };

    I2C_INIT_DIRECT(obj, &explicit_i2c_pinmap);
}

/** Release a I2C object
 *
 * Return the pins owned by the I2C object to their reset state
 * @param obj The I2C object to deinitialize
 */
void i2c_free(i2c_t *obj)
{
    obj->ops->free(obj);
}

/** Configure the I2C frequency
 *
 *  @param obj The I2C object
 *  @param hz  Frequency in Hz
 */
void i2c_frequency(i2c_t *obj, int hz)
{
    obj->ops->setfrequency(obj, hz);
}

/** Send START command
 *
 *  @param obj The I2C object
 */
int i2c_start(i2c_t *obj)
{
    return obj->ops->start(obj);
}

/** Send STOP command
 *
 *  @param obj The I2C object
 */
int i2c_stop(i2c_t *obj)
{
    return obj->ops->stop(obj);
}

/** Blocking reading data
 *
 *  @param obj     The I2C object
 *  @param address 7-bit address (last bit is 1)
 *  @param data    The buffer for receiving
 *  @param length  Number of bytes to read
 *  @param stop    Stop to be generated after the transfer is done
 *  @return Number of read bytes
 */
int i2c_read(i2c_t *obj, int address, char *data, int length, int stop)
{
//  obj->slave_addr = address;
    obj->ops->setaddress(obj, address, 0);
    return obj->ops->read(obj, data, length, stop);
}

/** Blocking sending data
 *
 *  @param obj     The I2C object
 *  @param address 7-bit address (last bit is 0)
 *  @param data    The buffer for sending
 *  @param length  Number of bytes to write
 *  @param stop    Stop to be generated after the transfer is done
 *  @return
 *      zero or non-zero - Number of written bytes
 *      negative - I2C_ERROR_XXX status
 */
int i2c_write(i2c_t *obj, int address, const char *data, int length, int stop)
{
//  obj->slave_addr = address;
    obj->ops->setaddress(obj, address, 0);
    return obj->ops->write(obj, data, length, stop);
}

/** Reset I2C peripheral. TODO: The action here. Most of the implementation sends stop()
 *
 *  @param obj The I2C object
 */
void i2c_reset(i2c_t *obj)
{
    i2c_stop(obj);
}

/** Read one byte
 *
 *  @param obj The I2C object
 *  @param last Acknoledge
 *  @return The read byte
 */
int i2c_byte_read(i2c_t *obj, int last)
{
    return obj->ops->byteread(obj, last);
}

/** Write one byte
 *
 *  @param obj The I2C object
 *  @param data Byte to be written
 *  @return 0 if NAK was received, 1 if ACK was received, 2 for timeout.
 */
int i2c_byte_write(i2c_t *obj, int data)
{
    return obj->ops->bytewrite(obj, data);
}

/** Get the pins that support I2C SDA
 *
 * Return a PinMap array of pins that support I2C SDA in
 * master mode. The array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *i2c_master_sda_pinmap(void)
{
    return PinMap_I2C_SDA;
}

/** Get the pins that support I2C SCL
 *
 * Return a PinMap array of pins that support I2C SCL in
 * master mode. The array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *i2c_master_scl_pinmap(void)
{
    return PinMap_I2C_SCL;
}

/** Get the pins that support I2C SDA
 *
 * Return a PinMap array of pins that support I2C SDA in
 * slave mode. The array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *i2c_slave_sda_pinmap(void)
{
    return PinMap_I2C_SDA;
}

/** Get the pins that support I2C SCL
 *
 * Return a PinMap array of pins that support I2C SCL in
 * slave mode. The array is terminated with {NC, NC, 0}.
 *
 * @return PinMap array
 */
const PinMap *i2c_slave_scl_pinmap(void)
{
    return PinMap_I2C_SCL;
}

/**@}*/

#if DEVICE_I2CSLAVE
/**
 * \defgroup SynchI2C Synchronous I2C Hardware Abstraction Layer for slave
 * @{
 */

/** Configure I2C as slave or master.
 *  @param obj The I2C object
 *  @param enable_slave Enable i2c hardware so you can receive events with ::i2c_slave_receive
 *  @return non-zero if a value is available
 */
void i2c_slave_mode(i2c_t *obj, int enable_slave)
{
}

/** Check to see if the I2C slave has been addressed.
 *  @param obj The I2C object
 *  @return The status - 1 - read addresses, 2 - write to all slaves,
 *         3 write addressed, 0 - the slave has not been addressed
 */
int i2c_slave_receive(i2c_t *obj)
{
    return 0;
}

/** Configure I2C as slave or master.
 *  @param obj The I2C object
 *  @param data    The buffer for receiving
 *  @param length  Number of bytes to read
 *  @return non-zero if a value is available
 */
int i2c_slave_read(i2c_t *obj, char *data, int length)
{
    return 0;
}

/** Configure I2C as slave or master.
 *  @param obj The I2C object
 *  @param data    The buffer for sending
 *  @param length  Number of bytes to write
 *  @return non-zero if a value is available
 */
int i2c_slave_write(i2c_t *obj, const char *data, int length)
{
    return 0;
}

/** Configure I2C address.
 *  @param obj     The I2C object
 *  @param idx     Currently not used
 *  @param address The address to be set
 *  @param mask    Currently not used
 */
void i2c_slave_address(i2c_t *obj, int idx, uint32_t address, uint32_t mask)
{
}

#endif //DEVICE_I2CSLAVE

#endif // DEVICE_I2C
