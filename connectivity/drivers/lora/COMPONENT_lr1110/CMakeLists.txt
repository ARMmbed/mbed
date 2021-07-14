# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

#add_subdirectory(src)

target_include_directories(mbed-lorawan
    INTERFACE
        .
)

target_sources(mbed-lorawan
	INTERFACE
        lr1110_bootloader.c
        lr1110_crypto_engine.c
		lr1110_driver_version.c
		lr1110_gnss.c 
		lr1110_hal.c 
		lr1110_radio.cpp
		lr1110_regmem.c 
		lr1110_system.c 
		lr1110_wifi.c 
		system.c 
		system_gpio.c 
		system_spi.c 
		system_time.c 
)
