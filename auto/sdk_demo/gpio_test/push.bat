
adb push %~dp0gpio_test /usr/bin/gpio_test
adb shell chmod +x /usr/bin/gpio_test

adb shell gpio_test

pause
