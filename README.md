# Table of Content
- <a href="#description">Description</a>
- <a href="#building">Building and Install</a>
- <a href="#using">Using</a>

# Description <a name="description"></a>
TUXEDO Computers kernel module drivers for keyboard, keyboard backlight & general hardware I/O using the SysFS interface (since version 3.2.0)

Features
- Driver for Fn-keys
- SysFS control of brightness/color/mode for most TUXEDO keyboards
    - [https://docs.kernel.org/leds/leds-class.html](https://docs.kernel.org/leds/leds-class.html)
    - [https://docs.kernel.org/leds/leds-class-multicolor.html](https://docs.kernel.org/leds/leds-class-multicolor.html)
- Hardware I/O driver for TUXEDO Control Center

Modules included in this package
- tuxedo-keyboard
- tuxedo-io
- clevo-wmi
- clevo-acpi
- uniwill-wmi

# Building and Install <a name="building"></a>

## Dependencies:
- make
- gcc
- linux-headers
- dkms (Only when using this module with DKMS functionality)

## Warning when installing the module:

Use either method only. Do not combine installation methods, such as starting with the build step below and proceeding to use the same build artifacts with the DKMS module. Otherwise the module built via dkms will fail to load with an `exec_format` error on newer kernels due to a mismatched version magic.

This is why the DKMS build step begins with a `make clean` step. 

For convenience, on platforms where DKMS is in use, skip to the DKMS section directly.

## Clone the Git Repo:

```sh
git clone https://github.com/tuxedocomputers/tuxedo-keyboard.git

cd tuxedo-keyboard

git checkout release
```

## Build the Module:

```sh
make clean && make
```

## The DKMS route:

### Add as DKMS Module:

Install the Module:
```sh
make clean

sudo make dkmsinstall
```

Load the Module with modprobe:
```sh
modprobe tuxedo_keyboard
```
or
```sh
sudo modprobe tuxedo_keyboard
```

You might also want to activate `tuxedo_io` module the same way if you are using [TCC](https://github.com/tuxedocomputers/tuxedo-control-center).

### Uninstalling the DKMS module:

Remove the DKMS module and source:
```sh
sudo make dkmsremove

sudo rm /etc/modprobe.d/tuxedo_keyboard.conf
```

# Using <a name="using"></a>

## modprobe

```sh
modprobe tuxedo_keyboard
```

## Load the Module on boot:

If a module is relevant it will be loaded automatically on boot. If it is not loaded after a reboot, it most likely means that it is not needed.

Add Module to /etc/modules
```sh
sudo su

echo tuxedo_keyboard >> /etc/modules
```
