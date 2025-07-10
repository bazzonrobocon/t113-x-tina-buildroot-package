## 功能介绍
此demo提供一种GPIO的配置方法

## 使用说明
默认把测试bin安装到rootfs中，可在小机端执行gpio即可。


=========================== usage Ver.V1.1.20221102 ===============================
gpio op io func data dlevel pull   ---set io's function data dlevel pull          =
gpio op io func data               ---set io's func data                          =
gpio op io func                    ---set io's func                               =
gpio op io                         ---get io's status                            =
for example:                                                                      =
gpio -s PH1  1   1     1     1     ---set PH1's function=1 data=1,dlevel=1 pull=1 =
gpio -s PH1  1   0                 ---set PH1's function=1 data=0                 =
gpio -s PH1  1                     ---set PH1's function=1                        =
gpio -g PH1                        ---get PH1's status                            =
===================================================================================