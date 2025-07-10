/*
 * Copyright (C) 2023 Allwinnertech
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/version.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <asm/types.h>
#include <getopt.h>

#include "sunxi_camera_v2.h"
#include "sunxi_display2.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))
#define ALIGN_4K(x) (((x) + (4095)) & ~(4095))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#define display_frame 0

#define log_test 0
int mCaptureFormat = 0;

struct size {
  int width;
  int height;
};
struct buffer {
  void *start;
  int length;
};

typedef enum {
  TVD_PL_YUV420 = 0,
  TVD_MB_YUV420 = 1,
  TVD_PL_YUV422 = 2,
} TVD_FMT_T;

struct disp_screen {
  int x;
  int y;
  int w;
  int h;
};

struct test_layer_info {
  int screen_id;
  int layer_id;
  int mem_id;
  disp_layer_config layer_config;
  int addr_map;
  int width, height; /* screen size */
  int dispfh;        /* device node handle */
  int fh;            /* picture resource file handle */
  int mem;
  int clear; /* is clear layer */
  char filename[32];
  int full_screen;
  unsigned int pixformat;
  disp_output_type output_type;
};

/**
 * tvd_dev info
 */
struct tvd_dev {
  unsigned int ch_id;
  unsigned int height;
  unsigned int width;
  unsigned int interface;
  unsigned int system;
  unsigned int row;
  unsigned int column;
  unsigned int ch0_en;
  unsigned int ch1_en;
  unsigned int ch2_en;
  unsigned int ch3_en;
  unsigned int pixformat;
  struct test_layer_info layer_info;
  int frame_no_to_grap;
  FILE *raw_fp;
};
struct tvd_dev dev;

static char path_name[20];
static char dev_name[20];
static int fd = -1;

struct buffer *buffers;
static unsigned int n_buffers;

struct size input_size;

unsigned int req_frame_num = 8;
unsigned int read_num = 20;
unsigned int count;
int dev_id;
unsigned int fps = 30;

static int disp_set_addr(int width, int height, struct v4l2_buffer *buf)

{
  unsigned long arg[6];
  int ret;

  if (dev.layer_info.pixformat == TVD_PL_YUV420) {
    /* printf("******YUV420!\n"); */
    dev.layer_info.layer_config.info.fb.size[0].width = width;
    dev.layer_info.layer_config.info.fb.size[0].height = height;
    dev.layer_info.layer_config.info.fb.size[1].width = width / 2;
    dev.layer_info.layer_config.info.fb.size[1].height = height / 2;
    dev.layer_info.layer_config.info.fb.size[2].width = width / 2;
    dev.layer_info.layer_config.info.fb.size[2].height = height / 2;
    dev.layer_info.layer_config.info.fb.crop.width = (unsigned long long)width << 32;
    dev.layer_info.layer_config.info.fb.crop.height = (unsigned long long)height << 32;

    dev.layer_info.layer_config.info.fb.addr[0] = buf->m.planes[0].m.mem_offset;
    dev.layer_info.layer_config.info.fb.addr[1] = buf->m.planes[1].m.mem_offset;
    dev.layer_info.layer_config.info.fb.addr[2] = buf->m.planes[2].m.mem_offset;

  } else {
    dev.layer_info.layer_config.info.fb.size[0].width = width;
    dev.layer_info.layer_config.info.fb.size[0].height = height;
    dev.layer_info.layer_config.info.fb.size[1].width = width / 2;
    dev.layer_info.layer_config.info.fb.size[1].height = height;
    dev.layer_info.layer_config.info.fb.size[2].width = width / 2;
    dev.layer_info.layer_config.info.fb.size[2].height = height;
    dev.layer_info.layer_config.info.fb.crop.width = (unsigned long long)width << 32;
    dev.layer_info.layer_config.info.fb.crop.height = (unsigned long long)height << 32;

    dev.layer_info.layer_config.info.fb.addr[0] = buf->m.planes[0].m.mem_offset;
    dev.layer_info.layer_config.info.fb.addr[1] = buf->m.planes[1].m.mem_offset;
    dev.layer_info.layer_config.info.fb.addr[2] = buf->m.planes[2].m.mem_offset;
  }

  dev.layer_info.layer_config.enable = 1;

  arg[0] = dev.layer_info.screen_id;
  arg[1] = (unsigned long)&dev.layer_info.layer_config;
  arg[2] = 1;
  arg[3] = 0;
  ret = ioctl(dev.layer_info.dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);
  if (ret != 0) printf("disp_set_addr fail to set layer info\n");

  return 0;
}

