#!/bin/bash
# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi
# Installing Trekstor S11B Touchscreen
echo "Installing Trekstor S11B Touchscreen..."
cp 61-sensor-local.hwdb /etc/udev/hwdb.d/
systemd-hwdb update
udevadm trigger -v -p DEVNAME=/dev/iio\:device0
echo "Done. You might have to reboot to activate device. Do not forget to check if your (Gnome) Desktop has the screen position locked"
