/*
 * drivers/media/platform/sunxi-vin/vin_test/mplane_image/csi_test_mplane.c
 *
 * Copyright (c) 2014 softwinner.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * zw
 * for csi & isp test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/version.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>

#include "sunxi_camera_v2.h"
#include "sunxi_display2.h"

//#include "DmaIon.h"
#include "sunxiMemInterface.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))
#define ALIGN_4K(x) (((x) + (4095)) & ~(4095))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#define display_frame 1
#define video_s_ctrl 0

#include "hwdisp2.h"
#define PIC_W  1920     // camera size
#define PIC_H  1080
#define SCREEN_W 1280   // screen size
#define SCREEN_H 800

int gCameraDmafd[10];

static android::HwDisplay* gdisp;
struct de_info_config {
    int screen_id;
    int ch_id;
    int lyr_id;
    int layer0;
};
struct de_info_config disp_info;

struct size {
    int width;
    int height;
};

struct buffer {
    void *start[3];
    int length[3];
    void *phy_addr[3];
    unsigned int fd[3];
};

dma_mem_des_t gMem;
struct video_buf {
    struct buffer buffers;
    dma_mem_des_t vbuf;
};
struct video_buf *vin_buf;

struct video_config {
    int dev_id;
    char dev_name[20];
    char path_name[20];
    int test_cnt;
    int sel;
    int width;
    int height;
    int mode;
    unsigned int fps;
    unsigned int wdr_mode;
    enum v4l2_buf_type buf_type;    // V4L2 buffer type  ---- V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE / V4L2_BUF_TYPE_VIDEO_CAPTURE
    enum v4l2_memory v4l2_memtype;  // V4L2 Memory type  ---- V4L2_MEMORY_MMAP / V4L2_MEMORY_USERPTR
    int alloc_memtype;              // Video Memory type ---- MEM_TYPE_CDX_NEW / MEM_TYPE_DMA
};
struct video_config vconfig;

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
    int width, height;/* screen size */
    int dispfh;/* device node handle */
    int fh;/* picture resource file handle */
    int mem;
    int clear;/* is clear layer */
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

// static char path_name[20];
// static char dev_name[20];
static int fd = -1;
#ifdef SUBDEV_TEST
static int isp0_fd = -1;
static int isp1_fd = -1;
#endif
static unsigned int n_buffers;

struct size input_size;

unsigned int req_frame_num = 3;
unsigned int read_num = 20;
unsigned int count;
unsigned int nplanes;
unsigned int save_flag;

#define ROT_90 0

#define DEFAULT_FILE_PATH "/tmp/csi_test/"

void printUsage()
{
    printf("==========================Usage==========================\n");
    printf("csi_test_usrptr <videoX> <sel> <width> <height> <path> <format_mode> <test_cnt> <fps>\n");
    printf("videoX          means: open /dev/videoX\n");
    printf("sel             means: select subdev-X\n");
    printf("width           means: camera capture pic width\n");
    printf("height          means: camera capture pic height\n");
    printf("path            means: camera capture save file path, default \"/tmp/tvd_test/\"\n");
    printf("format_mode     means: camera capture pic format, default 4(NV21)\n");
    printf("test_cnt        means: camera capture pic count, default 20\n");
    printf("fps             means: camera capture fps, default 30\n");
    printf("==========================Usage==========================\n");
}

static int free_frame_buffers(void);

static void disp_show(int dmafd, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    disp_info.screen_id = 0;
    disp_info.ch_id = 0;
    disp_info.lyr_id = 0;

    if (gdisp == NULL) {
        printf("gdisp is NULL\n");
        return ;
    }

    struct view_info frame = {0, 0, w, h};
    disp_info.layer0 = gdisp->aut_hwd_layer_request(&frame, disp_info.screen_id, disp_info.ch_id, disp_info.lyr_id);
    gdisp->aut_hwd_layer_sufaceview(disp_info.layer0, &frame);

    struct src_info src = {PIC_W, PIC_H, DISP_FORMAT_YUV420_SP_VUVU};
    gdisp->aut_hwd_layer_set_src(disp_info.layer0, &src, 0, dmafd);

    struct view_info crop = {0, 0, PIC_W, PIC_H};
    gdisp->aut_hwd_layer_set_rect(disp_info.layer0, &crop);

    gdisp->aut_hwd_layer_set_zorder(disp_info.layer0, 20);

    gdisp->aut_hwd_layer_open(disp_info.layer0);
}

