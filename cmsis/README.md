
# CMSIS Version 5


This directory contains a ported version of the upstream repository for [CMSIS_5](http://arm-software.github.io/CMSIS_5/General/html/index.html) as well as Mbed specific RTX configuration and RTOS boot source files.

The upstream source code is imported using the Python script located at [`tools/importer/importer.py`](../tools/importer/importer.py) when passed the [`tools/importer/cmsis_importer.json`](../tools/importer/cmsis_importer.json) configuration file.
For more information on the importer script please read [`tools/importer/README.md`](../tools/importer/README.md).

The directory is organised as follows:

```
+--cmsis
|   +-- README.md                           # The present mardown file
|   +-- device/
|       +-- mbed_cmsis_conf.h               # Process stack configuration
|       +-- RTE/
|           +-- include/
|               +-- RTE_Components.h        # Run-Time-Environment Component Configuration File
|       +-- rtos/
|           +-- mbed_lib.json               # Mbed library configuration file
|           +-- include/
|               +-- mbed_boot.h             # Declares the functions that run before main()
|               +-- mbed_rtx_conf.h         # Changes to RTX configuration
|               +-- mbed_rtx_storage.h      # Declares the primitives storage types for RTX
|           +-- source/
|               +-- mbed_boot.c             # Implements some of the functions that run before main()
|               +-- mbed_rtos_rtx.c         # Implements some of the functions that run before main()
|               +-- mbed_rtx_handlers.c     # Implements RTX handlers
|               +-- mbed_rtx_idle.cpp       # Implements RTX idle loop
|           +-- TOOLCHAIN_ARM_MICRO/
|               +-- mbed_boot_arm_micro.c   # Mbed entry point for the uARM toolchain
|           +-- TOOLCHAIN_ARM_STD/
|               +-- mbed_boot_arm_std.c     # Mbed entry point for the ARM toolchain
|           +-- TOOLCHAIN_GCC_ARM/
|               +-- mbed_boot_gcc_arm.c     # Mbed entry point for the GCC_ARM toolchain
|           +-- TOOLCHAIN_IAR/
|               +-- mbed_boot_iar.c         # Mbed entry point for the IAR toolchain
|   +-- CMSIS_5/                            # Imported from the upstream repository as described in the importer script configuration file (cmsis_importer.json)
```

Two Mbed libraries live within this directory:
* `rtos` from `device/rtos`
* `cmsis-cmsis5-rtos2` from `CMSIS_5/CMSIS/RTOS2`
