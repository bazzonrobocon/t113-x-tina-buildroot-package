adb shell rm -f /usr/bin/dvr_test
adb push %~dp0dvr_test /usr/bin
adb shell chmod 777 /usr/bin/dvr_test

pause