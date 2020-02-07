# How to compile and install Kernel Module

## Summary

On Fedora 31 the kernel module for Goodix touchscreens is broken. Under certain conditions the configuration of the touchscreen cannot be read by the driver. On the Trekstor S11B this happens when it was suspended.

Thanks to the help of [Hans de Goede](https://github.com/jwrdegoede) a patch has been provided. Until this patch will be available on popular Linux distributions, a custom module, replacing the broken one, can be compiled on affected devices.

In this example I use [DKMS](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support) (Dynamic Kernel Module Support) to compile and install the module. It will be automatically updated when a new kernel is installed. This exmaple only covers Fedora Linux. The command shown are different on other Linux distributions!

## Preparations

### Disable Secure Boot

First of all you need to disable [Secure Boot](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface#Secure_boot) in your devices UEFI. On the Trekstor S11B you have to repeatedly press F7 after powering up. In the dialog "Please select boot device:" select "Enter Setup". In the "Aptio Setup Utility" navigate to "Security" and then to "Secure Boot". Make sure that "Secure Boot" is set to "Disabled". Navigate to "Save and Exit" and select "Save Changes and Exit".

### Install Kernel Development Tools

On Fedora Linux run this command to install all the programs needed to build and install the kernel module:

```bash
$ sudo dnf -y install kernel-devel dkms make git
```

### Update the system

Update Fedora Linux to latest version by running:

```bash
$ sudo dnf -y update
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
$ sudo dkms add goodix/20200207
$ sudo dkms build goodix/20200207
$ sudo dkms install goodix/20200207
```

The last step will take 2-3 minutes on the Trekstor S11B. You can follow whats happening behind the scenes by opening another terminal and running ```journalctl -f```.

### Test the new kernel module

The kernel module should be loaded automatically after the last step has finished.

## Further reading

* Introduction to Creating DKMS Packages for Ubuntu, Part 1: <https://www.forshee.me/2012/03/16/introduction-to-creating-dkms-packages.html>
