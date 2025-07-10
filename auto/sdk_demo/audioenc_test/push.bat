adb shell mkdir /tmp/audioenc_test/
adb push %~dp0testfiles/test.wav /tmp/audioenc_test/test.wav

adb push %~dp0audioenc_test /usr/bin/
adb shell chmod +x /usr/bin/audioenc_test

adb shell /usr/bin/audioenc_test

adb pull /tmp/audioenc_test/test.mp3 %~dp0testfiles/test.mp3

pause