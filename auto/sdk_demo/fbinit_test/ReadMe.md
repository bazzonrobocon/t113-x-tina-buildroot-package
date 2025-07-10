## 功能介绍
此demo用于清除当前显示的fb的数据，让显示屏上处于干净的状态，方便测试应用送显。

## 使用说明
默认把测试bin安装到rootfs中，可以直接在小机端执行fbinit即可，小机端显示黑屏即正常。

================Usage================
/fbinit           means:clean /dev/fb0
/fbinit 0         means:clean /dev/fb0
/fbinit 1         means:clean /dev/fb1
/fbinit 2         means:clean /dev/fb2
================usage================

参数缺省时，默认重置/dev/fb0