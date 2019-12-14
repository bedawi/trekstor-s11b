# Fix Touchscreen Standy Problem

## Issue

After sleep the touchscreen does not work anymore.

## How to fix

In many (not all) cases, it helps to unload the goodix kernel driver before sleep and restart it after resume. The script provided in this folder does that.

PLEASE NOTE: The script only works in Fedora Linux or distributions with SELinux enabled. It only works sometimes it cannot (yet) fix the underlying problems of the driver and/or i2c bus.

1. Clone this repository to your machine

...if you have not already done so.

```shell
git clone https://github.com/bedawi/trekstor-s11b.git
```

2. Run install script

```shell
cd hacks/touchscreen
sudo bash install_wakeuppatch.bash
```