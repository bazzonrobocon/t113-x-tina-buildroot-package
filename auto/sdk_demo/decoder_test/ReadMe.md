## 模块介绍
此demo是用于演示AWVideoDecoder类的测试demo，可用于解码h264的裸流为yuv。

AWVideoDecoder类的介绍可以参考《AWVideoDecoder使用手册.doc》

## 使用说明
默认不会把测试bin安装到rootfs中，需要手动执行push.bat脚本，脚本会默认执行demo程序，并把h264文件解码之后的yuv文件pull到testfiles文件下，电脑端查看yuv图片可正常显示即表示此demo功能正常。

====================================== Usage ======================================
decoder_test -h --help           Print this help
decoder_test -i --input          Input file path
decoder_test -f --codecType      0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder
decoder_test -p --pixelFormat    0: YUV420SP, 1:YVU420SP, 3:YUV420P
decoder_test -o --output         output file path
decoder_test -sw --srcw           SRC_W
decoder_test -sh --srch           SRC_H
decoder_test -w --test way       (0:single test, 1:multiDecoderTest)
decoder_test -t --test times      testTimes
====================================== Usage ======================================
