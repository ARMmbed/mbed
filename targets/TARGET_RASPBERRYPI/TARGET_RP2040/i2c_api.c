/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "mbed_assert.h"
#include "i2c_api.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "objects.h"
#include "stdio.h"

/******************************************************************************
 * DEFINE
 ******************************************************************************/

#if 0
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#define NoData         0 // the slave has not been addressed
#define ReadAddressed  1 // the master has requested a read from this slave (slave = transmitter)
#define WriteGeneral   2 // the master is writing to all slave
#define WriteAddressed 3 // the master is writing to this slave (slave = receiver)

/******************************************************************************
 * CONST
 ******************************************************************************/

static unsigned int const DEFAULT_I2C_BAUDRATE = 100 * 1000; /* 100 kHz */

/******************************************************************************
 * FUNCTION DEFINITION
 ******************************************************************************/

void i2c_init(i2c_t *obj, PinName sda, PinName scl)
{
    /* Verify if both pins belong to the same I2C peripheral. */
    I2CName const i2c_sda = (I2CName)pinmap_peripheral(sda, PinMap_I2C_SDA);
    I2CName const i2c_scl = (I2CName)pinmap_peripheral(scl, PinMap_I2C_SCL);
    MBED_ASSERT(i2c_sda == i2c_scl);

#if DEVICE_I2CSLAVE
    /** was_slave is used to decide which driver call we need
     * to use when uninitializing a given instance
     */
    obj->was_slave = false;
    obj->is_slave = false;
    obj->slave_addr = 0;
#endif

    /* Obtain the pointer to the I2C hardware instance. */
    obj->dev = (i2c_inst_t *)pinmap_function(sda, PinMap_I2C_SDA);
    //obj->baudrate = DEFAULT_I2C_BAUDRATE;
    //Call this function because if we are configuring a slave, we don't have to set the frequency
    //i2c_frequency(obj->dev, DEFAULT_I2C_BAUDRATE);

    /* Initialize the I2C module. */
    _i2c_init(obj->dev, DEFAULT_I2C_BAUDRATE);

    /* Configure GPIO for I2C as alternate function. */
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

    /* Enable pull-ups for I2C pins. */
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

void i2c_frequency(i2c_t *obj, int hz)
{
    DEBUG_PRINTF("obj->is_slave: %d\r\n", obj->is_slave);

#if DEVICE_I2CSLAVE
    /* Slaves automatically get frequency from master */
    if(obj->is_slave) {
    		return;
    }
#endif
    obj->baudrate = i2c_set_baudrate(obj->dev, hz);
}

int i2c_read(i2c_t *obj, int address, char *data, int length, int stop)
{
    int const bytes_read = i2c_read_blocking(obj->dev,
                                             (uint8_t)(address >> 1),
                                             (uint8_t *)data,
                                             (size_t)length,
                                             /* nostop = */(stop == 0));
    if (bytes_read < 0)
        return I2C_ERROR_NO_SLAVE;
    else
        return bytes_read;
}

int i2c_write(i2c_t *obj, int address, const char *data, int length, int stop)
{
    if (length == 0) {
        // From pico-sdk:
        // static int i2c_write_blocking_internal(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
        // Synopsys hw accepts start/stop flags alongside data items in the same
        // FIFO word, so no 0 byte transfers.
        // invalid_params_if(I2C, len == 0);
        length = 1;
    }

    int const bytes_written = i2c_write_blocking(obj->dev,
                                                 address >> 1,
                                                 (const uint8_t *)data,
                                                 (size_t)length,
                                                 /* nostop = */(stop == 0));
    if (bytes_written < 0)
        return I2C_ERROR_NO_SLAVE;
    else
        return bytes_written;
}

void i2c_reset(i2c_t *obj)
{
    i2c_deinit(obj->dev);
    _i2c_init(obj->dev, obj->baudrate);
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

int i2c_stop(i2c_t *obj)
{

}

#if DEVICE_I2CSLAVE

/** Configure I2C as slave or master.
 *  @param obj The I2C object
 *  @param enable_slave Enable i2c hardware so you can receive events with ::i2c_slave_receive
 *  @return non-zero if a value is available
 */
void i2c_slave_mode(i2c_t *obj, int enable_slave)
{
    DEBUG_PRINTF("i2c_slave_mode: %p, %d\r\n", obj, enable_slave);

    obj->is_slave = enable_slave;
}

/** Check to see if the I2C slave has been addressed.
 *  @param obj The I2C object
 *  @return The status - 1 - read addressed, 2 - write to all slaves,
 *         3 write addressed, 0 - the slave has not been addressed
 */
int i2c_slave_receive(i2c_t *obj)
{
    int retValue = NoData;

    int rd_req = (obj->dev->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_RD_REQ_BITS) >> 5;

    if (rd_req == I2C_IC_RAW_INTR_STAT_RD_REQ_VALUE_ACTIVE) {
        DEBUG_PRINTF("Read addressed\r\n");
        return ReadAddressed;
    }

    int wr_req = (obj->dev->hw->status & I2C_IC_STATUS_RFNE_BITS) >> 3;

    if (wr_req == I2C_IC_STATUS_RFNE_VALUE_NOT_EMPTY) {
        DEBUG_PRINTF("Write addressed\r\n");
        return WriteAddressed;
    }

    return (retValue);
}

/** Configure I2C as slave or master.
 *  @param obj The I2C objecti2c_get_read_availableread
 *  @return non-zero if a value is available
 */
int i2c_slave_read(i2c_t *obj, char *data, int length)
{
    size_t read_len = i2c_read_raw_blocking(obj->dev, (uint8_t *)data, length);

    DEBUG_PRINTF("i2c_slave read %d bytes\r\n", read_len);

    return read_len;
}

/** Configure I2C as slave or master.
 *  @param obj The I2C object
 *  @param data    The buffer for sending
 *  @param length  Number of bytes to write
 *  @return non-zero if a value is available
 */
int i2c_slave_write(i2c_t *obj, const char *data, int length)
{
    DEBUG_PRINTF("i2c_slave_write\r\n");

    i2c_write_raw_blocking(obj->dev, (const uint8_t *)data, (size_t)length);

    //Clear interrupt
    int clear_read_req = i2c_get_hw(obj->dev)->clr_rd_req;
    DEBUG_PRINTF("clear_read_req: %d\n", clear_read_req);

    return length;
}

/** Configure I2C address.
 *  @param obj     The I2C object
 *  @param idx     Currently not used
 *  @param address The address to be set
 *  @param mask    Currently not used
 */
void i2c_slave_address(i2c_t *obj, int idx, uint32_t address, uint32_t mask)
{
    if (obj->is_slave) {
        DEBUG_PRINTF("i2c_slave_address: %p, %d, %d, %d\r\n", obj, idx, address, mask);

        obj->slave_addr = (uint8_t)(address >> 1);
        i2c_set_slave_mode(obj->dev, true, obj->slave_addr);
    }
}

#endif // DEVICE_I2CSLAVE
