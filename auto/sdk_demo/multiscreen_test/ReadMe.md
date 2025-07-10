## 功能介绍
此demo用于演示双屏异显，仅供参考

## 使用说明

1. 此demo依赖部分库，具体可查看Makefile：
> libsdk_disp.so  libsdk_log.so

方法：

1. 需要LVDS屏幕，同时插上HDMI线，连接显示器

2. 确认板子上有依赖的库
   
3. windows电脑运行push.bat即可

效果：

在LVDS上显示一块绿色（注意可能会被其他界面覆盖，请确保无其他程序操作屏幕，可执行fbinit清除），HDMI屏幕上显示色条。并在若干秒后停止显示

备注：vga输出未进行实际测试

