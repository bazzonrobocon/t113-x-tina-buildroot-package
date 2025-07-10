## 功能介绍
此demo用于验证npu物体检测模型功能。

## 使用说明
默认把测试bin安装到rootfs中，推入模型文件（4.0.0_Beta.nb）和输入数据文件(640*384的yuv图片),只需在小机端执行chmod 777 detect_person;./detect_person AwDetPersion_3.0.3_Beta.nb bus_640x384.yuv即可，应用会有人形区域坐标信息打印。

支持11类物体的检测

person
car
bird
cat
dog
pottedplant
diningtable
tvmonitor
refrigerator
clock
chair

## 调试说明

```shell
$ ./npu_detect_test 4.0.0_Beta.nb bus_640x384.yuv

VIPLite driver software version 1.13.0.0-AW-2023-10-19
Num: 3
0: cls 0, prob 0.498999, rect [542, 11, 639, 384]
1: cls 0, prob 0.467779, rect [48, 16, 161, 382]
2: cls 0, prob 0.385399, rect [179, 9, 284, 344]
```

