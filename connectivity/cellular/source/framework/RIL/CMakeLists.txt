# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

target_sources(mbed-cellular
    INTERFACE
        RIL_CellularContext.cpp
        RIL_CellularDevice.cpp
        RIL_CellularInformation.cpp
        RIL_CellularNetwork.cpp
        RILAdaptation.cpp
)