# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0


if(${MBED_TOOLCHAIN} STREQUAL "ARM")

	find_library(MODEM_LIBRARY
		NAMES libSAMSUNG_S5JS100_Modem_Proxy.ar
		HINTS "./TOOLCHAIN_ARM_STD"
	)
elseif(${MBED_TOOLCHAIN} STREQUAL "GCC_ARM")
    find_library(MODEM_LIBRARY
		NAMES libSAMSUNG_S5JS100_Modem_Proxy.a
		HINTS "./TOOLCHAIN_GCC_ARM"
	)
endif()


target_include_directories(mbed-cellular
    INTERFACE
        ./RIL/include
        ./modem/include
)


target_sources(mbed-cellular
    INTERFACE
        modem/src/modem_io_device.cpp
        modem/src/modem_link_device_shmem.cpp
        modem/src/s5js100_mbox_ipc.cpp
        modem/src/shmem_save.cpp


        RIL/src/dcxo_update.cpp
        RIL/src/mbed_main_init.cpp
        RIL/src/ONBOARD_SAMSUNG_S5JS100_RIL.cpp
        RIL/src/SAMSUNG_RIL_stringutil.cpp
        RIL/src/SAMSUNG_S5JS100_L3IP_Driver.cpp
        RIL/src/SAMSUNG_S5JS100_RIL.cpp
        RIL/src/SAMSUNG_S5JS100_RIL_CellularContext.cpp
        RIL/src/SAMSUNG_S5JS100_RIL_CellularNetwork.cpp
        RIL/src/SAMSUNG_S5JS100_RIL_ControlPlane_netif.cpp
        RIL/src/SAMSUNG_S5JS100_RIL_IF.cpp
        RIL/src/SAMSUNG_S5JS100_RILAdaptation.cpp
        RIL/src/SAMSUNG_S5JS100_STK.cpp
        RIL/src/SAMSUNG_S5JS100_RIL_CellularInformation.cpp
)

target_link_libraries(mbed-cellular
	INTERFACE
		${MODEM_LIBRARY}
)