## 功能介绍
此demo验证支持RGB888/RGB565/YV12图片颜色格式显示

## 使用说明
默认不会把测试bin安装到rootfs中，需要手动执行push.bat脚本，把测试程序以及测试资源文件push到小机端，然后在小机端执行yuvtest即可，小机端可以正常显示对应的资源图片即表示功能正常。

程序默认测试RGB565格式数据，如需测试另外格式，需要修改源码，打开对应的宏定义即可

#define INPUT_BMP_PATH "/tmp/yuv_test/bike_1280x720_565"
#define DISP_FMT DISP_FORMAT_RGB_565

// #define INPUT_BMP_PATH "/tmp/yuv_test/bike_1280x720_888"
// #define DISP_FMT DISP_FORMAT_RGB_888

// #define INPUT_BMP_PATH "/tmp/yuv_test/bike_1280x720_yv12.yuv"
// #define DISP_FMT DISP_FORMAT_YUV420_P
