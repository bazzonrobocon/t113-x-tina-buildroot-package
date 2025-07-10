
adb shell mkdir /tmp/record_test
adb push %~dp0testfiles/720p_record_test.yuv /tmp/record_test/720p_record_test.yuv

adb push %~dp0record_test /usr/bin/
adb shell chmod 777 /usr/bin/record_test

adb shell /usr/bin/record_test

adb pull /tmp/record_test/recode_out.h264 %~dp0testfiles/recode_out.h264

pause