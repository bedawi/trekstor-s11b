# How to compile and install Kernel Module

## Summary

On Fedora 31 the kernel module for Goodix touchscreens is broken. Under certain conditions the configuration of the touchscreen cannot be read by the driver. On the Trekstor S11B this happens when it was suspended.

Thanks to the help of [Hans de Goede](https://github.com/jwrdegoede) a patch has been provided. Until this patch will be available on popular Linux distributions, a custom module, replacing the broken one, can be compiled on affected devices.

In this example I use [DKMS](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support) (Dynamic Kernel Module Support) to compile and install the module. It will be automatically updated when a new kernel is installed.

## Preparations

### Disable Secure Boot

First of all you need to disable [Secure Boot](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface#Secure_boot) in your devices UEFI. On the Trekstor S11B you have to repeatedly press F7 after powering up. In the dialog "Please select boot device:" select "Enter Setup". In the "Aptio Setup Utility" navigate to "Security" and then to "Secure Boot". Make sure that "Secure Boot" is set to "Disabled". Navigate to "Save and Exit" and select "Save Changes and Exit".

### Install Kernel Development Tools

#### Fedora Linux

On Fedora Linux run this command to install all the programs needed to build and install the kernel module:

```bash
$ sudo dnf -y install kernel-devel dkms make git
```

#### Ubuntu Linux

On Ubuntu Linux run this command to install all the programs needed to build and install the kernel module:

```bash
$ sudo apt install dkms
```

#### Pop!OS

On Pop!OS no extra packages need to be installed.

#### Clear Linux*

On Clear Linux* you need to install one of the following bundles, depending on your kernel: kernel-native-dkms or kernel-lts-dkms. The currently running kernel can be determined by running ```uname -r```. If you are running the native Kernel then run these commands to install the necessary kernel headers and tools:

```bash
$ sudo swupd bundle-add kernel-native-dkms
$ sudo clr-boot-manager update
$ reboot
```

Learn more about this in the [Clear Linux* documentation](https://docs.01.org/clearlinux/latest/guides/kernel/kernel-modules-dkms.html).

### Update the system

Update your Linux to latest version before you continue!

On Fedora run:

```bash
$ sudo dnf -y update
```

On Ubuntu run:

```bash
$ sudo apt update && sudo apt upgrade -y
```

Reboot before you continue!

## Download and install kernel module

### Download this repository to your Downloads folder

```bash
$ git clone https://github.com/bedawi/trekstor-s11b.git
```

### Copy source code

```bash
$ sudo cp -r trekstor-s11b/goodix-20200207 /usr/src/
```

### Unload the existing kernel module from memory

Not sure if this is necessary, though...

```bash
$ sudo rmmod goodix
```

### Add, build and install the driver with DKMS

```bash
$ sudo dkms install goodix/20200207
```

The last step will take 2-3 minutes on the Trekstor S11B. You can follow whats happening behind the scenes by opening another terminal and running ```journalctl -f```.

### Test the new kernel module

The kernel module should be loaded automatically after the last step has finished. Send your tablet to suspend and wake it up again after some seconds. The touchscreen should still be working.

Typically dmesg shows an output like this:

```bash
$ dmesg | grep Goodix
[   16.085836] Goodix-TS i2c-GDIX1001:00: i2c-GDIX1001:00 supply AVDD28 not found, using dummy regulator
[   16.085869] Goodix-TS i2c-GDIX1001:00: i2c-GDIX1001:00 supply VDDIO not found, using dummy regulator
[   16.086400] Goodix-TS i2c-GDIX1001:00: Using ACPI INTI and INTO methods for IRQ pin access
[   16.086952] Goodix-TS i2c-GDIX1001:00: ID 928, version: 1040
[   16.091295] input: Goodix Capacitive TouchScreen as /devices/pci0000:00/0000:00:16.3/i2c_designware.3/i2c-9/i2c-GDIX1001:00/input/input15
[  141.629386] Goodix-TS i2c-GDIX1001:00: Error reading config version: -121, resetting controller
```

The message ```Error reading config version: -121, resetting controller``` comes from the patched driver.

## Further reading

* Introduction to Creating DKMS Packages for Ubuntu, Part 1: <https://www.forshee.me/2012/03/16/introduction-to-creating-dkms-packages.html>