static int read_frame(int mode) {
  struct v4l2_buffer buf;
  char fdstr[50];
  FILE *file_fd = NULL;

  CLEAR(buf);
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (-1 == ioctl(fd, VIDIOC_DQBUF, &buf)) {
    free(buf.m.planes);
    printf("VIDIOC_DQBUF failed\n");
    return -1;
  }

  assert(buf.index < n_buffers);

  int framesize = buf.bytesused;

  switch (mCaptureFormat) {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_H264:
      sprintf(fdstr, "%s/fb%d_y%d_%d_%d.bin", path_name, dev_id, mode, input_size.width,
              input_size.height);
      file_fd = fopen(fdstr, "ab");
      fwrite(buffers[buf.index].start, 1, framesize, file_fd);
      fclose(file_fd);
      break;
    case V4L2_PIX_FMT_MJPEG:
      sprintf(fdstr, "%s/fb%d_y%d_%d_%d_%u.jpg", path_name, dev_id, mode, input_size.width,
              input_size.height, count);
      file_fd = fopen(fdstr, "w");
      fwrite(buffers[buf.index].start, 1, framesize, file_fd);
      fclose(file_fd);
  }

  if (-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
    printf("VIDIOC_QBUF buf.index %d failed\n", buf.index);
    return -1;
  }

  return 0;
}

static struct disp_screen get_disp_screen(int w1, int h1, int w2, int h2) {
  struct disp_screen screen;
  float r1, r2;

  r1 = (float)w1 / (float)w2;
  r2 = (float)h1 / (float)h2;
  if (r1 < r2) {
    screen.w = w2 * r1;
    screen.h = h2 * r1;
  } else {
    screen.w = w2 * r2;
    screen.h = h2 * r2;
  }

  screen.x = (w1 - screen.w) / 2;
  screen.y = (h1 - screen.h) / 2;

  return screen;
}

static int disp_disable(void) {
#if display_frame
  int ret;
  unsigned long arg[6];
  struct disp_layer_config disp;

  /* release memory && clear layer */
  arg[0] = 0;
  arg[1] = 0;
  arg[2] = 0;
  arg[3] = 0;
  ioctl(dev.layer_info.dispfh, DISP_LAYER_DISABLE, (void *)arg);

  /*close channel 0*/
  memset(&disp, 0, sizeof(disp_layer_config));
  disp.channel = 0;
  disp.layer_id = 0;
  disp.enable = 0;
  arg[0] = dev.layer_info.screen_id;
  arg[1] = (unsigned long)&disp;
  arg[2] = 1;
  arg[3] = 0;
  ret = ioctl(dev.layer_info.dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);
  if (ret != 0) printf("disp_disable:disp_set_addr fail to set layer info\n");

  /*close channel 2*/
  memset(&disp, 0, sizeof(disp_layer_config));
  disp.channel = 2;
  disp.layer_id = 0;
  disp.enable = 0;
  arg[0] = dev.layer_info.screen_id;
  arg[1] = (unsigned long)&disp;
  arg[2] = 1;
  arg[3] = 0;
  ret = ioctl(dev.layer_info.dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);
  if (ret != 0) printf("disp_disable:disp_set_addr fail to set layer info\n");

  return ret;
#else
  return 0;
#endif
}

