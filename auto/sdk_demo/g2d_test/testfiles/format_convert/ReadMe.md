
使用方法：
确保adb 正常，然后执行此目录下的 push.bat

case 6: 将本地的1张rgba8888的图片push到/tmp/g2d_test/目录，然后进行格式转换，得到1张nv21的图片，最后再pull回本地。
case 7: 将本地的1张NV21的图片push到/tmp/g2d_test/目录，然后进行格式转换，得到1张RGBA8888的图片，最后再pull回本地。

测试打印log如下：
C:\Windows>adb shell g2d_test 6
g2d test version:V2.1.20220906
set sdk log level 6
01-02 04:16:32.393 g2d_test(D) : arc=2, testid=6
01-02 04:16:32.393 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 04:16:32.395 g2d_test(D) : fopen /tmp/g2d_test/1024x600.rgba OK
01-02 04:16:32.402 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-02 04:16:32.403 g2d_test(D) : alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=6
(D) 01-02 04:16:32.404 g2d_test(D) : ================== start rgb convert to yuv =============
01-02 04:16:32.408 g2d_test(D) : g2d convert ret:0, use time:4494 us
01-02 04:16:32.408 g2d_test(D) : WritePicFileContent size=921600
01-02 04:16:32.409 g2d_test(D) : fopen /tmp/g2d_test/1024x600_convert.yuv OK


C:\Windows>adb shell g2d_test 7
g2d test version:V2.1.20220906
set sdk log level 6
01-02 04:16:45.692 g2d_test(D) : arc=2, testid=7
01-02 04:16:45.693 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 04:16:45.694 g2d_test(D) : fopen /tmp/g2d_test/1024x600.yuv OK
01-02 04:16:45.697 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-02 04:16:45.701 g2d_test(D) : alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=6
(D) 01-02 04:16:45.701 g2d_test(D) : ================== start yuv convert to rgb =============
01-02 04:16:45.705 g2d_test(D) : g2d convert ret:0, use time:3972 us
01-02 04:16:45.705 g2d_test(D) : WritePicFileContent size=2457600
01-02 04:16:45.707 g2d_test(D) : fopen /tmp/g2d_test/1024x600_convert.rgba OK

C:\Windows>adb shell g2d_test 8
g2d test version:V2.1.20220906
set sdk log level 6
01-01 00:03:14.383 g2d_test(D) : arc=2, testid=8
01-01 00:03:14.384 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-01 00:03:14.386 g2d_test(D) : fopen /tmp/g2d_test/1024x600.yuv OK
01-01 00:03:14.388 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-01 00:03:14.388 g2d_test(D) : WritePicFileContent size=921600
01-01 00:03:14.389 g2d_test(D) : fopen /tmp/g2d_test/ck.yuv OK
(D) 01-01 00:03:14.395 g2d_test(D) : ================== start yuv convert to rgb by phy addr =============
01-01 00:03:14.398 g2d_test(D) : g2d convert by phy address ret:0, use time:2304 us
01-01 00:03:14.398 g2d_test(D) : WritePicFileContent size=2457600
01-01 00:03:14.400 g2d_test(D) : fopen /tmp/g2d_test/1024x600_convert.rgba OK
