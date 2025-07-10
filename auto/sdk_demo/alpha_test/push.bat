adb shell mkdir /tmp/alpha_test/
adb push %~dp0testfiles/pic_480p /tmp/alpha_test/pic_480p

adb push %~dp0alpha_test /usr/bin/
adb shell chmod +x /usr/bin/alpha_test

pause