static int disp_init(int width, int height, unsigned int pixformat) {
  /* display_handle* disp = (display_handle*)display; */
  unsigned int arg[6] = {0};
  int layer_id = 0;

  dev.layer_info.screen_id = 0;

  if (dev.layer_info.screen_id < 0) return 0;

  /* open device /dev/disp */
  dev.layer_info.dispfh = open("/dev/disp", O_RDWR);
  if (dev.layer_info.dispfh == -1) {
    printf("open display device fail!\n");
    return -1;
  }

  /* get current output type */
  arg[0] = dev.layer_info.screen_id;
  dev.layer_info.output_type =
      (disp_output_type)ioctl(dev.layer_info.dispfh, DISP_GET_OUTPUT_TYPE, (void *)arg);
  if (dev.layer_info.output_type == DISP_OUTPUT_TYPE_NONE) {
    printf("the output type is DISP_OUTPUT_TYPE_NONE %d\n", dev.layer_info.output_type);
    return -1;
  }

  disp_disable();

  dev.layer_info.pixformat = pixformat;
  dev.layer_info.layer_config.channel = 0;
  dev.layer_info.layer_config.layer_id = layer_id;
  dev.layer_info.layer_config.info.zorder = 1;
  dev.layer_info.layer_config.info.alpha_mode = 1;
  dev.layer_info.layer_config.info.alpha_value = 0xff;
  dev.layer_info.width = ioctl(dev.layer_info.dispfh, DISP_GET_SCN_WIDTH, (void *)arg);
  dev.layer_info.height = ioctl(dev.layer_info.dispfh, DISP_GET_SCN_HEIGHT, (void *)arg);

  dev.layer_info.layer_config.info.mode = LAYER_MODE_BUFFER;

  if (dev.layer_info.pixformat == TVD_PL_YUV420)
    dev.layer_info.layer_config.info.fb.format =
        DISP_FORMAT_YUV420_P; /*DISP_FORMAT_YUV420_P ---- V4L2_PIX_FMT_YUV420M*/
  // DISP_FORMAT_YUV420_SP_UVUV;  /*DISP_FORMAT_YUV420_SP_UVUV ----
  // V4L2_PIX_FMT_NV12*/
  else
    dev.layer_info.layer_config.info.fb.format = DISP_FORMAT_YUV422_SP_VUVU;

  if (dev.layer_info.full_screen == 0 && width < dev.layer_info.width &&
      height < dev.layer_info.height) {
    dev.layer_info.layer_config.info.screen_win.x = (dev.layer_info.width - width) / 2;
    dev.layer_info.layer_config.info.screen_win.y = (dev.layer_info.height - height) / 2;
    if (!dev.layer_info.layer_config.info.screen_win.width) {
      dev.layer_info.layer_config.info.screen_win.width = width;
      dev.layer_info.layer_config.info.screen_win.height = height;
    }
  } else {
    /* struct disp_screen screen; */
    get_disp_screen(dev.layer_info.width, dev.layer_info.height, width, height);
    dev.layer_info.layer_config.info.screen_win.x = 0; /* screen.x; */
    dev.layer_info.layer_config.info.screen_win.y = 0; /* screen.y; */
    dev.layer_info.layer_config.info.screen_win.width = dev.layer_info.width;
    dev.layer_info.layer_config.info.screen_win.height = dev.layer_info.height;
    /* printf("x: %d, y: %d, w: %d, h:
     * %d\n",screen.x,screen.y,screen.w,screen.h); */
  }
  return 0;
}

static void terminate(int sig_no) {
  printf("Got signal %d, exiting ...\n", sig_no);
  disp_disable();
  usleep(20 * 1000);
  exit(1);
}

static void install_sig_handler(void) {
  signal(SIGBUS, terminate);
  signal(SIGFPE, terminate);
  signal(SIGHUP, terminate);
  signal(SIGILL, terminate);
  signal(SIGKILL, terminate);
  signal(SIGINT, terminate);
  signal(SIGIOT, terminate);
  signal(SIGPIPE, terminate);
  signal(SIGQUIT, terminate);
  signal(SIGSEGV, terminate);
  signal(SIGSYS, terminate);
  signal(SIGTERM, terminate);
  signal(SIGTRAP, terminate);
  signal(SIGUSR1, terminate);
  signal(SIGUSR2, terminate);
}

