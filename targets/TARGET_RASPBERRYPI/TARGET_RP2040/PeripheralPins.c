#include "pinmap.h"
#include "objects.h"
#include "PeripheralPins.h"

const PinMap PinMap_FULL[] = {
    {p0,  0, 0},
    {p1,  0, 0},
    {p2,  0, 0},
    {p3,  0, 0},
    {p4,  0, 0},
    {p5,  0, 0},
    {p6,  0, 0},
    {p7,  0, 0},
    {p8,  0, 0},
    {p9,  0, 0},
    {p10, 0, 0},
    {p11, 0, 0},
    {p12, 0, 0},
    {p13, 0, 0},
    {p14, 0, 0},
    {p15, 0, 0},
    {p16, 0, 0},
    {p17, 0, 0},
    {p18, 0, 0},
    {p19, 0, 0},
    {p20, 0, 0},
    {p21, 0, 0},
    {p22, 0, 0},
    {p23, 0, 0},
    {p24, 0, 0},
    {p25, 0, 0},
    {p26, 0, 0},
    {p27, 0, 0},
    {p28, 0, 0},
    {p29, 0, 0},
    {NC, NC, 0}
};


/************UART***************/
const PinMap PinMap_UART_TX[] = {
    {p0,  UART_0, (uint32_t) uart0},
    {p4,  UART_1, (uint32_t) uart1},
    {p8,  UART_1, (uint32_t) uart1},
    {p12, UART_0, (uint32_t) uart0},
    {p16, UART_0, (uint32_t) uart0},
    {p20, UART_1, (uint32_t) uart1},
    {p24, UART_1, (uint32_t) uart1},
    {p28, UART_0, (uint32_t) uart0},
    {NC, NC, 0}
};

const PinMap PinMap_UART_RX[] = {
    {p1,  UART_0, (uint32_t) uart0},
    {p5,  UART_1, (uint32_t) uart1},
    {p9,  UART_1, (uint32_t) uart1},
    {p13, UART_0, (uint32_t) uart0},
    {p17, UART_0, (uint32_t) uart0},
    {p21, UART_1, (uint32_t) uart1},
    {p25, UART_1, (uint32_t) uart1},
    {p29, UART_0, (uint32_t) uart0},
    {NC, NC, 0}
};

const PinMap PinMap_UART_CTS[] = {
    {p2,  UART_0, (uint32_t) uart0},
    {p6,  UART_1, (uint32_t) uart1},
    {p10, UART_1, (uint32_t) uart1},
    {p14, UART_0, (uint32_t) uart0},
    {p18, UART_0, (uint32_t) uart0},
    {p22, UART_1, (uint32_t) uart1},
    {p26, UART_1, (uint32_t) uart1},
    {NC, NC, 0}
};

const PinMap PinMap_UART_RTS[] = {
    {p3,  UART_0, (uint32_t) uart0},
    {p7,  UART_1, (uint32_t) uart1},
    {p11, UART_1, (uint32_t) uart1},
    {p15, UART_0, (uint32_t) uart0},
    {p19, UART_0, (uint32_t) uart0},
    {p23, UART_1, (uint32_t) uart1},
    {p27, UART_1, (uint32_t) uart1},
    {NC, NC, 0}
};

/************PWM***************/
const PinMap PinMap_PWM_OUT[] = {
    {p0,  PWM_0, 0},
    {p1,  PWM_0, 0},
    {p2,  PWM_1, 0},
    {p3,  PWM_1, 0},
    {p4,  PWM_2, 0},
    {p5,  PWM_2, 0},
    {p6,  PWM_3, 0},
    {p7,  PWM_3, 0},
    {p8,  PWM_4, 0},
    {p9,  PWM_4, 0},
    {p10, PWM_5, 0},
    {p11, PWM_5, 0},
    {p12, PWM_6, 0},
    {p13, PWM_6, 0},
    {p14, PWM_7, 0},
    {p15, PWM_7, 0},
    {p16, PWM_0, 0},
    {p17, PWM_0, 0},
    {p18, PWM_1, 0},
    {p19, PWM_1, 0},
    {p20, PWM_2, 0},
    {p21, PWM_2, 0},
    {p22, PWM_3, 0},
    {p23, PWM_3, 0},
    {p24, PWM_4, 0},
    {p25, PWM_4, 0},
    {p26, PWM_5, 0},
    {p27, PWM_5, 0},
    {p28, PWM_6, 0},
    {p29, PWM_6, 0},
    {NC, NC, 0}
};

