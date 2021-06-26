* [STM32WB family](#stm32wb-family)
* [Supported boards](#supported-boards)
   * [NUCLEO_WB55RG (NUCLEO-WB55RG)](#nucleo_wb55rg-nucleo-wb55rg)
   * [DISCO_WB5MMG (STM32WB5MM-DK)](#disco_wb5mmg-stm32wb5mm-dk)
* [BLE](#ble)
   * [MBED-OS support](#mbed-os-support)
   * [BLE FW](#ble-fw)
   * [BLE FW update](#ble-fw-update)
   * [BLE FW flashing procedure](#ble-fw-flashing-procedure)
   * [mbed-trace support](#mbed-trace-support)


# STM32WB family

[st.com STM32WB family page](https://www.st.com/en/microcontrollers-microprocessors/stm32wb-series.html)

This ST MCU family is dual-core : based on an Arm Cortex-M4 core and an Arm Cortex-M0+ core.

- M4 core is dedicated to application (mbed-os)
- M0 core is dedicated to BLE FW (only binary, source files not provided)

# Supported boards

## NUCLEO_WB55RG (NUCLEO-WB55RG)

[st.com STM32WB55RG MCU page](https://www.st.com/en/microcontrollers-microprocessors/stm32wb55rg.html)

[st.com NUCLEO board page](https://www.st.com/en/evaluation-tools/nucleo-wb55rg.html)

[st.com Pack board page](https://www.st.com/en/evaluation-tools/p-nucleo-wb55.html)

[mbed.com platform page](https://os.mbed.com/platforms/ST-Nucleo-WB55RG/)

- Total FLASH is 1MB

But FLASH is shared by M4 and M0 cores, [see BLE FW](#ble-fw)

- RAM: 256 KB
    - SRAM1: 192 KB
    - SRAM2a: 32 KB
    - SRAM2b: 32 KB

SRAM1 is dedicated for M4 core, and then for mbed-os applications.

SRAM2 is dedicated for M0 core and inter CPU communication, and then can not be addressed.


## DISCO_WB5MMG (STM32WB5MM-DK)

[st.com STM32WB5MMG module page](https://www.st.com/en/microcontrollers-microprocessors/stm32wb5mmg.html)

[st.com DISCO board page](https://www.st.com/en/evaluation-tools/stm32wb5mm-dk.html)

[mbed.com platform page](https://os.mbed.com/platforms/DISCO-WB5MMG/)

- Total FLASH is 1MB

But FLASH is shared by M4 and M0 cores, [see BLE FW](#ble-fw)

- RAM: 256 KB
    - SRAM1: 192 KB
    - SRAM2a: 32 KB
    - SRAM2b: 32 KB

SRAM1 is dedicated for M4 core, and then for mbed-os applications.

SRAM2 is dedicated for M0 core and inter CPU communication, and then can not be addressed.

NB: MBED CLI1 tool can be used thanks to this command:
```
mbedls -m 0884:DISCO_WB5MMG
```


## NUCLEO_WB15CC (NUCLEO-WB15CC)

[st.com STM32WB15CC module page](https://www.st.com/en/microcontrollers-microprocessors/stm32wb15cc.html)

[st.com NUCLEO board page]()

[mbed.com platform page](https://os.mbed.com/platforms/ST-NUCLEO-WB15CC/)

- Total FLASH is 320KB

But FLASH is shared by M4 and M0 cores, [see BLE FW](#ble-fw)

- RAM: 48 KB
    - SRAM1:  12 KB
    - SRAM2a: 32 KB
    - SRAM2b:  4 KB

SRAM1 is dedicated for M4 core, and then for mbed-os applications.

SRAM2 is dedicated for M0 core and inter CPU communication, and some part can not be addressed by M4.

NB: MBED CLI1 tool can be used thanks to this command:
```
mbedls -m 0883:NUCLEO_WB15CC
```


# BLE

## MBED-OS support

BLE feature is based on Cordio stack and API (above HCI) implementation.

Note that the BLE controller firmware running on the cortex-M0 is the same as in STcubeFW solution

- within mbed, the controller will run only the below-HCI part
- within STcubeFW (and STM32WPAN middleware), it can run also up to GATT / GAP layers ...

Official ST Application Note : 
[AN5289: Building wireless applications with STM32WB Series microcontrollers](https://www.st.com/resource/en/application_note/dm00598033-building-wireless-applications-with-stm32wb-series-microcontrollers-stmicroelectronics.pdf)

## BLE FW

All available BLE FW for M0 core are provided in ths ST STM32CubeWB repo:

### STM32WB5x

https://github.com/STMicroelectronics/STM32CubeWB/tree/master/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x

Default BLE FW in ST boards is **stm32wb5x_BLE_Stack_full_fw.bin**
- As explained in Release_Notes.html, this FW is flashed at @ 0x080CA000
- Default "mbed_rom_size" in targets.json is then "0xCA000" (808K)

To optimize FLASH size, **stm32wb5x_BLE_HCILayer_fw.bin** is supported for MBED-OS use case
- As explained in Release_Notes.html, this FW is flashed at @ 0x080E0000
- Then "mbed_rom_size" can be updated to "0xE0000" (896K)

Example in your local mbed_app.json:
```
    "target_overrides": {
        "NUCLEO_WB55RG": {
            "target.mbed_rom_size": "0xE0000"
        }
```

### STM32WB1x

https://github.com/STMicroelectronics/STM32CubeWB/tree/master/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB1x

Default BLE FW in ST boards is **stm32wb1x_BLE_Stack_full_fw.bin**
- **this is not supported in mbed**

It is mandatory to use **stm32wb1x_BLE_HCILayer_fw.bin**
- As explained in Release_Notes.html, this FW is flashed at @ 0x08032800
- Then "mbed_rom_size" is "0x32800" (202K) (default configuration in targets.json)


## BLE FW update

Official ST Application Note : 
[AN5185: ST firmware upgrade services for STM32WB Series](http://www.st.com/st-web-ui/static/active/en/resource/technical/document/application_note/DM00513965.pdf)


## BLE FW flashing procedure

STM32CubeProgrammer needs to be used:

https://www.st.com/en/development-tools/stm32cubeprog.html

Please check the Release Note and complete flashing procedure:
https://htmlpreview.github.io/?https://github.com/STMicroelectronics/STM32CubeWB/blob/master/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x/Release_Notes.html

- connect the board with ST-LINK
- In the left column, go to "Firmware Upgrade Services"
- "Start FUS"
- "Read FUS infos" => version v1.2.0 is expected
- Firmware Upgrade / "Browse" : select the chosen FW (see above)
- Firmware Upgrade / Start address : depends on the chosen FW (see above)
- Firmware Upgrade / "Firmware Upgrade"
- In the left column, go to "Option bytes"
- User Configuration => "Read"
- User Configuration / enable nSWBOOT0 => "Apply"


## mbed-trace support

trace group: BLWB

example:
````
[INFO][BLWB]: WIRELESS COPROCESSOR FW VERSION ID = 1.11.1
[INFO][BLWB]: WIRELESS COPROCESSOR FW STACK TYPE = 1
[DBG ][BLWB]: mbox_write type:1, len:3
[INFO][BLWB]: TX>> BLE CMD
[DBG ][BLWB]:   Type 0x1
[DBG ][BLWB]:    Cmd 0xc03
[DBG ][BLWB]:    Len 0D]
````
