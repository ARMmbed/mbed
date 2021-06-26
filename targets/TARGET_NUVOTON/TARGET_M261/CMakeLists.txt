# Copyright (c) 2020-2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(${MBED_TOOLCHAIN} STREQUAL "GCC_ARM")
    set(LINKER_FILE device/TOOLCHAIN_GCC_ARM/M261.ld)
elseif(${MBED_TOOLCHAIN} STREQUAL "ARM")
    set(LINKER_FILE device/TOOLCHAIN_ARMC6/M261.sct)
endif()

add_library(mbed-m261 INTERFACE)

target_sources(mbed-m261
    INTERFACE
        analogin_api.c
        analogout_api.c
        device/startup_M261.c
        device/system_M261.c

        device/StdDriver/m261_acmp.c
        device/StdDriver/m261_bpwm.c
        device/StdDriver/m261_can.c
        device/StdDriver/m261_clk.c
        device/StdDriver/m261_crc.c
        device/StdDriver/m261_crypto.c
        device/StdDriver/m261_dac.c
        device/StdDriver/m261_eadc.c
        device/StdDriver/m261_ebi.c
        device/StdDriver/m261_ecap.c
        device/StdDriver/m261_epwm.c
        device/StdDriver/m261_fmc.c
        device/StdDriver/m261_gpio.c
        device/StdDriver/m261_i2c.c
        device/StdDriver/m261_i2s.c
        device/StdDriver/m261_pdma.c
        device/StdDriver/m261_qei.c
        device/StdDriver/m261_qspi.c
        device/StdDriver/m261_rtc.c
        device/StdDriver/m261_sc.c
        device/StdDriver/m261_scuart.c
        device/StdDriver/m261_sdh.c
        device/StdDriver/m261_spi.c
        device/StdDriver/m261_sys.c
        device/StdDriver/m261_timer.c
        device/StdDriver/m261_timer_pwm.c
        device/StdDriver/m261_uart.c
        device/StdDriver/m261_usbd.c
        device/StdDriver/m261_usci_i2c.c
        device/StdDriver/m261_usci_spi.c
        device/StdDriver/m261_usci_uart.c
        device/StdDriver/m261_wdt.c
        device/StdDriver/m261_wwdt.c


        dma_api.c
        flash_api.c
        gpio_api.c
        gpio_irq_api.c
        i2c_api.c
        lp_ticker.c
        mbed_overrides.c
        PeripheralPins.c
        pinmap.c
        port_api.c
        pwmout_api.c
        rtc_api.c
        serial_api.c
        sleep.c
        spi_api.c
        trng_api.cpp
        us_ticker.c
        watchdog_api.c

        crypto/crypto-misc.cpp
)

target_include_directories(mbed-m261
    INTERFACE
        .
        crypto
        device
        device/Reg
        device/StdDriver
)

target_link_libraries(mbed-m261 INTERFACE mbed-nuvoton mbed-flash-cmsis-algo)

mbed_set_linker_script(mbed-m261 ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_FILE})

add_library(mbed-numaker-iot-m263a INTERFACE)

target_include_directories(mbed-numaker-iot-m263a
    INTERFACE
        TARGET_NUMAKER_IOT_M263A
)

target_link_libraries(mbed-numaker-iot-m263a INTERFACE mbed-m261)