/************SPI***************/
const PinMap PinMap_SPI_MISO[] = {
    {p0,  SPI_0, (uint32_t) spi0},
    {p4,  SPI_0, (uint32_t) spi0},
    {p8,  SPI_1, (uint32_t) spi1},
    {p12, SPI_1, (uint32_t) spi1},
    {p16, SPI_0, (uint32_t) spi0},
    {p20, SPI_0, (uint32_t) spi0},
    {p24, SPI_1, (uint32_t) spi1},
    {p28, SPI_1, (uint32_t) spi1},
    {NC, NC, 0}
};

const PinMap PinMap_SPI_SSEL[] = {
    {p1,  SPI_0, (uint32_t) spi0},
    {p5,  SPI_0, (uint32_t) spi0},
    {p9,  SPI_1, (uint32_t) spi1},
    {p13, SPI_1, (uint32_t) spi1},
    {p17, SPI_0, (uint32_t) spi0},
    {p21, SPI_0, (uint32_t) spi0},
    {p25, SPI_1, (uint32_t) spi1},
    {p29, SPI_1, (uint32_t) spi1},
    {NC, NC, 0}
};

const PinMap PinMap_SPI_SCLK[] = {
    {p2,  SPI_0, (uint32_t) spi0},
    {p6,  SPI_0, (uint32_t) spi0},
    {p10, SPI_1, (uint32_t) spi1},
    {p14, SPI_1, (uint32_t) spi1},
    {p18, SPI_0, (uint32_t) spi0},
    {p22, SPI_0, (uint32_t) spi0},
    {p26, SPI_1, (uint32_t) spi1},
    {NC, NC, 0}
};

const PinMap PinMap_SPI_MOSI[] = {
    {p3,  SPI_0, (uint32_t) spi0},
    {p7,  SPI_0, (uint32_t) spi0},
    {p11, SPI_1, (uint32_t) spi1},
    {p15, SPI_1, (uint32_t) spi1},
    {p19, SPI_0, (uint32_t) spi0},
    {p23, SPI_0, (uint32_t) spi0},
    {p27, SPI_1, (uint32_t) spi1},
    {NC, NC, 0}
};

/************I2C***************/
const PinMap PinMap_I2C_SDA[] = {
    {p0,  I2C_0, (uint32_t) i2c0},
    {p2,  I2C_1, (uint32_t) i2c1},
    {p4,  I2C_0, (uint32_t) i2c0},
    {p6,  I2C_1, (uint32_t) i2c1},
    {p8,  I2C_0, (uint32_t) i2c0},
    {p10, I2C_1, (uint32_t) i2c1},
    {p12, I2C_0, (uint32_t) i2c0},
    {p14, I2C_1, (uint32_t) i2c1},
    {p16, I2C_0, (uint32_t) i2c0},
    {p18, I2C_1, (uint32_t) i2c1},
    {p20, I2C_0, (uint32_t) i2c0},
    {p22, I2C_1, (uint32_t) i2c1},
    {p24, I2C_0, (uint32_t) i2c0},
    {p26, I2C_1, (uint32_t) i2c1},
    {p28, I2C_0, (uint32_t) i2c0},
    {NC, NC, 0}
};

const PinMap PinMap_I2C_SCL[] = {
    {p1,  I2C_0, (uint32_t) i2c0},
    {p3,  I2C_1, (uint32_t) i2c1},
    {p5,  I2C_0, (uint32_t) i2c0},
    {p7,  I2C_1, (uint32_t) i2c1},
    {p9,  I2C_0, (uint32_t) i2c0},
    {p11, I2C_1, (uint32_t) i2c1},
    {p13, I2C_0, (uint32_t) i2c0},
    {p15, I2C_1, (uint32_t) i2c1},
    {p17, I2C_0, (uint32_t) i2c0},
    {p19, I2C_1, (uint32_t) i2c1},
    {p21, I2C_0, (uint32_t) i2c0},
    {p23, I2C_1, (uint32_t) i2c1},
    {p25, I2C_0, (uint32_t) i2c0},
    {p27, I2C_1, (uint32_t) i2c1},
    {p29, I2C_0, (uint32_t) i2c0},
    {NC, NC, 0}
};

/************ADC***************/
/* ADC inputs 0-3 are GPIOs 26-29, ADC input 4
 * is the onboard temperature sensor.
 */
const PinMap PinMap_ADC[] = {
    { A0,       ADC0, 0},
    { A1,       ADC0, 1},
    { A2,       ADC0, 2},
    { A3,       ADC0, 3},
    { ADC_TEMP, ADC0, 4},
    { NC,       NC,   0}
};

