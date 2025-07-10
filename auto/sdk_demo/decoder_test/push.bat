adb push %~dp0decoder_test /usr/bin
adb shell chmod +x /usr/bin/decoder_test

rd /s /q %~dp0testfiles\video_800x480.yuv
adb shell rm -f /tmp/decoder_test/video_800x480.yuv
adb push %~dp0testfiles/video_800x480.h264 /tmp/decoder_test/video_800x480.h264

adb shell sync
adb shell decoder_test
adb shell sync

adb pull /tmp/decoder_test/video_800x480.yuv %~dp0testfiles/video_800x480.yuv

pause
