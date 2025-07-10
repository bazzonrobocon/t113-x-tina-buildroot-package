## 功能介绍

此demo用于验证G2D模块的相关功能，如格式转换/缩放/旋转/YUV图像合成/YUV图像拆分等。

## 使用说明

不同平台的 G2D 支持模式不同，所以对应该 demo 也有差异，默认会通过 g_avail_mode 来差异开不同平台的支持模式。如果兼容不同平台的时候，要在 Makefile 中 DEFINES 来指定以下使用平台，如下：

```
DEFINES += -D__T527__
```

此 demo 里面将对应模式分为了以下几种：

```
enum G2dmode {
    G2D_SCALE = 0,
    G2D_DECOMPOSE,
    G2D_COMPOSE,
    G2D_ROTATE,
    G2D_HFLIP,
    G2D_VFLIP,
    G2D_FORMAT_CONVERT_RGBA2YUV_ByFd,
    G2D_FORMAT_CONVERT_YUV2RGBA_ByFd,
    G2D_FORMAT_CONVERT_YUV2RGBA_ByPhy,
    G2D_MODE_NULL,
};
```

默认不会把测试bin安装到rootfs中，需要手动执行push.bat脚本，请到testfiles目录找对应功能下的资源文件中.bat，双击执行即可，bat脚本会自动push所需资源文件，然后执行g2d_test，最后把转换之后的文件在pull出来。

================Usage================
g2d_test 0        means: 1024x600*rgba scale to 1280x720*rgba
g2d_test 1        means: 1*2560x1440 nv21 decompose to 4*720p nv21
g2d_test 2        means: 4*720 nv21 compose to 1*2560x1440 nv21
g2d_test 3        means: 1280x720 nv21 rotate to 720x1280 nv21
g2d_test 4        means: 1024x600 rgba vertical flips
g2d_test 6        means: 1024x600 rgba convert to 1024x600 nv21
g2d_test 7        means: 1024x600 nv21 convert to 1024x600 rgba
g2d_test 8        means: 1024x600 nv21 convert to 1024x600 rgba by phy address
================usage================
