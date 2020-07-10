# Clear Linux on the Trekstor S11B

![Trekstor Primetab S11B running Clear Linux Screenshot](https://techrevelations.de/wp-content/uploads/2020/02/Bildschirmfoto-vom-2020-02-22-20-25-32.png)

## Installation

The installation now works without problems. Language and Region can be set after first boot. Some bundles (packages) have to be installed to be able to use the device's screen rotation.

## What needs to be patched

* No more patches needed. Clear Linux* works on the S11B well!

## What does not work

* Webcam

## Install necessary packages

### IIO-Sensor-Proxy

Install [IIO-Sensor-Proxy](https://clearlinux.org/software/bundle/iio-sensor-proxy) to enable screen rotation.

```bash
$ sudo swupd bundle-add iio-sensor-proxy
```

Reboot your device. For further information please check the [iio-sensor-proxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/)-page.