# viauled

This program connects to the Framework RGB keyboard running the default VIA firmware and exposes its backlight brightness as an LED via sysfs

The name of the LED is `viauled::kbd_backlight` is chosen to conform with how other drivers would name a keyboard backlight, making it work seamlessly in services like upower

Below is an example of how it appears on my Framework running KDE (the "Keyboard Backlight" slider)

![image](kde-example.png)

## Build

Make sure you have `hidapi`, `cmake` and your kernel headers installed

`./build.sh`

## Installing

You can copy the `viauled` binary from the `build` folder to its final destination.

## Running

[viauled.service](viauled.service) in this repo is an example systemd service file

Otherwise, the binary can just be run without arguments as root

## Special Thanks

Framework for their nice documentation of the "VIA" protocol in this tool [qmk_hid](https://github.com/FrameworkComputer/qmk_hid)
