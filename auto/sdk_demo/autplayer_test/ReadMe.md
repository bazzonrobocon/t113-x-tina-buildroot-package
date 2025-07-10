## 模块介绍
此demo是用于演示AUTPlayer类的测试demo，可用于设置播放文件路径、文件播放、暂停、获取媒体信息和切换字幕等等。

AUTPlayer类是基于xplayer封装的播放类，屏蔽了音频和显示等细节，便于直接使用。

## 使用说明
默认会把测试bin安装到rootfs中，系统启动后，在小机端执行对应命令即可，小机端可以正常播放音视频即表示功能正常。

================================Usage================================
autplayer_test <testcase> <mediafile>
autplayer_test 0 /tmp/autplayer/test.mp4          means: start play test.mp4, and pause
autplayer_test 1 /tmp/autplayer/test.mp4          means: start play test.mp4, and print MediaInfo
autplayer_test 3 /tmp/autplayer/test.mp4          means: start play test.mp4, and seekto to 20
autplayer_test 4 /tmp/autplayer/test.mp4          means: start play test.mp4, and setSpeed to 2
================================Usage================================
