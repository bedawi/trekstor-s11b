#!/bin/bash
# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi
# Copying file
echo "Blacklisting hid_multitouch"
cp blacklist.conf /etc/modprobe.d/
echo "Changing Kernel boot parameters to blacklist driver"
dnf -y install grubby
grubby --update-kernel=ALL --args="rd.driver.blacklist=hid_multitouch" --make-default
echo "Removing currently running kernel module"
rmmod hid_multitouch
echo "Done."
