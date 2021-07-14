## CMake for Mbed OS

In order to [replace the traditional Mbed tools](https://os.mbed.com/blog/entry/Introducing-the-new-Mbed-Tools/), we are adding CMake support to allow the generation of build systems instructions to build Mbed OS as a library.

This is still at the experimental stage and still in development. It does not yet completely replace our current build tools. If you find a bug, please report it or raise a pull request as contributions are welcome!

Mbed OS is built as collection of all core libraries. Application project executables can be linked with the `mbed-os` library (which includes the core libraries of Mbed OS) and optional `mbed-os-` prefixed libraries depending on the additional features required by the projects and supported by the Mbed target.

The list of optional libraries for each component can be seen in the CMake input source file at the root of each component directory.

The following features are not yet supported and will be progressively added:
* Application profile selection (`bare metal`)
* Features that require altering compiler/linker command line options (except for printf library and C selections)

The full profile with the selected printf and C libraries.

### Supported targets

Only a limited set of targets is supported at the moment.

The following targets are supported:
- Ambiq Micro targets
- Analog Devices targets
- ARM FM targets
- ARM SSG targets
- Cypress targets
- Freescale targets
- GigaDevice targets
- MAXIM targets
- Nordic targets
- NXP targets
- Renesas targets
- Samsung targets
- Silicon Labs targets
- STM targets
- Toshiba targets

The post build operations are supported for the following Mbed boards:
LPC1114, LPC1768, ARCH_PRO, LPC54114,
LPC546XX, FF_LPC546XX, CY8CKIT064B0S2_4343W, CYTFM_064B0S2_4343W, CYSBSYSKIT_01,
ARM_MUSCA_B1, ARM_MUSCA_S1.

### Supported toolchains

Arm Compiler 6 and GNU Arm Embedded toolchains are supported.

### Examples

Supported examples can be identified by the presence of a top level `CMakeLists.txt` input source file.

### Known issues

- Ninja: ARMClang6 response files require unix paths on Windows. Reference: https://gitlab.kitware.com/cmake/cmake/-/issues/21093. We disabled response files for Ninja on Windows until CMake is fixed.

## How to build an application

Prerequisities:
- CMake >=3.19.0
- mbed-tools >=4.0.0
- Python modules defined in [`tools/cmake/requirements.txt`](./requirements.txt)

From the application root or wherever `mbed-os.lib` is found, run the following command to:
 * create the Mbed OS configuration CMake module
 * create a build directory
 * generate the project configuration
 * build the project using the `Ninja` build system

    ```
    mbedtools compile -m <mbed-target> -t <toolchain>
    ```

`CMAKE_BUILD_TYPE` can overridden with Mbed specific values: `Develop` (default value), `Release` and `Debug`.

`mbed-tools` will pass `-DCMAKE_BUILD_TYPE=<supported-build-type>` for you when using the `--build-type` optional argument of the `build` subcommand as follows:

```
mbedtools compile -m <mbed-target> -t <toolchain> -b <supported-build-type>
```

If you're running CMake directly, you may need to pass it in yourself as follows:
```
cmake -S <source-dir> -B <build-dir> -DCMAKE_BUILD_TYPE=debug
```

## How to build a greentea test

Install prerequisites suggested in the previous section and follow the below steps to build:
* Set your current directory to the test suite directory

* Run the following command for the configuration CMake module to be generated
  ```
  mbedtools configure -t <TOOLCHAIN> -m <MBED_TARGET> --mbed-os-path /path/to/mbed-os
  ```
* Build the test binary with the full profile
  ```
  cd cmake_build/<MBED_TARGET>/<PROFILE>/<TOOLCHAIN>/ && cmake ../../../.. -G Ninja && cmake --build .
  ```
  Or build the test binary with the baremetal profile
  ```
  cd cmake_build/<MBED_TARGET>/<PROFILE>/<TOOLCHAIN>/ && cmake ../../../.. -G Ninja -DMBED_GREENTEA_TEST_BAREMETAL=ON && cmake --build .
  ```

Notes:
* These steps will change when `mbedtools` implements a sub-command to invoke Greentea tests
* Some Greentea tests require specific application configuration files in order to build and run successfully. For example, the `connectivity/mbedtls/tests/TESTS/mbedtls/sanity` test requires the configuration file found at `TESTs/configs/experimental.json`.

## Naming convention

- `mbed` namespace: Mbed CMake targets must have `mbed-` prefix
- all characters are lower case
- words separated by hyphens

For example: `mbed-ble-cordio`
