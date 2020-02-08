# How to compile and install Kernel Module

## Summary

In some kernel versions loading the hid_multitouch driver/kernel module causes problems with the Trekstor S11B keyboard cover's touchpad. The problem can be solved by compiling a newer version of the hid_multitouch module.

This problem is usually solved by a system update. On Fedora 31 there is no need to install this module, because a system update fixes the issue. On Ubuntu 19.10 (with Kernel 5.3.0-29-generic) the problem exists and this kernel module can solve it.

In this example I use [DKMS](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support) (Dynamic Kernel Module Support) to compile and install the module. It will be automatically updated when a new kernel is installed.

## Preparations

### Disable Secure Boot

First of all you need to disable [Secure Boot](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface#Secure_boot) in your devices UEFI. On the Trekstor S11B you have to repeatedly press F7 after powering up. In the dialog "Please select boot device:" select "Enter Setup". In the "Aptio Setup Utility" navigate to "Security" and then to "Secure Boot". Make sure that "Secure Boot" is set to "Disabled". Navigate to "Save and Exit" and select "Save Changes and Exit".

### Install Kernel Development Tools

On Fedora Linux run this command to install all the programs needed to build and install the kernel module:

```bash
$ sudo dnf -y install kernel-devel dkms make git
```

On Ubuntu Linux run this command to install all the programs needed to build and install the kernel module:

```bash
$ sudo apt install dkms
```

On Pop!OS no extra packages need to be installed.

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
$ sudo cp -r trekstor-s11b/hid-multitouch-20200208 /usr/src/
```

### Add, build and install the driver with DKMS

```bash
$ sudo dkms install hid-multitouch/20200208

hid-multitouch.ko:
Running module version sanity check.
 - Original module
   - No original module exists within this kernel
 - Installation
   - Installing to /lib/modules/5.3.0-29-generic/updates/dkms/

depmod.....

DKMS: install completed.
```

The last step can take 2-3 minutes on the Trekstor S11B. You can follow whats happening behind the scenes by opening another terminal and running ```journalctl -f```.

### Test the new kernel module

Restart the tablet _or_ run ```sudo rmmod hid_multitouch && sudo modprobe hid_multitouch```. Is your touchpad now working? Good. If you like to check whether the new kernel module has been loaded, run this command:

```bash
$ modinfo -n hid-multitouch
/lib/modules/5.3.0-29-generic/updates/dkms/hid-multitouch.ko
```

In the example you can see that the new kernel module has been loaded from the updates/dkms/ subfolder.

### Uninstall

In case you like to uninstall the custom kernel module, run this command:

```bash
$ sudo dkms uninstall hid-multitouch/20200208
```

## Further reading

* Introduction to Creating DKMS Packages for Ubuntu, Part 1: <https://www.forshee.me/2012/03/16/introduction-to-creating-dkms-packages.html>
