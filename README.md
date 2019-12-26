# Trekstor Primetab S11B Linux Support

## Welcome

This repository has been set up to help you to run Fedora Linux on your Trekstor Primetab S11B Tablet PC. At the moment, development and testing is still going on and not for all hardware has a fix been found yet. 

![Trekstor Primetab S11B Fedora 31 Linux](https://techrevelations.de/wp-content/uploads/2019/12/IMG_2714.jpg)

## Hardware working out of the box

* Memory, RAM, Battery, USB
* WiFi (802.11 A/C/N)
* Screen
* Touchscreen
* Keyboard
* Bluetooth

## Hardware working with patches from this repository

* Touchscreen orientation sensor -> See [hacks/display-orientation folder](hacks/display-orientation/)
* Touchpad -> See [hacks/keyboardcover folder](hacks/keyboardcover/)

## Hardware that needs patching (working on) and Bugs

* [Touchscreen unresponsive after suspend](https://github.com/bedawi/trekstor-s11b/issues/1)
* Power events triggered during normal operation

## Tested with Fedora 31

Right now all scripts, manuals and patches are made for and on Fedora 31 Linux. If you are using another distribution, please be aware that applications, commands and paths might be different. Support for other distributions is planned for the future.
