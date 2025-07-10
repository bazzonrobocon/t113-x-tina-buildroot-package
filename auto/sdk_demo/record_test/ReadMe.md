## 功能介绍
此demo提供读取yuv数据，并通过libsdk_dvr.so提供的接口，编码输出h264或mp4的文件
注：如果希望查找如何使用cedarx的编码接口，请参考encoder_test

## 测试说明

测试数据：720p_recordTest.yuv  格式是NV21,10帧，1280*720

### 输出H264
把720p_recordTest.yuv源文件push放到/tmp/record_test/目录下，直接运行recordTest，程序会生成一个/tmp/record_test/recode_out.h264文件，再pull出来播放即可。

### 输出mp4
如果需要封装，需要修改测试源码，修改recordTest.cpp，把TEST_H264_OUTPUT宏注释掉，编译后push到机器中，直接运行recordTest，那么libsdk_dvr.so会根据dvrconfig.ini中配置的路径保存mp4文件（默认使用camera0的配置项），最后把mp4文件pull出来播放即可。

备注：此测试输出的mp4仅有视频，没有音频。

备注2：T113平台不支持H264编码
