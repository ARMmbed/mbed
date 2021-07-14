# mbed-OS development environment docker image

This docker image is the official mbed-OS development environment.
* It is based on ubuntu 20.04
* Python and cmake are installed
* arm-none-eabi-gcc toolchain is installed
* Latest released version of mbed-cli and GreenTea are installed
* All other mbed-OS dependency tools are installed.

# How to use docker image:

## Pull docker images
```bash
docker pull ghcr.io/ARMmbed/mbed-os-env

```

## Run mbed OS environment without HW support (build mbed images only)
launch docker by
```bash
docker run -it docker pull ghcr.io/ARMmbed/mbed-os-env:<label>
```
Then you will have the container with mbed OS development environment.
you should be able to compile mbed commands/examples as recommenced in mbed documentations
e.g.
```bash
mbed import mbed-os-example-blinky
cd mbed-os-example-blinky
mbed compile -m <TARGET> -t GCC_ARM
```

## Run mbed OS environment with HW support (USB pass-through)
If you want to use this docker container connect and flash your targets. you need some extra command line option to pass-through your USB devices.
```bash
sudo docker run -it --privileged -v /dev/disk/by-id:/dev/disk/by-id -v /dev/serial/by-id:/dev/serial/by-id ghcr.io/ARMmbed/mbed-os-env:<label>
```
Then you will have the container with mbed OS development environment.
To make sure your mbed targets been detected, you might want to manually run the mount command and `mbedls` to check
```bash
mount /dev/sdb /mnt
mbedls
```
if `mbedls` detected  your connected target, then you should be able to run mbed tests/examples as recommenced in mbed documentations
``` bash
mbed clone https://github.com/ARMmbed/mbed-os.git
cd mbed-os
mbed test -t GCC_ARM -m <target> 
```
