# Directory Structure
The directory is organised as follows:

```
+--TARGET_S5JS100
|   +-- README.md                      # The present markdown file
|
|   +-- modem/                         # The S111 NB network register logic
|
|   +-- RIL/                           # Adapt RIL interface for S111 target
|
|   +-- TOOLCHAIN_ARM_STD/             # S111 NB network data processing logic in ARM toolchain
|           +-- libSAMSUNG_S5JS100_Modem_Proxy.ar       # The static library for the ARM toolchain
|
|   +-- TOOLCHAIN_GCC_ARM/             # S111 NB network data processing logic in GCC_ARM toolchain
|           +-- libSAMSUNG_S5JS100_Modem_Proxy.a       # The static library for the GCC_ARM toolchain
|
|   +-- TOOLCHAIN_IAR/                 # S111 NB network data processing logic in IAR toolchain
|           +-- libSAMSUNG_S5JS100_Modem_Proxy.a       # The static library for the IAR toolchain
|
|   +-- mbed_lib.json                   # Macro config
|
|   +-- CMakeLists.txt                  # CMake file
```

**Note:**  
>>The version of toochain that generates the Modem Proxy static library is as follows:  
>>>* The ARM toolchain version is 6.3.1  
>>>* The GCC_ARM toolchain version is 6.12  
>>>* The IAR toolchain version is 8.32  