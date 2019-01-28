#!/bin/sh

PID=`pidof preset_top_layer`
kill ${PID}
echo "0" > /sys/class/android_usb/android0/enable
echo "adb" > /sys/class/android_usb/android0/functions
echo 2 > /sys/devices/10180000.usb/driver/force_usb_mode
echo "1" > /sys/class/android_usb/android0/enable





