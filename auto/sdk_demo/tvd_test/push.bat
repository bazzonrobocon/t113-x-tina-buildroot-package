adb shell mkdir /tmp/tvd_test/
adb shell rm -f /tmp/tvd_test/*

adb push %~dp0tvd_test_mmap /usr/bin/
adb shell chmod +x /usr/bin/tvd_test_mmap

adb shell tvd_test_mmap 4 1 720 576 /tmp/tvd_test/ 4 10 30

rd /s /q %~dp0testfiles\
adb pull /tmp/tvd_test/ testfiles/

pause