static int req_frame_buffers(void) {
  unsigned int i;
  struct v4l2_requestbuffers req;

  CLEAR(req);
  req.count = req_frame_num;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) {
    printf("VIDIOC_REQBUFS error\n");
    return -1;
  }

  buffers = calloc(req.count, sizeof(*buffers));

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
      printf("VIDIOC_QUERYBUF error\n");
      free(buf.m.planes);
      return -1;
    }

    buffers[n_buffers].length = (int)buf.length;
    printf("buf.length = %d offset %d\n", buf.length, buf.m.offset);
    buffers[n_buffers].start =
        (void *)mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */,
                     MAP_SHARED /* recommended */, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start) {
      printf("mmap failed\n");
      return -1;
    }

#if log_test
    printf("[%u]: addr is 0x%x !\r\n", n_buffers, (unsigned int)buffers[n_buffers].start);
#endif
  }

  for (i = 0; i < n_buffers; ++i) {
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
      printf("VIDIOC_QBUF failed\n");
      return -1;
    }
  }
  return 0;
}

static int free_frame_buffers(void) {
  unsigned int i;

  for (i = 0; i < n_buffers; ++i) {
    if (-1 == munmap(buffers[i].start, buffers[i].length)) {
      printf("munmap error");
      return -1;
    }
  }
  free(buffers);
  return 0;
}

static int subdev_open(int *sub_fd, char *str) {
  char subdev[20] = {'\0'};
  char node[50] = {'\0'};
  char data[20] = {'\0'};
  int i, fs = -1;

  for (i = 0; i < 255; i++) {
    sprintf(node, "/sys/class/video4linux/v4l-subdev%d/name", i);
    fs = open(node, O_RDONLY /* required */ | O_NONBLOCK, 0);
    if (fs < 0) {
      printf("open %s falied\n", node);
      continue;
    }
    /*data_length = lseek(fd, 0, SEEK_END);*/
    lseek(fs, 0L, SEEK_SET);
    read(fs, data, 20);
    close(fs);
    if (!strncmp(str, data, strlen(str))) {
      sprintf(subdev, "/dev/v4l-subdev%d", i);
      printf("find %s is %s\n", str, subdev);
      *sub_fd = open(subdev, O_RDWR | O_NONBLOCK, 0);
      if (*sub_fd < 0) {
        printf("open %s falied\n", str);
        return -1;
      }
      printf("open %s fd = %d\n", str, *sub_fd);
      return 0;
    }
  }
  printf("can not find %s!\n", str);
  return -1;
}

static int tryFmt(int format) {
  struct v4l2_fmtdesc fmtdesc;
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (int i = 0; i < 12; i++) {
    fmtdesc.index = i;
    if (-1 == ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
      break;
    }
    printf("format index = %d, name = %s, v4l2 pixel format = %x\n", i, fmtdesc.description,
           fmtdesc.pixelformat);

    if ((int)fmtdesc.pixelformat == format) {
      return 0;
    }
  }

  return -1;
}

int uvc_flag = 0;

