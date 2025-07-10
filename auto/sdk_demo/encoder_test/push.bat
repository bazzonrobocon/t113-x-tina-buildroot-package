adb push %~dp0encoder_test /usr/bin
adb shell chmod +x /usr/bin/encoder_test

rd /s /q %~dp0testfiles\output
adb shell rm -rf /tmp/encoder_test/output
adb shell mkdir /tmp/encoder_test/output
adb push %~dp0testfiles/video_800x480.yuv /tmp/encoder_test/video_800x480.yuv

adb shell sync
adb shell encoder_test -i /tmp/encoder_test/video_800x480.yuv -o /tmp/encoder_test/output/video_800x480 -p 1 -f 0
adb shell sync

adb pull /tmp/encoder_test/output/ %~dp0testfiles/

pause
