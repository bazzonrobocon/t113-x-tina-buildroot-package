使用方法：
确保adb 正常，然后执行此目录下的 push.bat

将本地的1张rgba8888的图片push到/tmp/g2d_test/目录，然后进行左右镜像处理，最后再pull回本地。

测试打印log如下：
C:\Windows>adb shell g2d_test 4
g2d test version:V2.1.20220906
set sdk log level 6
01-02 04:14:30.453 g2d_test(D) : arc=2, testid=4
01-02 04:14:30.453 suxiMemInterface(D) : sdk_memory version:V2.1.20220906
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
01-02 04:14:30.455 g2d_test(D) : fopen /tmp/g2d_test/1024x600.rgba OK
01-02 04:14:30.462 g2d_test(D) : alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=5
01-02 04:14:30.463 g2d_test(D) : alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=6
(D) 01-02 04:14:30.463 g2d_test(D) : ================== start rgb v flips =============
01-02 04:14:30.467 g2d_test(D) : g2d v flips ret:0, use time:4041 us
01-02 04:14:30.468 g2d_test(D) : WritePicFileContent size=2457600
01-02 04:14:30.470 g2d_test(D) : fopen /tmp/g2d_test/1024x600_h_flips.rgba OK
