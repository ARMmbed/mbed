Mbed OS's CMake build system supports a number of upload methods to help get code onto your device.  Some uploaders also support debugging, and for these CMake will help you by automatically running the needed commands to start a debugging session.

In order to enable uploading, you'll need to configure which upload methods your project supports.  To do this, you'll need to create an UploadMethodConfig.cmake file and include it in your top-level CMakeLists.txt.  You can start by using the template UploadMethodConfig-Sample.cmake (which is for Nucleo F429ZI) in this folder.  You'll then edit it to control the upload methods that are available for your target, and set the parameters that they need to operate.  THe different parameters and their values are explained in this document.

This page lists each upload method, its _parameters_ (variables that will have a fixed value) and its _options_ (variables that are different for different people and should be set on the command line when you run cmake).  Since this config is set by standard CMake code, it's very easy to add your own additional logic (including command line options) to adjust these variables!

Once you configure some upload methods, you can then run cmake with the `-DUPLOAD_METHOD=<method>` argument to enable uploading your code to the target.

To upload an executable, just run `make flash-xxx`, where xxx is replaced by the name of the executable target.  Don't forget that you can also use `make help` to see the list of all available targets.  Also, for Ninja users, just replace `make` with `ninja` in these commands.

To debug, first run `make gdbserver` in one terminal to start up the debug probe server.  Then, you can connect to the GDB server in your debug tool of choice, or run `make debug-xxx` in another terminal to start command-line GDB on the given program.

**All current upload methods (see below for details):**
|Name|CMake Argument|Supports Uploading|Supports Debugging|Upload Speed|Supported On
|---|---|---|---|---|---|
|No upload method|`-DUPLOAD_METHOD=NONE`|❌|❌|N/A|N/A
|MBed USB|`-DUPLOAD_METHOD=MBED`|✔️|❌|Fast|All Mbed Boards
|J-Link|`-DUPLOAD_METHOD=JLINK`|✔️|✔️|Fast|Mbed boards with J-Link On-Board.  Custom boards with a J-Link probe.
|pyOCD|`-DUPLOAD_METHOD=PYOCD`|✔️|✔️|Medium|Almost all Mbed boards.  Custom boards with an ST-Link or DAPLink probe.
|OpenOCD|`-DUPLOAD_METHOD=OPENOCD`|✔️|✔️|(Varies)|A number of different debug probes, but requires configuration.
|STM32Cube|`-DUPLOAD_METHOD=STM32CUBE`|✔️|✔️|Fast|All STMicroelectronics Mbed boards, custom boards with ST_Link probes.


### Common Parameters (for all methods)

> UPLOAD_METHOD_DEFAULT

**Type:** String

This sets the default upload method that CMake will use if one is not explicitly set.

> GDB_PORT

**Type:** Integer

This controls the port that GDB servers will be run on when debugging.  A value higher than 1024 is recommended to allow debugging without root on Linux/Mac.

## MBed USB Upload Method

This upload method interfaces with standard MBed boards which present themselves as USB drives.  The Mbed python tools are used to automatically locate and flash boards connected to the system.

### Parameters

> MBED_UPLOAD_ENABLED

**Type:** Bool

Whether the MBed upload method can be activated.

> MBED_RESET_BAUDRATE

**Type:** Integer

**Default:** 9600

On some boards, Mbed Tools has to connect to the board's serial port in order to reset them.  This configuration requires Mbed Tools to know the board rate the board is operating at (though you can also likely get away with setting a slower baud rate here than what's in use).

### Options

> MBED_TARGET_UID

**Type:** String

UID of the Mbed board to be programmed.  This is only needed if multiple of the same target are connected to your machine -- if it is not set, any one target will be flashed.  You can get a the list of UIDs from `python -m pyocd list`.

## J-Link Upload Method

This upload method connects to your processor via a J-Link JTAG box and the J-Link command line tools.  It supports both flashing and debugging.  CMake will automatically locate the J-Link tools in their standard install locations on Windows, Mac, and Linux.

### Parameters

> JLINK_UPLOAD_ENABLED

**Type:** Bool

Whether the J-Link upload method can be activated.

> JLINK_CPU_NAME

**Type:** String

