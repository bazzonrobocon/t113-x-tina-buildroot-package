使用方法：
确保adb 正常，然后执行此目录下的 push.bat

将本地的1张2K的图片push到/tmp/g2d_test/目录，然后拆分成4个720p的yuv图像，最后再pull回本地。

测试打印log如下：
C:\Windows>adb shell g2d_test 1
g2d test version:V2.1.20220906
set sdk log level 6
01-01 03:32:55.562 g2d_test(D) : arc=2, testid=1
01-01 03:32:55.563 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-01 03:32:55.565 g2d_test(D) : fopen /tmp/g2d_test/dcvideo.yuv OK
01-01 03:32:55.582 g2d_test(D) : ================== start yuv 1to4 decompose =============
01-01 03:32:55.597 g2d_test(D) : g2d decompose ret:0, use time =15118 us
01-01 03:32:55.597 g2d_test(D) : WritePicFileContent size=1382400
01-01 03:32:55.599 g2d_test(D) : fopen /tmp/g2d_test/dvideo0.yuv OK
01-01 03:32:55.603 g2d_test(D) : WritePicFileContent size=1382400
01-01 03:32:55.604 g2d_test(D) : fopen /tmp/g2d_test/dvideo1.yuv OK
01-01 03:32:55.609 g2d_test(D) : WritePicFileContent size=1382400
01-01 03:32:55.610 g2d_test(D) : fopen /tmp/g2d_test/dvideo2.yuv OK
01-01 03:32:55.614 g2d_test(D) : WritePicFileContent size=1382400
01-01 03:32:55.616 g2d_test(D) : fopen /tmp/g2d_test/dvideo3.yuv OK
