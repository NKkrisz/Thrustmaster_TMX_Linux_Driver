# T150 Force Feedback Wheel Linux drivers
**DISCLAMER**
*This is not an official driver from Thrustmaster and is provided without any kind of warranty. Loading and using this driver is at your own risk; I don't take responsibility for kernel panics, devices bricked or any other kind of inconvenience*

## Project status
At the moment the `t150` module has memory leaks in case of errors in the `t150_probe` function

### What's working 👌
+ All axis and buttons of the wheel are reported
+ You can set the range of the wheel from 270° to 1080°
+ You can set the return force of the wheel from 0% to 100%

### What is missing 🚧
- Reading the settings from the wheel
- Automatically default setting when the wheel is attached to the machine 
- This driver is not used by the Kernel when the `hid` module is loaded
- Force feedback
- Force feedback settings
- Firmware upgrades

## How to use the driver
**Always put the switch of your wheel to the `PS3` position before plug it into your machine!**

### Switch the wheel from FFB to full T150
When attached to your machine the wheel reports itself as `Thrustmaster FFB Wheel`, in this mode not all functionalities
are available. In order to switch to the `Thrustmaster T150RS` we have to send the following USB control packet to the 
wheel:
```
bRequestType = 0x41
bRequest = 83
wValue = 0x0006
wIndex = 0
wLength = 0
``` 
To do so we can use the [`ffb`](./ffb/ffb.c) driver from this project xor you can write a simple userspace applications like 
[this one](https://gitlab.com/her0/tmdrv) thanks to `libusb`.

When the wheel receives the control packet it will reset and re-appear in the system as a T150.

### Setting up the wheel parameters
You can edit the settings of each wheel attached to the machine by writing the sysfs attributes usually found in the 
subdirectories at `/sys/bus/usb/drivers/t150`.

This table contains a summary of each attribute

|Attribute          |Value                         |Description                                                       |
|-------------------|------------------------------|------------------------------------------------------------------|
|`range`            |decimal from `270` to `1080`  |How far the wheel turns                                           |
|`return_force`     |decimal from `0` to `100`     |The force used to re-center the wheel                             |

## How to install and load the driver

### Build the drivers
For a simple build: install all the required tools to compile (like `build-essential`, `linux-headers` etc...) and run
```
make
```
into the repo's root. Now, if everything went right, in `build/` you should have the two modules `t150.ko` and `ffb.ko`
ready to be loaded into the Kernel. You can load them with 
```
insmod ffb.ko
insmod t150.ko
```

...
