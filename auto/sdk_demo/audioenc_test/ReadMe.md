## 功能介绍
此demo用于验证本地读取wav文件的audio数据，编码成mp3格式的文件。

## 使用说明
默认不会把测试bin安装到rootfs中，需要手动执行push.bat脚本，把测试程序以及测试资源文件push到小机端，然后在小机端执行audioenc_test即可，demo会编码生成一个mp3文件，最后再pull出来即可。


备注：
1.编码生成的mp3无法播放，待查