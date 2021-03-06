# Setting up the touchscreen orientation

This setup has been tested on Fedora 31 and Ubuntu 19.10.

**Note: This device was already [added](https://github.com/systemd/systemd/pull/14342) to systemd hardware database. Please run a system update first to determine whether this patch is needed at all.**

Distributions already including this patch:

* Fedora 32

## Setup

1. Clone this repository to your machine

```shell
git clone https://github.com/bedawi/trekstor-s11b.git
```

2. Run install script

```shell
cd trekstor-s11b/systemd
sudo bash install-touchscreen.bash
```

Alternatively copy the ```61-sensor-local.hwdb```-file to ```/etc/udev/hwdb.d/```, run ```systemd-hwdb update```, and reboot your machine.

```shell
cp 61-sensor-local.hwdb /etc/udev/hwdb.d/
systemd-hwdb update
reboot
```
