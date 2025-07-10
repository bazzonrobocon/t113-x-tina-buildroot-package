## 功能介绍
此demo用于验证DVR预览/录像功能。

## 使用说明
默认把测试bin安装到rootfs中，只需在小机端执行dvr_test即可，小机端可以正常录像到配置的目录下即表示正常。

---------------------------dvr_test---------------------------
camera id : 0,1
arg1 : N : camera number
arg2->arg(N+1) : camera id : /dev/video0 ... /dev/videoN
arg(N+2) : 't' : runing always flag
arg(N+3) : m : suspend delay time(s)

for example:
/dvr_test 1 0          //for test /dev/video0
/dvr_test 1 0 t        //for test /dev/video0 and running always
/dvr_test 1 0 t 99     //suspend after 99 seconds
---------------------------dvr_test---------------------------

1.默认预览60s自动停止，如需一直录像，需要增加t参数
2.默认录像存储位置：/mnt/sdcard/mmcblk1p1/，如需修改可修改/etc/dvrconfig.ini文件下对应的camera节点下cur_filedir属性，其中/dev/video0对应配置文件中的[camera0]
3.T113平台最大支持双录
