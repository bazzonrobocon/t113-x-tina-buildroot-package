使用方法：
确保adb 正常，然后执行此目录下的 push.bat

将本地的4张720p的图片push到/tmp/g2d_test/目录，然后合成之后会被pull回来，得到一张4合一的图片2560x1440_cvideo.yuv，最后再pull回本地。

测试打印log如下：
C:\Windows>adb shell g2d_test 2
g2d test version:V2.1.20220906
set sdk log level 6
01-02 03:07:06.672 g2d_test(D) : arc=2, testid=2
01-02 03:07:06.673 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 03:07:06.674 g2d_test(D) : fopen /tmp/g2d_test/video0.yuv OK
01-02 03:07:06.679 g2d_test(D) : fopen /tmp/g2d_test/video1.yuv OK
01-02 03:07:06.683 g2d_test(D) : fopen /tmp/g2d_test/video2.yuv OK
01-02 03:07:06.688 g2d_test(D) : fopen /tmp/g2d_test/video3.yuv OK
(D) 01-02 03:07:06.695 g2d_test(D) : ================== start yuv 4in1 compose =============
01-02 03:07:06.711 g2d_test(D) : g2d compose ret:0, use time =16289 us
01-02 03:07:06.712 g2d_test(D) : WritePicFileContent size=5529600
01-02 03:07:06.716 g2d_test(D) : fopen /tmp/g2d_test/cvideo.yuv OK
