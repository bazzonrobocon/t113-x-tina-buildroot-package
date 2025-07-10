## 模块介绍
此demo是用于演示AWVideoEncoder类的测试demo，可用于yuv数据编码成jpeg或者h264。

AWVideoEncoder类的介绍可以参考《AWVideoEncoder使用手册.doc》

## 使用说明
默认不会把测试bin安装到rootfs中，需要手动执行push.bat脚本，脚本会默认执行demo程序，并把yuv文件编码之后的文件pull到testfiles/output文件下，电脑端查看编码后的文件可正常显示即表示此demo功能正常。

====================================== Usage ======================================
encoder_test -h --help                           Print this help
encoder_test -i --input                          Input file path
encoder_test -n --frameCount                     After encoder n frames, encoder stop
encoder_test -f --codecType                      0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder
encoder_test -p --pixelFormat                    0: YUV420SP, 1:YVU420SP, 3:YUV420P
encoder_test -o --output                         output file path
encoder_test -sw --srcw                           SRC_W
encoder_test -sh --srch                           SRC_H
encoder_test -b --bitrate                        bitRate:kbps
encoder_test -w --test way                       (0:single test, 1:multiDecoderTest)
encoder_test -t --test times                      testTimes
====================================== Usage ======================================
