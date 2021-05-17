#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "PeripheralNames.h"
#include "boards/pico.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    p0  = 0,
    p1  = 1,
    p2  = 2,
    p3  = 3,
    p4  = 4,
    p5  = 5,
    p6  = 6,
    p7  = 7,
    p8  = 8,
    p9  = 9,
    p10 = 10,
    p11 = 11,
    p12 = 12,
    p13 = 13,
    p14 = 14,
    p15 = 15,
    p16 = 16,
    p17 = 17,
    p18 = 18,
    p19 = 19,
    p20 = 20,
    p21 = 21,
    p22 = 22,
    p23 = 23,
    p24 = 24,
    p25 = 25,
    p26 = 26,
    p27 = 27,
    p28 = 28,
    p29 = 29,

    // ADC internal channels
    ADC_TEMP = 0xF0,
    ADC_VREF = 0xF1,

#ifndef ARDUINO_ARCH_MBED
    D0          = p1,
    D1          = p0,
    D2          = p25,
    D3          = p15,
    D4          = p16,
    D5          = p17,
    D6          = p18,
    D7          = p19,
    D8          = p20,
    D9          = p21,
    D10         = p5,
    D11         = p7,
    D12         = p4,
    D13         = p6,

    A0          = 26,
    A1          = 27,
    A2          = 28,
    A3          = 29,
#endif

    CONSOLE_TX = p0,
    CONSOLE_RX = p1,

    LED1 = p6,
    LED2 = p6,
    LED3 = p6,
    LED4 = p6,

    // Not connected
    NC = (int)0xFFFFFFFF
} PinName;

#ifdef __cplusplus
}
#endif

#endif
