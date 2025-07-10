## 编译说明
1.直接在此目录执行build.sh可以进行全部编译，同时把sdk_demo/bin和sdk_lib/libs下的文件拷贝到out/buildroot下，即默认打包到img中；

2.局部编译，可以进入到子目录去执行make，编译成功后sdk_demo会默认拷贝到sdk_dem/bin/下,sdk_lib下的库会拷贝到sdk_lib/libs下，但是不会自动拷贝到out/buildroot；

3.每次编译前，最好先执行clean操作，防止没有编译到的情况出现；

4.sdk_demo下的大部分的demo都依赖于../sdk_lib/libs和../sdk_lib/cedarx/lib目录下的so，请注意Makefile的编写；

5.每个demo都需要创建对应的ReadMe.md以及用adb执行的脚本，用于介绍此demo功能的使用以及预期结果；

## 注意事项
1.编写文件请注意统一使用utf-8编码；

2.各模块中log打印建议统一使用sdk_log封装后的打印格式，即ALOGx，并在Makefile中定义USE_LOGCAT等宏；

3.每个模块请配套编写ReadMe.md，包含功能介绍，使用说明等；

4.每个模块请配套编写push.bat，方便进行模块测试；

5.每个模块如存在单独的测试资源文件，请建立testfiles文件夹进行管理；

6.每个模块建议使用相同格式的Makefile文件，请尽量只添加真实的依赖库以及头文件；

7.如果模块中存在依赖的资源文件，建议不用打包到img中，可以使用adb push的方式进行手动测试；

8.各个模块名请使用下划线命名法，sdk_demo下使用如xxx_test，sdk_lib下使用如sdk_xxx；

## sdklog使用说明
1.为了统一sdkdemo与sdklib的打印格式，故增加sdklog库进行统一封装；

2.打印函数参考Android环境下的ALOG打印方案，定义5个等级的打印方法，等级从低到高排序为ALOGV/ALOGD/ALOGI/ALOGW/ALOGE；

3.各个模块需要定义USE_LOGCAT才能输出，否则只能输出无格式的ALOGW与ALOGE；

4.不再使用sdk_log_setlevel函数进行设置全局打印等级，打印等级设置可参考LOG_LEVEL的说明；

5.各个模块也可以针对本模块设置打印等级(LOG_LEVEL)，默认打印等级为3（ALOGE/ALOGW/ALOGD），即如果Makefile中定义了-DUSE_LOGCAT -DLOG_LEVEL=6，则会优先采用等级6的打印方案进行log输出；

6.为了区分模块的功能文件，需要在c/cpp中定义LOG_TAG，用于区别不同文件产生的打印标志；

7.USE_LOGCAT/LOG_LEVEL/LOG_TAG均需要定义在sdklog.h头文件之前，否则会出现定义无效的情况，建议USE_LOGCAT/LOG_LEVEL定义在Makefile中，LOG_TAG定义在c/cpp文件中；

8.sdklog中提供当前时间戳的功能，可以在sdklog.c中注释HAVE_LOCALTIME_R的定义，即可取消时间戳的相关信息；

## 完整目录结构

注：不同的SDK，对应的目录结构可能不同，具体可以参考sdk_demo/sdk_lib下Makefile文件中定义的SUBDIRS变量
.
├── build.sh                    // 完整编译demo的脚本
├── ReadMe.md                   // 本模块的使用说明及注意事项
├── makefile_cfg                // sdk_demo与sdk_lib公共的Makefile属性
├── rootfs                      // sdk_demo依赖的rootfs配置文件等
│   ├── etc
│   ├── lib
│   └── usr
├── sdk_demo
│   ├── alpha_test              // 图片透明度叠加显示
│   ├── audioenc_test           // 音频编码demo
│   ├── autplayer_test          // autplayer解码demo
│   ├── awrpaf_test             // dsp测试demo
│   ├── bin                     // 所有demo编译后生成的bin文件
│   ├── csi_test                // csi抓图demo
│   ├── decoder_test            // 硬解码demo
│   ├── dsp_debug_test          // dsp模块debug数据
│   ├── encoder_test            // 硬编码demo
│   ├── fbinit_test             // fb初始化demo
│   ├── g2d_test                // 2D图形加速demo
│   ├── gif2rgb_test            // gif转rgb格式demo
│   ├── lcd_bright_test         // lcd亮度调节demo
│   ├── Makefile
│   ├── mem_test                // ion内存demo
│   ├── png_decode_test         // png格式解码demo
│   ├── record_test             // 录像demo
│   ├── sdk_test                // camera录像demo
│   ├── stream_player_test      // 裸流解码demo
│   ├── tpadc_test              // 触摸坐标demo
│   ├── tvd_test                // cvbs摄像头抓图demo
│   ├── usbcam_test             // usb摄像头抓图demo
│   └── yuv_test                // yuv显示demo
└── sdk_lib
    ├── cedarx                  // cedarx头文件
    ├── include                 // sdk_lib库的头文件
    ├── libs                    // 所有lib编译后生成的so文件
    ├── Makefile
    ├── push.bat
    ├── sdk_audioenc            // 音频编码封装库
    ├── sdk_camera              // Camera封装库
    ├── sdk_config              // 配置项封装
    ├── sdk_decoder             // 解码封装库
    ├── sdk_encoder             // 编码封装库
    ├── sdk_log                 // 格式化log输出库
    ├── sdk_memory              // ion内存封装库
    ├── sdk_misc                // 工具类接口封装
    ├── sdk_rpaf                // dsp通信封装库
    ├── sdk_rpaf_plugin         // dsp模块插件
    ├── sdk_shm                 // 共享内存封装库
    ├── sdk_sound               // 音频封装库
    ├── sdk_storage             // 文件回写封装库
    └── sdk_stream_player       // 裸流解码封装库