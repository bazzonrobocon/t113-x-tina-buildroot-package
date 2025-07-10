adb shell rm -f /tmp/g2d_test/cvideo.yuv
del /s /q %~dp02560x1440_cvideo.yuv

adb push %~dp01280x720_video0.yuv /tmp/g2d_test/video0.yuv
adb push %~dp01280x720_video1.yuv /tmp/g2d_test/video1.yuv
adb push %~dp01280x720_video2.yuv /tmp/g2d_test/video2.yuv
adb push %~dp01280x720_video3.yuv /tmp/g2d_test/video3.yuv

adb push %~dp0../../g2d_test /usr/bin
adb shell chmod 777 /usr/bin/g2d_test

adb shell sync
adb shell g2d_test 2
adb shell sync

adb pull /tmp/g2d_test/cvideo.yuv %~dp02560x1440_cvideo.yuv

pause