static int camera_init(int sel, int mode) {
  struct v4l2_input inp;
  struct v4l2_streamparm parms;

  fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

  if (fd < 0) {
    printf("open falied\n");
    return -1;
  }
  printf("open %s fd = %d\n", dev_name, fd);

  struct v4l2_capability cap = {0};
  int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (ret < 0) {
    printf("dbg1 VIDIOC_QUERYCAP failed ret=%d\n", ret);
    return -1;
  }

  if (!strcmp((char *)cap.driver, "uvcvideo")) {
    uvc_flag = 1;
    printf("mCameraType = CAMERA_TYPE_UVC\n");
  }
  struct v4l2_input input;

  for (int i = 0; i < 12; i++) {
    input.index = i;
    if (-1 == ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
      break;
    }
    printf("format index = %d, name = %s\n", i, input.name);
  }

  printf("input is %d\r\n", input.index);

  inp.index = sel;
  if (-1 == ioctl(fd, VIDIOC_S_INPUT, &inp)) {
    printf("VIDIOC_S_INPUT %d error!\n", sel);
    return -1;
  }

  CLEAR(parms);
  parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // V4L2_BUF_TYPE_VIDEO_CAPTURE;
  parms.parm.capture.timeperframe.numerator = 1;
  parms.parm.capture.timeperframe.denominator = fps;
  parms.parm.capture.capturemode = V4L2_MODE_VIDEO;  // V4L2_MODE_VIDEO;
  /* parms.parm.capture.capturemode = V4L2_MODE_IMAGE; */
  /*when different video have the same sensor source, 1:use sensor current win,
   * 0:find the nearest win*/

  if (-1 == ioctl(fd, VIDIOC_S_PARM, &parms)) {
    printf("VIDIOC_S_PARM error\n");
    return -1;
  }

  return 0;
}

static int camera_fmt_set(int mode) {
  struct v4l2_format fmt;

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix_mp.width = input_size.width;
  fmt.fmt.pix_mp.height = input_size.height;
  fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;

  switch (mode) {
    case 0:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
      mCaptureFormat = V4L2_PIX_FMT_MJPEG;
      break;
    case 1:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
      mCaptureFormat = V4L2_PIX_FMT_YUYV;
      break;
    case 2:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
      mCaptureFormat = V4L2_PIX_FMT_H264;
      break;
    default:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
      mCaptureFormat = V4L2_PIX_FMT_YUYV;
      break;
  }

  if (tryFmt(fmt.fmt.pix_mp.pixelformat) != 0) {
    printf("not support format %d\n", fmt.fmt.pix_mp.pixelformat);
    return -1;
  }

  printf(" pixel format is %x\n", fmt.fmt.pix_mp.pixelformat);

  if (-1 == ioctl(fd, VIDIOC_S_FMT, &fmt)) {
    printf("VIDIOC_S_FMT error!\n");
    return -1;
  }

  if (-1 == ioctl(fd, VIDIOC_G_FMT, &fmt)) {
    printf("VIDIOC_G_FMT error!\n");
    return -1;
  } else {
    // nplanes = fmt.fmt.pix_mp.num_planes;
    printf("resolution got from sensor = %d*%d num_planes = %d\n", fmt.fmt.pix_mp.width,
           fmt.fmt.pix_mp.height, fmt.fmt.pix_mp.num_planes);
  }

  return 0;
}

static int video_set_control(int cmd, int value) {
  struct v4l2_control control;

  control.id = cmd;
  control.value = value;
  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    printf("VIDIOC_S_CTRL failed\n");
    return -1;
  }
  return 0;
}

static int video_get_control(int cmd) {
  struct v4l2_control control;

  control.id = cmd;
  if (-1 == ioctl(fd, VIDIOC_G_CTRL, &control)) {
    printf("VIDIOC_G_CTRL failed\n");
    return -1;
  }
  return control.value;
}

static int main_test(int sel, int mode) {
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#if display_frame
  unsigned int pixformat;
  int ret;
#endif

  if (-1 == camera_init(sel, mode)) return -1;
  if (-1 == camera_fmt_set(mode)) return -1;
  if (-1 == req_frame_buffers()) return -1;

#if display_frame
  pixformat = TVD_PL_YUV420;
  ret = disp_init(input_size.width, input_size.height, pixformat);
  if (ret) return 2;
#endif

  if (-1 == ioctl(fd, VIDIOC_STREAMON, &type)) {
    printf("VIDIOC_STREAMON failed\n");
    return -1;
  } else
    printf("VIDIOC_STREAMON ok\n");

  count = read_num;
  while (count-- > 0) {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;

      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      tv.tv_sec = 2; /* Timeout. */
      tv.tv_usec = 0;

      r = select(fd + 1, &fds, NULL, NULL, &tv);

      if (-1 == r) {
        if (errno == EINTR) continue;
        printf("select err\n");
      }
      if (r == 0) {
        fprintf(stderr, "select timeout\n");
#ifdef TIMEOUT
        if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &type))
          printf("VIDIOC_STREAMOFF failed\n");
        else
          printf("VIDIOC_STREAMOFF ok\n");
        free_frame_buffers();
        return -1;
#else
        continue;
#endif
      }

      if (!read_frame(mode))
        break;
      else
        return -1;
    }
  }
  disp_disable();
  usleep(20 * 1000);

  if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) {
    printf("VIDIOC_STREAMOFF failed\n");
    return -1;
  } else
    printf("VIDIOC_STREAMOFF ok\n");

  if (-1 == free_frame_buffers()) return -1;
  return 0;
}

