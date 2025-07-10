使用方法：
确保adb 正常，然后执行此目录下的 push.bat

将本地的1张720p的yuv图片push到/tmp/g2d_test/目录，然后旋转90度得到1张720x1280的yuv图像，最后再pull回本地。

要注意的是如果是旋转90，输出的长宽就不是1280x720，而是720*1280

测试打印log如下：
C:\Windows>adb shell g2d_test 3
g2d test version:V2.1.20220906
set sdk log level 6
01-02 04:16:59.472 g2d_test(D) : arc=2, testid=3
01-02 04:16:59.473 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 04:16:59.474 g2d_test(D) : fopen /tmp/g2d_test/1280x720.yuv OK
01-02 04:16:59.478 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-02 04:16:59.478 g2d_test(D) : alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=6
(D) 01-02 04:16:59.479 g2d_test(D) : ================== start yuv rotate =============
01-02 04:16:59.481 g2d_test(D) : g2d rotate ret:0, use time =2557 us
01-02 04:16:59.481 g2d_test(D) : WritePicFileContent size=1382400
01-02 04:16:59.483 g2d_test(D) : fopen /tmp/g2d_test/720x1280_rotate_90.yuv OK