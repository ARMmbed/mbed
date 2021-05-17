/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "mbed_assert.h"
#include "mbed_critical.h"
#include "spi_api.h"
#include "PeripheralPins.h"
#include "PeripheralNames.h"

/******************************************************************************
 * CONSTANT
 ******************************************************************************/

static unsigned int const SPI_MASTER_DEFAULT_BITRATE = 1000 * 1000; /* 1 MHz */

/******************************************************************************
 * FUNCTION DEFINITION
 ******************************************************************************/

void spi_init(spi_t *obj, PinName mosi, PinName miso, PinName sclk, PinName ssel)
{
    /* Check if all pins do in fact belong to the same SPI module. */
    SPIName spi_mosi = (SPIName)pinmap_peripheral(mosi, PinMap_SPI_MOSI);
    SPIName spi_miso = (SPIName)pinmap_peripheral(miso, PinMap_SPI_MISO);
    SPIName spi_sclk = (SPIName)pinmap_peripheral(sclk, PinMap_SPI_SCLK);
    SPIName spi_ssel = (SPIName)pinmap_peripheral(ssel, PinMap_SPI_SSEL);

    MBED_ASSERT(spi_mosi == spi_miso);
    MBED_ASSERT(spi_miso == spi_sclk);
    if (spi_ssel != (SPIName)NC) {
        MBED_ASSERT(spi_sclk == spi_ssel);
    }

    /* Obtain pointer to the SPI module. */
    obj->dev = (spi_inst_t *)pinmap_function(mosi, PinMap_SPI_MOSI);

    /* Configure GPIOs for SPI usage. */
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(sclk, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);

    /* Initialize SPI at 1 MHz bitrate */
    _spi_init(obj->dev, SPI_MASTER_DEFAULT_BITRATE);
}

void spi_format(spi_t *obj, int bits, int mode, int slave)
{
    /* Doing some parameter sanity checks. */
    MBED_ASSERT((bits >= 4) && (bits <= 16));
    MBED_ASSERT((mode >= 0) && (mode <= 3));

    /* Determine parameters for CPOL, CPHA */
    typedef struct
    {
        spi_cpol_t cpol;
        spi_cpha_t cpha;
    } spi_mode_t;
    spi_mode_t const SPI_MODE[4] =
    {
        {SPI_CPOL_0, SPI_CPHA_0}, /* MODE 0 */
        {SPI_CPOL_0, SPI_CPHA_1}, /* MODE 1 */
        {SPI_CPOL_1, SPI_CPHA_0}, /* MODE 2 */
        {SPI_CPOL_1, SPI_CPHA_1}  /* MODE 3 */
    };

    /* Configure the SPI. */
    spi_set_format(obj->dev, bits, SPI_MODE[mode].cpol, SPI_MODE[mode].cpha, SPI_MSB_FIRST);
    /* Set's the SPI up as slave if the value of slave is different from 0, e.g. a value of 1 or -1 set's this SPI up as a slave. */
    spi_set_slave(obj->dev, slave != 0);
}

void spi_frequency(spi_t *obj, int hz)
{
    spi_set_baudrate(obj->dev, hz);
}

int spi_master_write(spi_t *obj, int value)
{
    uint8_t rx;
    uint8_t const tx = (uint8_t)value;
    spi_write_read_blocking(obj->dev, &tx, &rx, sizeof(rx));
    return rx;
}

int spi_master_block_write(spi_t *obj, const char *tx_buffer, int tx_length, char *rx_buffer, int rx_length, char write_fill)
{
    /* The pico-sdk API does not support different length SPI buffers. */
    MBED_ASSERT(tx_length == rx_length);
    /* Perform the SPI transfer. */
    return spi_write_read_blocking(obj->dev, (const uint8_t *)tx_buffer, (uint8_t *)rx_buffer, (size_t)tx_length);
}

const PinMap *spi_master_mosi_pinmap()
{
    return PinMap_SPI_MOSI;
}

const PinMap *spi_master_miso_pinmap()
{
    return PinMap_SPI_MISO;
}

const PinMap *spi_master_clk_pinmap()
{
    return PinMap_SPI_SCLK;
}

const PinMap *spi_master_cs_pinmap()
{
    return PinMap_SPI_SSEL;
}
