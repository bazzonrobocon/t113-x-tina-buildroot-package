adb shell mkdir /tmp/csi_test/
adb shell rm -f /tmp/csi_test/*

adb push %~dp0csi_test_usrptr /usr/bin/
adb shell chmod +x /usr/bin/csi_test_usrptr

adb shell csi_test_usrptr 0 0 1280 720 /tmp/csi_test/ 4 10 30

rd /s /q %~dp0testfiles\
adb pull /tmp/csi_test/ testfiles/

pause