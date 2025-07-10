adb shell mkdir /tmp/yuv_test
adb push %~dp0testfiles/bike_1280x720_565 /tmp/yuv_test/bike_1280x720_565
adb push %~dp0testfiles/bike_1280x720_888 /tmp/yuv_test/bike_1280x720_888
adb push %~dp0testfiles/bike_1280x720_yv12.yuv /tmp/yuv_test/bike_1280x720_yv12.yuv

adb push yuvtest /usr/bin/
adb shell chmod +x /usr/bin/yuvtest

adb shell yuvtest

pause
