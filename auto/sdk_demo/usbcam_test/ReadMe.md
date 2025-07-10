## 功能介绍
此demo用于验证usb camera功能是否正常。

## 使用说明
默认把测试bin安装到rootfs中，可以直接在系统中执行usbcam_test即可。

测试方式：
1. 确定 usb camera 的分辨率，如 1920x1080
2. 确定 usb camera 生成的节点，可以通过拔插 usb camera，然后 ls /dev 看对应的是哪个 videoX
3. 键入命令执行usbcam_test即可
4. 最后把生成的bin文件pull出来即可

==========================Usage==========================
usbcam_test -v [videoX] -s [sel] -w [width] -h [height] -o [path] -m [format_mode] -n [test_cnt] -f [fps]
videoX          means: open /dev/videoX
sel             means: select subdev-X
width           means: camera capture pic width
height          means: camera capture pic height
path            means: camera capture save file path, default "/tmp/usbcam_test/"
format_mode     means: camera capture pic format, default 1(YUV422)
test_cnt        means: camera capture pic count, default 20
fps             means: camera capture fps, default 30
==========================Usage==========================

注意：
1. 此 usbcam_test 只适用于 usb camera 的调试，并不通用于其他 AHD camera 的应用
2. mode 表示如下，注意要确认usb camera是否支持。
	- 0 YUYV
	- 1 MJPEG
	- 2 H264


## 测试参考

1. 抓取yuv数据
# usbcam_test -v 0 -s 0 -w 1280 -h 720 -o /tmp/usbcam_test/ -m 1 -n 30 -f 25
usbcam test version:V2.1.20230905
open /dev/video0 fd = 3
mCameraType = CAMERA_TYPE_UVC
format index = 0, name = Input 1
input is 1
format index = 0, name = YUYV 4:2:2, v4l2 pixel format = 56595559
 pixel format is 56595559
resolution got from sensor = 1280*720 num_planes = 0
VIDIOC_STREAMON ok
VIDIOC_STREAMOFF ok
mode 1 test done!!
time cost 3.790318(s)

2. 抓取mjpeg格式数据
# usbcam_test -v 0 -s 0 -w 1280 -h 720 -o /tmp/usbcam_test/ -m 0 -n 30 -f 25
usbcam test version:V2.1.20230905
open /dev/video0 fd = 3
mCameraType = CAMERA_TYPE_UVC
format index = 0, name = Input 1
input is 1
format index = 0, name = YUYV 4:2:2, v4l2 pixel format = 56595559
format index = 1, name = H.264, v4l2 pixel format = 34363248
format index = 2, name = Motion-JPEG, v4l2 pixel format = 47504a4d
 pixel format is 47504a4d
resolution got from sensor = 1280*720 num_planes = 0
VIDIOC_STREAMON ok
VIDIOC_STREAMOFF ok
mode 0 test done!!
time cost 1.427896(s)
