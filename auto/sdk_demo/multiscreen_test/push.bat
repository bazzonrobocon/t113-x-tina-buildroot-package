adb push %~dp0multiscreen_test /usr/bin/
adb shell chmod +x /usr/bin/multiscreen_test
adb shell /usr/bin/multiscreen_test 5

choice /t 10 /d y /m "close after 10s"
