
del /s /q %~dp01280x720_dvideo0.yuv
del /s /q %~dp01280x720_dvideo1.yuv
del /s /q %~dp01280x720_dvideo2.yuv
del /s /q %~dp01280x720_dvideo3.yuv
adb shell rm -f /tmp/g2d_test/dvideo0.yuv
adb shell rm -f /tmp/g2d_test/dvideo1.yuv
adb shell rm -f /tmp/g2d_test/dvideo2.yuv
adb shell rm -f /tmp/g2d_test/dvideo3.yuv

adb push %~dp02560x1440_dcvideo.yuv /tmp/g2d_test/dcvideo.yuv

adb push %~dp0../../g2d_test /usr/bin
adb shell chmod 777 /usr/bin/g2d_test

adb shell sync
adb shell g2d_test 1
adb shell sync

adb pull /tmp/g2d_test/dvideo0.yuv %~dp01280x720_dvideo0.yuv
adb pull /tmp/g2d_test/dvideo1.yuv %~dp01280x720_dvideo1.yuv
adb pull /tmp/g2d_test/dvideo2.yuv %~dp01280x720_dvideo2.yuv
adb pull /tmp/g2d_test/dvideo3.yuv %~dp01280x720_dvideo3.yuv

pause
