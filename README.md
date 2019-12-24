# Trekstor Primetab S11B Linux Support

## Welcome

This repository has been set up to help you to run Linux on your Trekstor Primetab S11B Tablet PC. At the moment, development and testing is still going on and not for all hardware has a fix been found yet. 

## Hardware working out of the box

* Memory, RAM, Battery, USB
* WiFi (802.11 A/C/N)
* Screen
* Touchscreen
* Keyboard
* Bluetooth

## Hardware working with patches from this repository

* Touchscreen orientation sensor -> See hwdb.d folder
* Touchpad -> See bugfixes/keyboardcover

## Hardware that needs patching (working on) and Bugs

* Touchscreen unresponsive after suspend. See [Red Hat Bugzilla bugreport](https://github.com/bedawi/trekstor-s11b/projects/1)
* Power events triggered during normal operation

## Tested with Fedora 31

Right now all scripts, manuals and patches are made for and on Fedora 31 Linux. If you are using another distribution, please be aware that applications, commands and paths might be different. Support for other distributions is planned for the future.
