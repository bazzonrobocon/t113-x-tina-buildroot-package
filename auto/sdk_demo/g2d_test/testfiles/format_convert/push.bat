
adb shell rm -f /tmp/g2d_test/1024x600_convert.yuv
del /s /q %~dp01024x600_convert.yuv

adb shell rm -f /tmp/g2d_test/1024x600_convert.rgba
del /s /q %~dp01024x600_convert_fd.rgba
del /s /q %~dp01024x600_convert_phy.rgba

adb push %~dp01024x600.rgba /tmp/g2d_test/1024x600.rgba
adb push %~dp01024x600.yuv /tmp/g2d_test/1024x600.yuv

adb push %~dp0../../g2d_test /usr/bin
adb shell chmod 777 /usr/bin/g2d_test

adb shell sync
adb shell g2d_test 6
adb shell sync
adb shell g2d_test 7
adb shell sync

adb pull /tmp/g2d_test/1024x600_convert.yuv %~dp01024x600_convert.yuv
adb pull /tmp/g2d_test/1024x600_convert.rgba %~dp01024x600_convert_fd.rgba

adb shell g2d_test 8
adb shell sync
adb pull /tmp/g2d_test/1024x600_convert.rgba %~dp01024x600_convert_phy.rgba

pause
