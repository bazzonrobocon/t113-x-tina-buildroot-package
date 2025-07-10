## 模块介绍
此demo是用于演示AWStreamPlayer类的测试demo，可用于直接播放H264/H265/jpg等格式的数据。

AWStreamPlayer类的介绍可以参考《AWStreamPlayer使用手册.doc》

## 使用说明
默认把测试bin安装到rootfs中，系统启动后，在小机端执行对应命令即可，小机端可以正常播放音视频即表示功能正常。

===================================== Usage ======================================
stream_player_test -h --help           Print this help
stream_player_test -i --input          Input file path
stream_player_test -f --codecType      0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder
stream_player_test -p --pixelFormat    0: YUV420SP, 1:YVU420SP, 3:YUV420P
stream_player_test -o --output         output file path
stream_player_test -s --srcw           SRC_W
stream_player_test -d --srch           SRC_H
stream_player_test -w --test way       (0:single test, 1:multiDecoderTest,2:single h264 fream)
stream_player_test -t --test times      testTimes
stream_player_test -c --test cmd        testCmd
====================================== Usage ======================================

直接执行stream_player_test会默认播放testfiles下的video_800x480.h264