static void releaseDE()
{
    if (gdisp == NULL) {
        printf("gdisp is NULL\n");
        return ;
    }

    gdisp->aut_hwd_layer_close(disp_info.layer0);
}

static void yuv_r90(char *dst, char *src, int width, int height)
{
    int i = 0, j = 0;

    for (i = 0; i < width; i++) {
        for (j = 0; j < height; j++)
            *(char *)(dst + j + i * height) = *(char *)(src + (height - j - 1) * width + i);
    }
}

static void uv_r90(char *dst, char *src, int width, int height)
{
    int i = 0, j = 0;

    for (i = 0; i < width / 2; i++) {
        for (j = 0; j < height / 2; j++)
            *(char *)(dst + j * 2 + i * height) = *(char *)(src + (height / 2 - j - 1) * width + i * 2);
    }

    for (i = 0; i < width / 2; i++) {
        for (j = 0; j < height / 2; j++)
            *(char *)(dst + j * 2 + 1 + i * height) = *(char *)(src + (height / 2 - j - 1) * width + i * 2 + 1);
    }
}

static int read_frame(int mode)
{
    struct v4l2_buffer buf;
    char fdstr[50];
    FILE *file_fd = NULL;

    CLEAR(buf);
    buf.type = vconfig.buf_type;
    buf.memory = vconfig.v4l2_memtype;//V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;
    buf.length = nplanes;
    buf.m.planes =
        (struct v4l2_plane *)calloc(nplanes, sizeof(struct v4l2_plane));

    if (-1 == ioctl(fd, VIDIOC_DQBUF, &buf)) {
        free(buf.m.planes);
        printf("VIDIOC_DQBUF failed\n");
        return -1;
    }

    assert(buf.index < n_buffers);

    if (save_flag == 0) {
        if ((count == read_num / 2) || ((count > 0) && (nplanes == 1))) {
            printf("file length = %d %d %d\n", vin_buf[buf.index].buffers.length[0],
                  vin_buf[buf.index].buffers.length[1],
                  vin_buf[buf.index].buffers.length[2]);
            printf("file start = %p %p %p\n", vin_buf[buf.index].buffers.start[0],
                  vin_buf[buf.index].buffers.start[1],
                  vin_buf[buf.index].buffers.start[2]);

            switch (nplanes) {
                case 1:
                    sprintf(fdstr, "%s/fb%d_y%d_%d_%d_%u.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height, count);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    break;
                case 2:
#if ROT_90
                    dst = (char *)malloc(vin_buf[buf.index].buffers.length[0]);
                    yuv_r90(dst, vin_buf[buf.index].buffers.start[0], input_size.width, input_size.height);
                    sprintf(fdstr, "%s/fb%d_y%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.height, input_size.width);
                    file_fd = fopen(fdstr, "w");
                    fwrite(dst, vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    free(dst);

                    dst = (char *)malloc(vin_buf[buf.index].buffers.length[1]);
                    uv_r90(dst, vin_buf[buf.index].buffers.start[1], input_size.width, input_size.height);
                    sprintf(fdstr, "%s/fb%d_uv%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.height, input_size.width);
                    file_fd = fopen(fdstr, "w");
                    fwrite(dst, vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);
                    free(dst);
#else
                    sprintf(fdstr, "%s/fb%d_y%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    sprintf(fdstr, "%s/fb%d_uv%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[1], vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);
#endif
                    break;
                case 3:
#if ROT_90
                    dst = (char *)malloc(vin_buf[buf.index].buffers.length[0]);
                    yuv_r90(dst, vin_buf[buf.index].buffers.start[0], input_size.width, input_size.height);
                    sprintf(fdstr, "%s/fb%d_y%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.height, input_size.width);
                    file_fd = fopen(fdstr, "w");
                    fwrite(dst, vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    free(dst);

                    dst = (char *)malloc(vin_buf[buf.index].buffers.length[1]);
                    yuv_r90(dst, vin_buf[buf.index].buffers.start[1], input_size.width / 2, input_size.height / 2);
                    sprintf(fdstr, "%s/fb%d_u%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.height, input_size.width);
                    file_fd = fopen(fdstr, "w");
                    fwrite(dst, vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);
                    free(dst);

                    dst = (char *)malloc(vin_buf[buf.index].buffers.length[2]);
                    yuv_r90(dst, vin_buf[buf.index].buffers.start[2], input_size.width / 2, input_size.height / 2);
                    sprintf(fdstr, "%s/fb%d_v%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.height, input_size.width);
                    file_fd = fopen(fdstr, "w");
                    fwrite(dst, vin_buf[buf.index].buffers.length[2], 1, file_fd);
                    fclose(file_fd);
                    free(dst);
#else
                    sprintf(fdstr, "%s/fb%d_y%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);

                    sprintf(fdstr, "%s/fb%d_u%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[1], vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);

                    sprintf(fdstr, "%s/fb%d_v%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "w");
                    fwrite(vin_buf[buf.index].buffers.start[2], vin_buf[buf.index].buffers.length[2], 1, file_fd);
                    fclose(file_fd);
#endif
                    break;
                default:
                    break;
            }
        }
    } else if (save_flag == 1) {
        if ((count > 0)) {
            switch (nplanes) {
                case 1:
                    sprintf(fdstr, "%s/fb%d_yuv%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "ab");
                    if (1920 == input_size.width && 1080 == input_size.height)
                        fwrite(vin_buf[buf.index].buffers.start[0], 1920*1080*3/2, 1, file_fd);
                    else
                        fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    break;
                case 2:
                    sprintf(fdstr, "%s/fb%d_yuv%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "ab");
                    fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    file_fd = fopen(fdstr, "ab");
                    fwrite(vin_buf[buf.index].buffers.start[1], vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);
                    break;
                case 3:
                    sprintf(fdstr, "%s/fb%d_yuv%d_%d_%d.bin", vconfig.path_name, vconfig.dev_id, mode, input_size.width, input_size.height);
                    file_fd = fopen(fdstr, "ab");
                    fwrite(vin_buf[buf.index].buffers.start[0], vin_buf[buf.index].buffers.length[0], 1, file_fd);
                    fclose(file_fd);
                    file_fd = fopen(fdstr, "ab");
                    fwrite(vin_buf[buf.index].buffers.start[1], vin_buf[buf.index].buffers.length[1], 1, file_fd);
                    fclose(file_fd);
                    file_fd = fopen(fdstr, "ab");
                    fwrite(vin_buf[buf.index].buffers.start[2], vin_buf[buf.index].buffers.length[2], 1, file_fd);
                    fclose(file_fd);
                    break;
                default:
                    break;
            }
        }
    } else if (save_flag == 2) {
        if (count <= 1)
            count = read_num;
#if display_frame
        disp_show(vin_buf[buf.index].vbuf.ion_buffer.fd_data.aw_fd, 0, 0, SCREEN_W, SCREEN_H);
#endif
    } else {
        count = 0;
    }

    buf.m.planes[0].m.fd = vin_buf[buf.index].buffers.fd[0];
    buf.m.planes[0].length = vin_buf[buf.index].buffers.length[0];
    if (-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
        printf("VIDIOC_QBUF buf.index %d failed\n", buf.index);
        free(buf.m.planes);
        return -1;
    }

    free(buf.m.planes);

    return 0;
}

static void terminate(int sig_no)
{
    printf("Got signal %d, exiting ...\n", sig_no);

    if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &vconfig.buf_type)) {
        printf("VIDIOC_STREAMOFF failed\n");
    } else
        printf("VIDIOC_STREAMOFF ok\n");

    close(fd);

    free_frame_buffers();

#if display_frame
    releaseDE();
#endif

    usleep(20 * 1000);
    exit(1);
}

static void install_sig_handler(void)
{
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

static int req_frame_buffers(void)
{
    unsigned int i;
    struct v4l2_requestbuffers req;
#if 0
    struct v4l2_exportbuffer exp;
#endif
    CLEAR(req);
    req.count = req_frame_num;
    req.type = vconfig.buf_type;
    req.memory = vconfig.v4l2_memtype;//V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;
    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) {
        printf("VIDIOC_REQBUFS error\n");
        return -1;
    }

    vin_buf = (struct video_buf*)calloc(req.count, sizeof(*vin_buf));

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = vconfig.buf_type;
        buf.memory = vconfig.v4l2_memtype;//V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        buf.length = nplanes;
        buf.m.planes =
            (struct v4l2_plane *)calloc(nplanes,
                                        sizeof(struct v4l2_plane));
        if (buf.m.planes == NULL) {
            printf("buf.m.planes calloc failed!\n");
            return -1;
        }
        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            printf("VIDIOC_QUERYBUF error\n");
            free(buf.m.planes);
            return -1;
        }
        printf("VIDIOC_QUERYBUF buf.m.planes[j].length=%d\n", buf.m.planes[0].length);
        switch (buf.memory) {
            case V4L2_MEMORY_MMAP:
                for (i = 0; i < nplanes; i++) {
                    vin_buf[n_buffers].buffers.length[i] = buf.m.planes[i].length;
                    vin_buf[n_buffers].buffers.start[i] =
                        mmap(NULL,/* start anywhere */
                             buf.m.planes[i].length,
                             PROT_READ | PROT_WRITE,/* required */
                             MAP_SHARED, /* recommended */
                             fd, buf.m.planes[i].m.mem_offset);

                    if (vin_buf[n_buffers].buffers.start[i] == MAP_FAILED) {
                        printf("mmap failed\n");
                        free(buf.m.planes);
                        return -1;
                    }
#if 0
                    CLEAR(exp);
                    exp.type = vconfig.buf_type;
                    exp.index = n_buffers;
                    exp.plane = i;
                    exp.flags = O_CLOEXEC;
                    if (-1 == ioctl(fd, VIDIOC_EXPBUF, &exp)) {
                        printf("VIDIOC_EXPBUF error\n");
                        return -1;
                    }
                    printf("buffer %d plane %d DMABUF fd is %d\n", n_buffers, i, exp.fd);
#endif
                }
                free(buf.m.planes);
                break;
            case V4L2_MEMORY_USERPTR:
            case V4L2_MEMORY_DMABUF:
                for (int j = 0; j < (int)nplanes; j++) {
                    vin_buf[n_buffers].buffers.length[j] = ALIGN_16B(ALIGN_16B(input_size.width) *
                        ALIGN_16B(input_size.height) * 3 / 2);
                    gMem.size = (unsigned int)vin_buf[n_buffers].buffers.length[j];
                    int ret = allocAlloc(vconfig.alloc_memtype, &gMem, NULL);
                    if (ret < 0) {
                        printf("allocAlloc failed.\n");
                        return -1;
                    }

                    vin_buf[n_buffers].buffers.phy_addr[j] = (void *)gMem.phy;
                    vin_buf[n_buffers].buffers.start[j] = (void *)gMem.vir;
                    if (vin_buf[n_buffers].buffers.start[j] == NULL) {
                        printf("Camera v4l2QueryBuf buffer allocate ERROR\n");
                        return -1;
                    }
                    vin_buf[n_buffers].buffers.fd[j] = gMem.ion_buffer.fd_data.aw_fd;

                    vin_buf[n_buffers].vbuf.size = gMem.size;
                    vin_buf[n_buffers].vbuf.phy = gMem.phy;
                    vin_buf[n_buffers].vbuf.vir = gMem.vir;
                    vin_buf[n_buffers].vbuf.ops = gMem.ops;
                    vin_buf[n_buffers].vbuf.ion_buffer.fd_data.aw_fd = gMem.ion_buffer.fd_data.aw_fd;
                }
                break;
        }
    }

    for (i = 0; i < req_frame_num; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = vconfig.buf_type;
        buf.memory = vconfig.v4l2_memtype;//V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = nplanes;
        buf.m.planes =
            (struct v4l2_plane *)calloc(nplanes,
                                        sizeof(struct v4l2_plane));
        for (int j = 0; j < (int)nplanes; j++) {
            buf.m.planes[j].m.userptr = (unsigned long)vin_buf[i].buffers.start[j];
            buf.m.planes[j].length = vin_buf[i].buffers.length[j];
            if((V4L2_MEMORY_USERPTR == vconfig.alloc_memtype )||
                    (V4L2_MEMORY_DMABUF == vconfig.alloc_memtype )) {
                buf.m.planes[j].m.fd =
                        vin_buf[i].buffers.fd[j];
            }
        }


        if (-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
            printf("VIDIOC_QBUF failed\n");
            free(buf.m.planes);
            allocFree(vconfig.alloc_memtype, (dma_mem_des_t *)vin_buf[i].buffers.phy_addr[0], NULL);
            return -1;
        }
        free(buf.m.planes);
//      allocFree(vconfig.alloc_memtype,(void *)buffers[i].phy_addr[0],NULL);
    }
    return 0;
}

static int free_frame_buffers(void)
{
    unsigned int i, j;

    for (i = 0; i < n_buffers; ++i) {
        for (j = 0; j < nplanes; j++)
            if ((V4L2_MEMORY_USERPTR == vconfig.v4l2_memtype) || (V4L2_MEMORY_DMABUF == vconfig.v4l2_memtype)) {
                allocFree(vconfig.alloc_memtype, (dma_mem_des_t *)&vin_buf[i].vbuf, NULL);
                vin_buf[i].buffers.start[j] = NULL;
                vin_buf[i].buffers.phy_addr[j] = NULL;
            } else if (V4L2_MEMORY_MMAP == vconfig.v4l2_memtype) {
                if (-1 == munmap(vin_buf[i].buffers.start[j], vin_buf[i].buffers.length[j])) {
                    printf("munmap error\n");
                    return -1;
                }
            }
    }

    free(vin_buf);

    int ret = allocClose(vconfig.alloc_memtype, &gMem, NULL);
    if (ret < 0) {
        printf("allocClose failed.\n");
        return -1;
    }

    return 0;
}

static int subdev_open(int *sub_fd, char *str)
{
    char subdev[20] = {'\0'};
    char node[50] = {'\0'};
    char data[20] = {'\0'};
    int i, fs = -1;

    for (i = 0; i < 255; i++) {
        sprintf(node, "/sys/class/video4linux/v4l-subdev%d/name", i);
        fs = open(node, O_RDONLY/* required */ | O_NONBLOCK, 0);
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

static int camera_init(int sel, int mode)
{
    struct v4l2_input inp;
    struct v4l2_streamparm parms;

    fd = open(vconfig.dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);

    if (fd < 0) {
        printf("open %s falied\n", vconfig.dev_name);
        return -1;
    }
    printf("open %s fd = %d\n", vconfig.dev_name, fd);

#ifdef SUBDEV_TEST
    if (-1 == subdev_open(&isp0_fd, "sunxi_isp.0"))
        return -1;
    if (-1 == subdev_open(&isp1_fd, "sunxi_isp.1"))
        return -1;
#endif

    inp.index = sel;
    if (-1 == ioctl(fd, VIDIOC_S_INPUT, &inp)) {
        printf("VIDIOC_S_INPUT %d error!\n", sel);
        return -1;
    }

    CLEAR(parms);
    parms.type = vconfig.buf_type;
    parms.parm.capture.timeperframe.numerator = 1;
    parms.parm.capture.timeperframe.denominator = vconfig.fps;
    parms.parm.capture.capturemode = V4L2_MODE_VIDEO;
    /* parms.parm.capture.capturemode = V4L2_MODE_IMAGE; */
    /*when different video have the same sensor source, 1:use sensor current win, 0:find the nearest win*/
    parms.parm.capture.reserved[0] = 0;
    parms.parm.capture.reserved[1] = vconfig.wdr_mode;/*2:command, 1: wdr, 0: normal*/

    if (-1 == ioctl(fd, VIDIOC_S_PARM, &parms)) {
        printf("VIDIOC_S_PARM error\n");
        return -1;
    }

    return 0;
}

static int camera_fmt_set(int mode)
{
    struct v4l2_format fmt;

    CLEAR(fmt);
    fmt.type = vconfig.buf_type;
    fmt.fmt.pix_mp.width = input_size.width;
    fmt.fmt.pix_mp.height = input_size.height;
    switch (mode) {
        case 0:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
            break;
        case 1:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
            break;
        case 2:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
            break;
        case 3:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
            break;
        case 4:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV21;
            break;
        case 5:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR10;
            break;
        case 6:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR12;
            break;
        case 7:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_LBC_2_5X;
            break;
        default:
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV21;
            break;
    }
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    printf("fmt.fmt.pix_mp.pixelformat=%d\n", fmt.fmt.pix_mp.pixelformat);
    if (-1 == ioctl(fd, VIDIOC_S_FMT, &fmt)) {
        printf("VIDIOC_S_FMT error!\n");
        return -1;
    }

    if (-1 == ioctl(fd, VIDIOC_G_FMT, &fmt)) {
        printf("VIDIOC_G_FMT error!\n");
        return -1;
    } else {
        nplanes = fmt.fmt.pix_mp.num_planes;
        printf("resolution got from sensor = %d*%d num_planes = %d\n",
              fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
              fmt.fmt.pix_mp.num_planes);
    }

    return 0;
}

static int video_set_control(int cmd, int value)
{
    struct v4l2_control control;

    control.id = cmd;
    control.value = value;
    if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
        printf("VIDIOC_S_CTRL failed\n");
        return -1;
    }
    return 0;
}

static int video_get_control(int cmd)
{
    struct v4l2_control control;

    control.id = cmd;
    if (-1 == ioctl(fd, VIDIOC_G_CTRL, &control)) {
        printf("VIDIOC_G_CTRL failed\n");
        return -1;
    }
    return control.value;
}

static int main_test(int sel, int mode)
{
#ifdef SUBDEV_TEST
    struct v4l2_ext_control ctrls[4];
    struct v4l2_ext_controls ext_ctrls;
    struct v4l2_control control;
    unsigned int pixformat;
    int ret;
    int i = 0;
#endif

#if video_s_ctrl
    int j = 0;
#endif
    if (-1 == camera_init(sel, mode))
        return -1;
    if (-1 == camera_fmt_set(mode))
        return -1;
    if (-1 == req_frame_buffers())
        return -1;

    if (-1 == ioctl(fd, VIDIOC_STREAMON, &vconfig.buf_type)) {
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
#if video_s_ctrl
            if (count % 3 == 0) {
                if (j == 0) {
                    video_set_control(V4L2_CID_VFLIP, 0);
                    video_set_control(V4L2_CID_HFLIP, 0);
                    j = 1;
                    printf("V4L2_CID_VFLIP done, j = %d, count = %d\n", j, count);
                } else {
                    video_set_control(V4L2_CID_VFLIP, 1);
                    video_set_control(V4L2_CID_HFLIP, 1);
                    j = 0;
                    printf("V4L2_CID_VFLIP no done, j = %d, count = %d\n", j, count);
                }
            }
#endif
#ifdef SUBDEV_TEST
            for (i = 0; i < 4; i++) {
                ctrls[i].id = V4L2_CID_R_GAIN + i;
                ctrls[i].value = count % 256;
            }
            memset(&ext_ctrls, 0, sizeof(ext_ctrls));
            ext_ctrls.ctrl_class = V4L2_CID_R_GAIN;
            ext_ctrls.count = 4;
            ext_ctrls.controls = ctrls;
            ioctl(isp0_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);

            for (i = 0; i < 4; i++) {
                ctrls[i].id = V4L2_CID_AE_WIN_X1 + i;
                ctrls[i].value = count * 16 % 256;
            }
            memset(&ext_ctrls, 0, sizeof(ext_ctrls));
            ext_ctrls.ctrl_class = V4L2_CID_AE_WIN_X1;
            ext_ctrls.count = 4;
            ext_ctrls.controls = ctrls;
            ioctl(isp0_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);

            for (i = 0; i < 4; i++) {
                ctrls[i].id = V4L2_CID_AF_WIN_X1 + i;
                ctrls[i].value = count * 16 % 256;
            }
            memset(&ext_ctrls, 0, sizeof(ext_ctrls));
            ext_ctrls.ctrl_class = V4L2_CID_AF_WIN_X1;
            ext_ctrls.count = 4;
            ext_ctrls.controls = ctrls;
            ioctl(isp0_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);

            if (count == read_num / 4) {
                control.id = V4L2_CID_VFLIP;
                control.value = 1;
                if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
                    printf("VIDIOC_S_CTRL failed\n\n");
                    return -1;
                } else
                    printf("VIDIOC_S_CTRL ok\n");
            }

            if (count == read_num / 2) {
                control.id = V4L2_CID_HFLIP;
                control.value = 1;
                if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
                    printf("VIDIOC_S_CTRL failed\n");
                    return -1;
                } else
                    printf("VIDIOC_S_CTRL ok\n");
            }
#endif

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (errno == EINTR)
                    continue;
                printf("select err\n");
            }
            if (r == 0) {
                fprintf(stderr, "select timeout\n");
#ifdef TIMEOUT
                if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &vconfig.buf_type))
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
    usleep(20 * 1000);

    if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &vconfig.buf_type)) {
        printf("VIDIOC_STREAMOFF failed\n");
        return -1;
    } else
        printf("VIDIOC_STREAMOFF ok\n");

    if (-1 == free_frame_buffers())
        return -1;
#if SUBDEV_TEST
    close(isp0_fd);
    close(isp1_fd);
#endif
    return 0;
}

int config_video_buf(int buf_type, int v4l2_memtype, int alloc_memtype, struct video_config *config)
{
    if (buf_type < V4L2_BUF_TYPE_VIDEO_CAPTURE || buf_type > V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
        printf("buf_type set failed!\n");
        return -1;
    }

    if (v4l2_memtype < V4L2_MEMORY_MMAP || v4l2_memtype > V4L2_MEMORY_DMABUF) {
        printf("v4l2_memtype set failed!\n");
        return -1;
    }

    if (alloc_memtype < MEM_TYPE_DMA || alloc_memtype > MEM_TYPE_CDX_NEW) {
        printf("alloc_memtype set failed!\n");
        return -1;
    }

    config->buf_type = (enum v4l2_buf_type)buf_type;
    config->v4l2_memtype = (enum v4l2_memory)v4l2_memtype;
    config->alloc_memtype = alloc_memtype;

    return 0;
}

void parser_config(int argc, char **argv, struct video_config *config)
{
    config->test_cnt = 100;
    config->sel = 0;
    config->width = 1280;
    config->height = 720;
    config->mode = 4;
    config->fps = 30;

    CLEAR(config->dev_name);
    CLEAR(config->path_name);
    if (argc == 5) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, DEFAULT_FILE_PATH);
    } else if (argc == 6) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, "%s", argv[5]);
    } else if (argc == 7) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, "%s", argv[5]);
        config->mode = atoi(argv[6]);
    } else if (argc == 8) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, "%s", argv[5]);
        config->mode = atoi(argv[6]);
        config->test_cnt = atoi(argv[7]);
    } else if (argc == 9) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, "%s", argv[5]);
        config->mode = atoi(argv[6]);
        config->test_cnt = atoi(argv[7]);
        config->fps = atoi(argv[8]);
    } else if (argc == 10) {
        config->dev_id = atoi(argv[1]);
        sprintf(config->dev_name, "/dev/video%d", config->dev_id);
        config->sel = atoi(argv[2]);
        config->width = atoi(argv[3]);
        config->height = atoi(argv[4]);
        sprintf(config->path_name, "%s", argv[5]);
        config->mode = atoi(argv[6]);
        config->test_cnt = atoi(argv[7]);
        config->fps = atoi(argv[8]);
        config->wdr_mode = atoi(argv[9]);
    } else {
        printUsage();
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    printf("csi_test_usrptr version:%s\n", MODULE_VERSION);
    int i;
    int ret = 0;
    struct timeval tv1, tv2;
    float tv;

    install_sig_handler();

    parser_config(argc, argv, &vconfig);

    ret = config_video_buf(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, MEM_TYPE_CDX_NEW, &vconfig);
    if (ret < 0) {
        printf("config video buf failed!\n");
        return -1;
    }

    ret = allocOpen(vconfig.alloc_memtype, &gMem, NULL);
    if (ret < 0) {
        printf("allocOpen failed.\n");
        return -1;
    }

    input_size.width = vconfig.width;
    input_size.height = vconfig.height;

    if (vconfig.test_cnt < (int)read_num) {
        read_num = vconfig.test_cnt;
        save_flag = 0;
        vconfig.test_cnt = 1;
    } else if (vconfig.test_cnt < 1000) {
        read_num = vconfig.test_cnt;
        /*if output is raw then save one frame*/
        if (vconfig.mode < 5)
            save_flag = 1;
        else
            save_flag = 0;
        vconfig.test_cnt = 1;
    } else if (vconfig.test_cnt < 10000) {
        read_num = vconfig.test_cnt;
        save_flag = 3;
        vconfig.test_cnt = 10;
    } else {
        read_num = vconfig.test_cnt;
        save_flag = 2;
        vconfig.test_cnt = 1;
    }

#if display_frame
    gdisp = android::HwDisplay::getInstance();
#endif

    for (i = 0; i < vconfig.test_cnt; i++) {
        gettimeofday(&tv1, NULL);
        if (0 == main_test(vconfig.sel, vconfig.mode))
            printf("mode %d test done at the %d time!!\n", vconfig.mode, i);
        else
            printf("mode %d test failed at the %d time!!\n", vconfig.mode, i);
        close(fd);
        gettimeofday(&tv2, NULL);
        tv = (float)((tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec) / 1000000;
        printf("time cost %f(s)\n", tv);
    }

#if display_frame
    releaseDE();
#endif

    return 0;
}
