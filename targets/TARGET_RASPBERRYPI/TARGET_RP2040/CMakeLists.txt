# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(TARGET_NANO_RP2040_CONNECT EXCLUDE_FROM_ALL)
add_subdirectory(TARGET_RASPBERRY_PI_PICO EXCLUDE_FROM_ALL)

add_library(mbed-rp2040 INTERFACE)

target_include_directories(mbed-rp2040
    INTERFACE
        pico-sdk/rp2_common/hardware_adc/include
        pico-sdk/rp2_common/hardware_gpio/include
        pico-sdk/rp2_common/hardware_resets/include
        pico-sdk/rp2_common/hardware_pwm/include
        pico-sdk/rp2_common/hardware_base/include
        pico-sdk/rp2_common/hardware_uart/include
        pico-sdk/rp2_common/hardware_spi/include
        pico-sdk/rp2_common/hardware_i2c/include
        pico-sdk/rp2_common/hardware_irq/include
        pico-sdk/rp2_common/hardware_flash/include
        pico-sdk/rp2_common/hardware_clocks/include
        pico-sdk/rp2_common/hardware_rtc/include
        pico-sdk/rp2_common/hardware_watchdog/include
        pico-sdk/rp2_common/hardware_timer/include
        pico-sdk/rp2_common/hardware_pll/include
        pico-sdk/rp2_common/hardware_sync/include
        pico-sdk/rp2_common/hardware_xosc/include
        pico-sdk/rp2_common/pico_platform/include
        pico-sdk/rp2_common/pico_fix/rp2040_usb_device_enumeration/include/pico/fix
        pico-sdk/rp2_common/pico_bootrom/include
        pico-sdk/rp2_common/hardware_claim/include
        pico-sdk/rp2040/hardware_structs/include
        pico-sdk/rp2040/hardware_regs/include
        pico-sdk/common/pico_sync/include
        pico-sdk/common/pico_time/include
        pico-sdk/common/pico_base/include
        pico-sdk/common/pico_binary_info/include
        pico-sdk/common/pico_util/include
        pico-sdk/boards/include
        pico-sdk/generated
        .
)

target_sources(mbed-rp2040
    INTERFACE
        analogin_api.c
        flash_api.c
        gpio_api.c
        i2c_api.c
        lp_ticker.c
        mbed_overrides.c
        PeripheralPins.c
        pwmout_api.c
        reset_reason.c
        serial_api.c
        spi_api.c
        us_ticker.c
        USBPhy_RP2040.cpp
        watchdog_api.c
        pico-sdk/rp2_common/pico_standard_link/crt0.S
        pico-sdk/rp2_common/hardware_flash/flash.c
        pico-sdk/rp2_common/hardware_uart/uart.c
        pico-sdk/rp2_common/hardware_spi/spi.c
        pico-sdk/rp2_common/hardware_gpio/gpio.c
        pico-sdk/rp2_common/hardware_xosc/xosc.c
        pico-sdk/rp2_common/hardware_irq/irq.c
        pico-sdk/rp2_common/hardware_pll/pll.c
        pico-sdk/rp2_common/hardware_watchdog/watchdog.c
        pico-sdk/rp2_common/hardware_clocks/clocks.c
        pico-sdk/rp2_common/hardware_sync/sync.c
        pico-sdk/rp2_common/pico_bootrom/bootrom.c
        pico-sdk/rp2_common/pico_runtime/runtime.c
        pico-sdk/rp2_common/pico_platform/platform.c
        pico-sdk/common/pico_sync/mutex.c
        pico-sdk/common/pico_time/time.c
        pico-sdk/common/pico_sync/lock_core.c
)

target_link_libraries(mbed-rp2040 INTERFACE mbed-raspberrypi)
