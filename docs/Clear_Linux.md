# Clear Linux on the Trekstor S11B

![Trekstor Primetab S11B running Clear Linux Screenshot](https://techrevelations.de/wp-content/uploads/2020/02/Bildschirmfoto-vom-2020-02-22-20-25-32.png)

## Installation

The normal installer can be used. System might not boot after installation because wrong EFI boot target is set. This can be fixed in UEFI (Bios)

## What needs to be patched

* Goodix Kernel Module
* Accelaration Sensors
* Systemd Hardware Database

## How to patch

### Installation of required bundles

```bash
$ sudo swupd bundle-add devpkg-libgudev dev-utils-dev
```

This installs a lot of development tools, much more then you need. You can later uninstall the packages again using ```bundle-remove dev-utils-dev``` with swupd later.

### Installation of Touchscreen kernel module

If your goodix touchscreen is not working after you send the S11B device to suspend, then installing a patched kernel module can help. Please follow the instructions in the goodix-20200207-folder of this repository.

### Patching the systemd database

Follow the instructions given in the systemd folder of this repository. Please note that on Clear Linux you have to create the folder ```/etc/udev/hwdb.d``` by running ```sudo mkdir /etc/udev/hwdb.d```.

### Installation of Acceleration sensors

```bash
$ git clone https://gitlab.freedesktop.org/hadess/iio-sensor-proxy.git
$ cd iio-sensor-proxy/
$ ./autogen.sh --prefix=/usr --sysconfdir=/etc
$ make
$ sudo make install
```

Important: If you had compiled the package without parameters before, you have to uninstall (```sudo make uninstall```), clone a new copy of the repository and then run autogen with the mentioned paramters again before compiling.

Check if sensors are detected:

```bash
$ udevadm info --export-db | grep iio
P: /devices/pci0000:00/0000:00:17.2/i2c_designware.6/i2c-11/i2c-KIOX000A:00/iio:device0
N: iio:device0
E: DEVPATH=/devices/pci0000:00/0000:00:17.2/i2c_designware.6/i2c-11/i2c-KIOX000A:00/iio:device0
E: SUBSYSTEM=iio
E: DEVNAME=/dev/iio:device0
E: DEVTYPE=iio_device
```

Reboot your device. For further information please check the [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/)-page.