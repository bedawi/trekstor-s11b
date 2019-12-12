#!/bin/bash
# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi
echo "Installing Touchscreen Standy Hack..."
cp touchscreen.sleep /usr/lib/systemd/system-sleep/
chmod +x /usr/lib/systemd/system-sleep/touchscreen.sleep
chcon -u system_u -r object_r -t lib_t /usr/lib/systemd/system-sleep/touchscreen.sleep
echo "done"