
del /s /q %~dp0720x1280_rotate_90.yuv
adb shell rm -f /tmp/g2d_test/720x1280_rotate_90.yuv

adb push %~dp01280x720.yuv /tmp/g2d_test/1280x720.yuv

adb push %~dp0../../g2d_test /usr/bin
adb shell chmod 777 /usr/bin/g2d_test

adb shell sync
adb shell g2d_test 3
adb shell sync

adb pull /tmp/g2d_test/720x1280_rotate_90.yuv %~dp0720x1280_rotate_90.yuv
pause

