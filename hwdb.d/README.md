# Setting up the touchscreen orientation

This setup has been tested on Fedora 31 Linux

## Setup

1. Clone this repository to your machine

```shell
git clone https://github.com/bedawi/trekstor-s11b.git
```

2. Run install script

```shell
cd hwdb.d
sudo bash install-touchscreen.bash
```

Alternatively copy the ```61-sensor-local.hwdb```-file to ```/etc/udev/hwdb.d/```, run ```systemd-hwdb update```, and reboot your machine.

```shell
cp 61-sensor-local.hwdb /etc/udev/hwdb.d/
systemd-hwdb update
reboot
```
