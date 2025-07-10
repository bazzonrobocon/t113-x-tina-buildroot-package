adb push %~dp0stream_player_test /usr/bin
adb shell chmod +x /usr/bin/stream_player_test

adb push %~dp0testfiles/. /tmp/stream_player_test/

adb shell stream_player_test -c 1

adb shell sleep 2

adb shell stream_player_test -c 3

adb shell sleep 2

adb shell stream_player_test -c 2

pause
