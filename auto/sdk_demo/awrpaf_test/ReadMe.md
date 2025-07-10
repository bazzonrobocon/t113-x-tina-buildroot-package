## 模块介绍

1、用于 Linux 端向 DSP 端进行通信的 API 测试demo, 框架结构大致如下：
   该 demo 用于演示如何利用这些 API 与 DSP 模块进行交互。
   IOCTL 的封装部分，源码请参照 sdk_lib/sdk_rpaf/。

    +++++++++++++++++++++++++++
    +                         +
    +       Linux App         +
    +                         +
    +++++++++++++++++++++++++++
               /|\
                | IOCTL
               \|/
    +++++++++++++++++++++++++++                  +++++++++++++++++++++++++++
    +                         +       RPC        +                         +
    +       Linux Kernel      +<---------------->+          DSP            +
    +                         +                  +                         +
    +++++++++++++++++++++++++++                  +++++++++++++++++++++++++++

2、具体使用方法可以参照《T113 DSP 模块调试方法汇总》。
3、如不需要使用 T113 内置的DSP，可不集成该组件。
4、该 demo 层次不太清晰，建议只参考其 API 测试的方法跟结果。

## 使用说明

================================Usage================================
 -C,  --card       sound card name
 -F,  --function   function select, 0:pcm stream algo install;
                                    1:pcm stream algo read;
                                    2:independence algo process
 -t,  --algotype   algo type, e.g. 4:EQ; 20:USER
 -i,  --algoindex  the sequence of execution of algo, 0 means the first algo
 -s,  --stream     pcm stream, 0:playback; 1:capture
 -f,  --filepath   file path
 -e,  --enable     used for "algo install", it means enable algo default
 -d,  --duration   used for "algo read", it means the time of algo read process
 -c,  --channels   pcm channels, default 2
 -r,  --rate       pcm rate, default 48000
 -h,  --help       show help

example(algo install):
awrpaf_test -C audiocodec -F 0 -t 4 -i 0 -s 0 -e
  install the algo with the type 4(EQ), index 0(1st algo), stream 0(playback)
  and enable default
example(algo read):
awrpaf_test -C audiocodec -F 1 -t 4 -s 0 -d 3 -f /tmp/awrpaf_test/test.pcm
  read algo data for 3 seconds, with the algo type 4(EQ), stream 0(playback)
  and save algo data into /tmp/awrpaf_test/test.pcm
example(algo process):
awrpaf_test -p 0 -F 2 -t 8 -f /tmp/awrpaf_test/algo_raw_data
================================Usage================================