The name that your processor is known by to J-link.  These are listed [here](https://www.segger.com/downloads/supported-devices.php).

> JLINK_CLOCK_SPEED

**Type:** Integer or String

Clock speed of the JTAG or SWD connection.  Accepts either a speed in kHz or "adaptive" to automatically determine speed using the RTCK pin.

## pyOCD

This upload method utilizes MBed's own [pyOCD](https://github.com/mbedmicro/pyOCD) application to flash and debug your processor.  pyOCD mainly supports the CMSIS-DAP, DAPLink, and STLink debug probes integrated into MBed dev boards, but can also use standalone DAP-based programmers and has experimental support for the J-Link probe.  Unlike all other debugging programs, pyOCD has the ability to recognize and display the threads that are currently running in MBed RTOS.  This makes it the most convenient debugging solution for many MBed applications.

Installation of pyOCD is usually as simple as `python3 -m pip install pyocd`, though on some platforms there are additional binary components that need to be installed for certain debug probes.

NOTE: Some older MBed boards will need to have their firmware updated to work with pyOCD.

### Parameters

> PYOCD_UPLOAD_ENABLED

**Type:** Bool

Whether the pyOCD upload method can be activated.

> PYOCD_TARGET_NAME

**Type:** String

Name of your processor as passed to the `-t` option of pyOCD.  This is usually the full or partial model number.

> PYOCD_CLOCK_SPEED

**Type:** Integer or String

Clock speed of the JTAG or SWD connection.  Default is in Hz, but can use k and M suffixes for MHz and GHz

### Options:

> PYOCD_PROBE_UID

**Type:** String

Probe UID to pass to pyOCD commands.  This selects which upload tool is used when multiple are connected to your computer.  You can get the UIDs from `python -m pyocd list`.

## OpenOCD

This upload method utilizes the [OpenOCD](http://openocd.org/) application to flash and debug your processor.  OpenOCD is highly configurable and supports a huge array of targets, from processors to flash memories to FPGAs.  However, this flexibility comes at a cost: it can be a bit of a pain to configure.  Using OpenOCD with your target requires you to select the appropriate configuration scripts from its vast array of included scripts, or to write your own if there is no appropriate option.  Once this is set up, OpenOCD can then be used as a GDB server and flash programmer.

OpenOCD can be installed through most distro package managers, and Windows binaries can be downloaded from [here](http://www.freddiechopin.info/en/download/category/4-openocd).  Once OpenOCD is installed to a standard location (extract the Windows version to Program Files), it should be automatically be detected by CMake.

### Parameters

> OPENOCD_UPLOAD_ENABLED

**Type:** Bool

Whether the OpenOCD upload method can be activated.

> OPENOCD_CHIP_CONFIG_COMMANDS

**Type:** List of Strings

This config option specifies all OpenOCD commands needed to configure the program for your target processor.  At minimum, this should include loading an interface config file and a target config file.  Since these options may need to access scripts in the OpenOCD install dir, CMake provides the variable `OpenOCD_SCRIPT_DIR` which will resolve to the scripts directory of OpenOCD on the current machine.

## STM32Cube

This uploader uses STMicroelectronics' official upload and debugging tools for its ST-LINK programmers.  The upload tool can be obtained from the standalone [STM32CubeProg package](https://www.st.com/en/development-tools/stm32cubeprog.html), but unfortunately the GDB server is only included in the [STM32Cube IDE](https://www.st.com/en/development-tools/stm32cubeide.html), which includes both programs (and also weighs in pretty large at 2.2GB).

In my testing, STM32Cube is at least 5 times faster than PyOCD at uploading code to the chip, so if you have a large program it might be worth taking the time to set up.  Also, its debugger seems to be considerably faster at things like setting breakpoints and single-stepping through code.

To use *STM32CubeProg only*, find the `STM32_Programmer_CLI` executable on your system and pass the `-DSTM32CubeProg_PATH=<path to STM32_Programmer_CLI>` argument to CMake to point to it.  To use the programmer and the debugger, you must first install STM32Cube IDE.  If you installed to the default install location, CMake should find it automatically.  If not, set the `STM32CUBE_IDE_PATH` variable to point to the IDE install dir, which CMake will use to find the other tools.  Note that on Macs this needs to point to the IDE dir inside the app package, e.g. `-DSTM32CUBE_IDE_PATH=/Applications/STM32CubeIDE.app/Contents/Eclipse`

### Parameters: 

> STM32CUBE_UPLOAD_ENABLED

**Type:** Bool

Whether the STM32Cube upload method can be activated.

> STM32CUBE_CONNECT_COMMAND

**Type:** List

"Connect" (-c) command to pass to the programmer to connect to your target device.  `port=SWD` should be all that's needed for most Mbed boards, but some also seem to need `reset=HWrst`. 

> STM32CUBE_GDBSERVER_ARGS

**Type:** List

Arguments to pass to the ST-Link gdbserver.  `--swd` should be all that's needed in most situations.

### Options:

> STM32CUBE_PROBE_SN

**Type:** String

Serial number of the ST-Link probe to connect to.  If blank, will connect to any probe.  You can get the list of serial numbers plugged into your machine with `STM32_Programmer_CLI -l`.


# Further Customization

These configurable options don't cover every single option that each uploader provides.  To customize the commands used further, you can create your own uploader CMake module for your needs.  First, copy one of the cmake scripts under `mbed-os/tools/cmake/upload_methods` to your own project (make sure that the location you add it to is on `CMAKE_MODULE_PATH`).  Then, give it a new name, and change all variables using the old name to use the new name (e.g. `UPLOAD_JLINK_FOUND` -> `UPLOAD_MYMETHOD_FOUND`).  Next, make the changes you need to the options and commands used, and add any needed configuration settings to your buildscript (including `UPLOAD_MYMETHOD_ENABLED`).  Finally, you can activate the new upload method by passing the name to CMake via `-DUPLOAD_METHOD=MYMETHOD`.