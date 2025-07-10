
adb shell rm -f /tmp/g2d_test/1280x720_scaler.rgba
del /s /q %~dp01280x720_scaler.rgba

adb push %~dp01024x600.rgba /tmp/g2d_test/1024x600.rgba

adb push %~dp0../../g2d_test /usr/bin
adb shell chmod 777 /usr/bin/g2d_test

adb shell sync
adb shell g2d_test 0
adb shell sync

adb pull /tmp/g2d_test/1280x720_scaler.rgba %~dp01280x720_scaler.rgba

pause
