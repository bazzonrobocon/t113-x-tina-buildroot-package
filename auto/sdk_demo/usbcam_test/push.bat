adb shell mkdir /tmp/usbcam_test/
adb shell rm -f /tmp/usbcam_test/*

adb push %~dp0usbcam_test /usr/bin/
adb shell chmod +x /usr/bin/usbcam_test

adb shell usbcam_test -v 0 -s 0 -w 1280 -h 720 -o /tmp/usbcam_test -m 1 -n 1 -f 1

rd /s /q %~dp0testfiles\
adb pull /tmp/usbcam_test/ testfiles/

pause

