## 功能介绍
此demo用于验证npu人脸检测模型功能。

## 使用说明
默认会把测试bin安装到rootfs中，推入模型文件（face_det_v4.0.0_T527.nb）和输入数据文件(face.jpg),只需在小机端执行chmod 777 npu_detect_face./npu_detect_face face_det_v4.0.0_T527.nb bus_face.jpg即可，应用会有人脸区域坐标信息打印。

## 调试信息

```shell
# npu_detect_face face_det_v4.0.0_A523.nb 12.jpg
Read the image....
>[aw_resize_image]: Copy values from source image of the same size.

Partial img ...
       126       147       128       127       148       129
       128       149       130       129       150       131
       130       151       132       131       152       133
       130       151       132       130       151       132
       133       154       135       133       154       135
       132       153       134       131       152       133
       129       150       131       128       149       130
       127       148       129       127       148       129
       128       149       130       129       150       131
       131       152       133       134       155       136
Finish!
Init....
VIPLite driver software version 1.13.0.0-AW-2023-10-19
cid=0x10000016, device_count=1
  device[0] core_count=1
init vip lite, driver version=0x00010d00...
input 0 dim 1440 480 1 1, data_format=2, quant_format=0, name=input[0], none-quant
ouput 0 dim 60 60 63 1, data_format=1, name=uid_141_out_0, none-quant
ouput 1 dim 30 30 63 1, data_format=1, name=uid_149_out_0, none-quant
ouput 2 dim 15 15 63 1, data_format=1, name=uid_151_out_0, none-quant
nbg name=face_det_v4.0.0_A523.nb
create network 3: 1862 us.
memory pool size=2075392byte
Finish!
Run....
score: 0.820466
xmin/ymin/xmax/ymax/w/h: 297 58 353 155 56 97.
Ldmk:
311 95.
341 102.
326 117.
311 125.
334 132.
score: 0.820466
xmin/ymin/xmax/ymax/w/h: 121 92 178 178 57 86.
Ldmk:
134 127.
164 127.
157 142.
142 157.
164 157.
score: 0.786586
xmin/ymin/xmax/ymax/w/h: 226 128 282 225 56 97.
Ldmk:
238 166.
268 166.
253 189.
238 196.
268 204.
Finish!
Free....
Finish!
#

```

