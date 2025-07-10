adb shell mkdir /tmp/gif2rgb_test
adb shell rm -f /tmp/gif2rgb_test/rainbow-*

adb push %~dp0../../sdk_lib/libs/libsdk_gifdecoder.so /usr/lib

adb push %~dp0testfiles/rainbow.gif /tmp/gif2rgb_test/rainbow.gif

adb push %~dp0gif2rgb_test /usr/bin
adb shell chmod +x /usr/bin/gif2rgb_test

adb shell gif2rgb_test

adb pull /tmp/gif2rgb_test/rainbow-0.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-1.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-2.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-3.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-4.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-5.rgb %~dp0testfiles/
adb pull /tmp/gif2rgb_test/rainbow-6.rgb %~dp0testfiles/
pause
