adb push %~dp0mem_test_de /usr/bin
adb shell chmod 777 /usr/bin/mem_test_de

adb shell /usr/bin/mem_test_de 60 20
adb shell sleep 1
adb shell /usr/bin/mem_test_de 80 128
adb shell sleep 1
adb shell /usr/bin/mem_test_de 60 240

pause
