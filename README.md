# Table of Content
- <a href="#description">Description</a>
- <a href="#building">Building and Install</a>
- <a href="#install">Install</a>
- <a href="#using">Using</a>
- <a href="#sysfs">Sysfs</a>
- <a href="#kernelparam">Kernel Parameter</a>
- <a href="#modes">Modes</a>

# Description <a name="description"></a>
TUXEDO Computers Kernel module for keyboard backlighting.

Additions
- Sysfs interface to control the brightness, mode, color, on/off state
- DKMS Ready
- Full RGB Color Support

# Building and Install <a name="building"></a>

## Dependencies
- make
- gcc
- linux-headers
- dkms (Only when use this module with the DKMS functionality)

## Build the Module

```sh
make clean && make
```

## dkms

### Add as DKMS Module

At first point add the Module
```sh
make clean
sudo cp -R . /usr/src/tuxedo_keyboard-1
sudo dkms add -m tuxedo_keyboard -v 1
```

The secound step is compile the module
```sh
sudo dkms build -m tuxedo_keyboard -v 1
```

Install the DKMS module
```sh
sudo dkms install -m tuxedo_keyboard -v 1
```

Load the module with modprobe
```sh
modprobe tuxedo_keyboard
```

### Remove DKMS Module

Remove the dkms module
```sh
sudo dkms remove -m tuxedo_keyboard -v 1 --all
```

Remove the source
```sh
sudo rm -rf /usr/src/tuxedo_keyboard-1
```

# Using <a name="using"></a>

## modprobe

```sh
modprobe tuxedo_keyboard
```

## Load at Boot-Up

Add Module to /etc/modules
```sh
sudo su
echo tuxedo_keyboard >> /etc/modules
```

Default Parameters at start.
In this example we start the kernel module with 
- mode 0 (Custom / Default Mode)
- red color for the left side of keyboard 
- green color for the center of keyboard 
- blue color for the right side of keyboard 

```sh
sudo su
echo "options tuxedo_keyboard mode=0 color_left=0xFF0000 color_center=0x00FF00 color_right=0x0000FF" > /etc/modprobe.d/tuxedo_keyboard.conf
```

# Sysfs <a name="sysfs"></a>

## General
Path: /sys/devices/platform/tuxedo_keyboard

## color_left
Allowed Values: Hex-Value (e.g. 0xFF0000 for the Color Red)   
Description: Set the color of the left Keyboard Side

## color_center
Allowed Values: Hex-Value (e.g. 0xFF0000 for the Color Red)   
Description: Set the color of the center of Keyboard

## color_right
Allowed Values: Hex-Value (e.g. 0xFF0000 for the Color Red)   
Description: Set the color of the right Keyboard Side

## color_extra
Allowed Values: Hex-Value (e.g. 0xFF0000 for the Color Red)   
Description: Set the color of the extra region (if exist) of the Keyboard

## brightness
Allowed Values: 0 - 255   
Description: Set the brightness of the Keyboard

## mode
Allowed Values: 0 - 7   
Description: Set the mode of the Keyboard. A list with the modes is under <a href="#modes">Modes</a>

## state
Allowed Values: 0, 1   
Description: Set the State of keyboard, 0 is keyboard is off and 1 is keyboard is on

## extra
Allowed Values: 0, 1   
Description: Only get the information, if the keyboard have the extra region

# Kernel Parameter <a name="kernelparam"></a>

## Using
```sh
sudo modprobe tuxedo_keyboard <params>
```

## color_left
Set the color of the left Keyboard Side

## color_center
Set the color of the left Keyboard Side

## color_right
Set the color of the left Keyboard Side

## color_extra
Set the color of the left Keyboard extra region (Only when is a supported keyboard)

## mode
Set the mode (on/off) of keyboard

## brightness
Set the brightness of keyboard

## state

# Modes <a name="modes"></a>

## CUSTOM

## BREATHE

## CYCLE

## DANCE

## FLASH

## RANDOM_COLOR

## TEMPO

## WAVE