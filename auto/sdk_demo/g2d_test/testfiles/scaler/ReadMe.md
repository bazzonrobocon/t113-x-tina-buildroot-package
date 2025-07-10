使用方法：
确保adb 正常，然后执行此目录下的 push.bat

将本地的1张rgba8888的图片push到/tmp/g2d_test/目录，然后进行图像缩放，得到一张放大之后的1280x720的rgba8888图片，最后再pull回本地。

测试打印log如下：
C:\Windows>adb shell g2d_test 0
g2d test version:V2.1.20220906
set sdk log level 6
01-02 04:17:13.812 g2d_test(D) : arc=2, testid=0
01-02 04:17:13.813 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 04:17:13.814 g2d_test(D) : fopen /tmp/g2d_test/1024x600.rgba OK
01-02 04:17:13.821 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-02 04:17:13.823 g2d_test(D) : alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=6
(D) 01-02 04:17:13.823 g2d_test(D) : ================== start rgb scaler =============
01-02 04:17:13.830 g2d_test(D) : g2d scale ret:0, use time =6364 us
01-02 04:17:13.830 g2d_test(D) : WritePicFileContent size=3686400
01-02 04:17:13.833 g2d_test(D) : fopen /tmp/g2d_test/1280x720_scaler.rgba OK
