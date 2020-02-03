# How to fix keyboard cover touchpad on Trekstor Surftab S11B

## Disable HID Multitouch driver

The problem of the not working touchpad on the S11B's keyboard cover seems to be related to Linux's multitouch driver. It is solved by newer Kernel versions. Before applying this patch please update your system and reboot. The problem should sbe gone. If not...

You can easily verify if this hack helps you by unloading the driver with this command ```sudo rmmod hid_multitouch```. If your touchpad is now working then this fix is for you.

### Using the bash script

This script has been written for Fedora 31 Linux and Red Hat Enterprise Linux 8.1. 

1. Clone this repository to your machine

...if you have not already done so.

```shell
git clone https://github.com/bedawi/trekstor-s11b.git
```

2. Run install script

```shell
cd hacks/keyboardcover
sudo bash fix_keyboardcover.bash
```

The script will add a boot parameter to the kernel to blacklist the kernel module from being loaded at boot time. It will also write a blacklist file to the modprobe service's configuration folder.

## Further Reading

<https://access.redhat.com/solutions/41278>

<https://manpages.ubuntu.com/manpages/precise/man5/modprobe.conf.5.html>