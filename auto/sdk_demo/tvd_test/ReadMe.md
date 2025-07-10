## 功能介绍
此demo用于验证cvbs camera功能是否正常。

## 使用说明
默认把测试bin安装到rootfs中，可以直接在系统中执行tvd_test即可。

注意1：
tvd_test.cpp 使用的是 MMAP 方式，没有支持送显

注意2：
T113平台支持2路cvbs in输入，如果需要切换另一路 cvbs_in , 直接修改 sel 为 1 便可
请务必接入摄像头测试，如果不接摄像头，会报错VIDIOC_G_FMT error!

==========================Usage==========================
tvd_test_mmap <videoX> <sel> <width> <height> <path> <format_mode> <test_cnt> <fps>
videoX          means: open /dev/videoX
sel             means: select subdev-X
width           means: camera capture pic width
height          means: camera capture pic height
path            means: camera capture save file path, default "/tmp/tvd_test/"
format_mode     means: camera capture pic format, default 4(NV21)
test_cnt        means: camera capture pic count, default 20
fps             means: camera capture fps, default 30
==========================Usage==========================

## 测试参考

1. 抓取PAL数据

tvd_test_mmap 2 0 720 576 /tmp/
open /dev/video2 fd = 3
[   37.787375] [tvd] vidioc_s_fmt_vid_cap:1598
[   37.787375] interface=0
[   37.787375] system=PAL
[   37.787375] format=0
[   37.787375] output_fmt=YUV420
fmt.fmt.pix_mp.pixelformat=82538[   37.805275] [tvd] vidioc_s_fmt_vid_cap:1602
[   37.805275] row=1
[   37.805275] column=1
[   37.805275] ch[0]=0
[   37.805275] ch[1]=0
[   37.805275] ch[2]=0
[   37.805275] ch[3]=0
2478
[   37.826763] [tvd] vidioc_s_fmt_vid_cap:1604
[   37.826763] width=720
[   37.826763] height=576
[   37.826763] dev->sel=0
[   37.827000] [tvd] tvd_cagc_and_3d_config:1456 tvd0 agc auto mode
[   37.827156] [tvd] tvd_cagc_and_3d_config:1465 tvd0 CAGC enable:0x1
[   37.859664] [tvd] tvd_cagc_and_3d_config:1492 tvd0 3d enable :0x7f100000
resolution got from sensor = 720*576 num_planes = 0
[   38.071512] [tvd] vidioc_streamon:1687 Out vidioc_streamon:0
VIDIOC_STREAMON ok
[   38.145363] [tvd] tvd_isr:785 In tvd_isr

VIDIOC_STREAMOFF ok
mode 4 test done at the 0 time!!
time cost 4.413493(s)