// static const char *usbcam_test_short_options = "D:atvh";
static const char *usbcam_test_short_options = "v:s:w:h:o:m:n:f:H";
static struct option usbcam_test_long_options[] = {
    {"video", required_argument, 0, 'v'},  {"sel", required_argument, 0, 's'},
    {"width", required_argument, 0, 'w'},  {"height", required_argument, 0, 'h'},
    {"output", required_argument, 0, 'o'}, {"mode", required_argument, 0, 'm'},
    {"cnt", required_argument, 0, 'n'},    {"fps", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'H'},         {0, 0, 0, 0}};

static void usage(void) {
  fprintf(stderr,
          "usbcam_test -v [videoX] -s [sel] -w [width] -h [height] -o [output_path] -m "
          "[format_mode] -n [test_cnt] -f [fps]\n"
          "    options:\n"
          "    --video|-v <#>   - use the given /dev/video#, default is /dev/video0.\n"
          "    --sel|-s   <#>   - use the subdev-#, deafult is subdev-0.\n"
          "    --width|-w <#>   - camera capture picture width.\n"
          "    --height|-h <#>   - camera capture picture height.\n"
          "    --output|-o<#>   - camera capture save file path, default /tmp/usbcam_test/.\n"
          "    --mode|-m<#>     - camera capture pic format, default 1(YUV422), 0(MJPEG).\n"
          "    --cnt|-n<#>     -  camera capture pic count, default 20.\n"
          "    --fps|-f<#>     -  camera capture fps, default 30.\n"
          "    --help|-H        - show this message\n");
}

int main(int argc, char *argv[]) {
  printf("usbcam test version:%s\n", MODULE_VERSION);
  int test_cnt = 1;
  int sel = 0;
  int width = 640;
  int height = 480;
  int mode = 1;
  struct timeval tv1, tv2;
  float tv;

  install_sig_handler();

  CLEAR(dev_name);
  CLEAR(path_name);

  while (1) {
    int option_index = 0;
    int option_char = 0;

    option_char =
        getopt_long(argc, argv, usbcam_test_short_options, usbcam_test_long_options, &option_index);
    if (option_char == -1) break;

    switch (option_char) {
      case 'v':
        dev_id = atoi(optarg);
        sprintf(dev_name, "/dev/video%d", dev_id);
        break;
      case 's':
        sel = atoi(optarg);
        break;
      case 'w':
        width = atoi(optarg);
        break;
      case 'h':
        height = atoi(optarg);
        break;
      case 'o':
        sprintf(path_name, "%s", optarg);
        break;
      case 'm':
        mode = atoi(optarg);
        break;
      case 'n':
        test_cnt = atoi(optarg);
        break;
      case 'f':
        fps = atoi(optarg);
        break;
      case 'H':
        usage();
        return 0;
      default:
        usage();
        return EINVAL;
    }
  }

  input_size.width = width;
  input_size.height = height;
  read_num = test_cnt;

  gettimeofday(&tv1, NULL);
  if (0 == main_test(sel, mode))
    printf("mode %d test done!!\n", mode);
  else
    printf("mode %d test failed\n", mode);
  close(fd);
  gettimeofday(&tv2, NULL);
  tv = (float)((tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec) / 1000000;
  printf("time cost %f(s)\n", tv);
  return 0;
}
