## 功能介绍
此demo用于验证CSI驱动功能是否正常，此demo基于V4L2框架开发，即只要驱动适配好V4L2接口，均可使用此demo进行测试验证。

## 使用说明
默认把测试bin安装到rootfs中，只需在小机端执行csi_test即可，小机端可以正常抓取到csi模块当前采集的图像即表示功能正常。


==========================Usage==========================
csi_test_usrptr <videoX> <sel> <width> <height> <path> <format_mode> <test_cnt> <fps>
videoX          means: open /dev/videoX
sel             means: select subdev-X
width           means: camera capture pic width
height          means: camera capture pic height
path            means: camera capture save file path, default "/tmp/tvd_test/"
format_mode     means: camera capture pic format, default 4(NV21)
test_cnt        means: camera capture pic count, default 20
fps             means: camera capture fps, default 30
==========================Usage==========================

## 调试说明

```
$ csi_test_usrptr 0 0 1280 720 /tmp/csi_test/ 4 10 30
csi_test_usrptr version:V2.1.20220922sdk_memory version:V2.0.20220506
INFO   : cedarc <VeInitialize:1307>: *** ic_version = 0x1301000010210,
(getInstance 93) version:V2.0.20220506
init chwd sucess !
(HwDisplay 279)
(hwd_init 1258)
open /dev/video0 fd = 6
fmt.fmt.pix_mp.pixelformat=82▒382478
resolution got from sensor } 1280*720 num_planes = 1
VIDIOC_QUERYBUF buf.m.planes[j].length=1382400
VIDIOC_QUERYBUF buf.m.planes[j].length=1382400
VIDIOC_QUERYBUF buf.m.planes[j].length=1382400
VIDIOC_STREAMON ok
file length = 1382400 0 0
file start = 0xb6ac1000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb696f000 (nil) (nil)
file length`= 1382400 0 0
file start = 0xb681d000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb6ac1000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb696f000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb681d000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb6ac1000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb696f000 (nil) (nil)
file length = 1382400 0 0
file start = 0xb681d000 (nil) (nil)
VIDIOC_STREAMOFF ok
INFO   : cedarc <VeRelease:1476>: not malloc locks

mode 4 test done at the 0 time!!
time cost 0.540688(s)
```
