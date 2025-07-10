#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include "sunxiMemInterface.h"
#include "sunxi_disp_hal.h"

#define PIC_W 1280
#define PIC_H 720
#define BMP_PIC_SIZE (PIC_W * PIC_H * 4)

#define INPUT_RAW_PATH "/tmp/yuv_test/bike_1280x720_565"
#define DISP_FMT DISP_FORMAT_RGB_565

// #define INPUT_RAW_PATH "/tmp/yuv_test/bike_1280x720_888"
// #define DISP_FMT DISP_FORMAT_RGB_888

// #define INPUT_RAW_PATH "/tmp/yuv_test/bike_1280x720_yv12.yuv"
// #define DISP_FMT DISP_FORMAT_YUV420_P

int allocPicMem(paramStruct_t* pops, int size)
{
    int ret = 0;

    ret = allocOpen(MEM_TYPE_CDX_NEW, pops, NULL);
    if (ret < 0) {
        printf("ion_alloc_open failed\n");
        return ret;
    }
    pops->size = size;

    ret = allocAlloc(MEM_TYPE_CDX_NEW, pops, NULL);
    if (ret < 0) {
        printf("allocAlloc failed\n");
        return ret;
    }
    printf("pops.vir=%p pops.phy=%p dmafd=%d,alloc len=%d\n", (void*)pops->vir, (void*)pops->phy,
                pops->ion_buffer.fd_data.aw_fd, pops->size);

    return 0;
}

int freePicMem(paramStruct_t* pops)
{
    allocFree(MEM_TYPE_CDX_NEW, pops, NULL);
    allocClose(MEM_TYPE_CDX_NEW, pops, NULL);

    return 0;
}

static int fillYuvData(void* ptr, int size)
{
    char yuvfile[64];
    sprintf(yuvfile, INPUT_RAW_PATH);

    FILE* inputfd = fopen(yuvfile, "rb");
    if (NULL == inputfd) {
        printf("fopen %s failed\n", yuvfile);
    } else {
        printf("fopen %s OK\n", yuvfile);
    }
    /* Skip the header if the image is a standard BMP file */
    // fseek(inputfd, 54, SEEK_SET);
    fread(ptr, size, 1, inputfd);
    fclose(inputfd);
    return 0;
}

int main(int argc, char** argv)
{
    printf("yuvtest version:%s\n", MODULE_VERSION);

    SunxiDisplay* m_sunxi_disp = SunxiDisplay::get_instance();

    paramStruct_t m_mem_ops;

    //申请地址
    allocPicMem(&m_mem_ops, BMP_PIC_SIZE);

    memset((void*)m_mem_ops.vir, 0, m_mem_ops.size);

    fillYuvData((void*)m_mem_ops.vir, m_mem_ops.size);

    printf("fill data success!\n");

    int layer_hdl = m_sunxi_disp->request_layer(0, 1, 0);

    struct fb_src_info fb_src = { PIC_W, PIC_H, DISP_FMT };
    m_sunxi_disp->set_layer_fb_addr(layer_hdl, &fb_src, m_mem_ops.ion_buffer.fd_data.aw_fd);

    struct disp_rect rect = { 0, 0, 1024, 600 };
    m_sunxi_disp->set_layer_frame(layer_hdl, &rect);

    m_sunxi_disp->set_layer_zorder(layer_hdl, 30);

    m_sunxi_disp->print_sdk_layer_cfg(layer_hdl);
    m_sunxi_disp->enable_layer(layer_hdl);

    getchar();

    m_sunxi_disp->enable_layer(layer_hdl);

    getchar();

#if 1

    m_sunxi_disp->switch_output_type(1,DISP_OUTPUT_TYPE_TV,DISP_TV_MOD_NTSC);

    int layer1_hdl = m_sunxi_disp->request_layer(1, 0, 0);

    m_sunxi_disp->set_layer_fb_addr(layer1_hdl, &fb_src, m_mem_ops.ion_buffer.fd_data.aw_fd);

    struct disp_rect screen1_rect = { 0, 0, 720, 480 };
    m_sunxi_disp->set_layer_frame(layer1_hdl, &screen1_rect);

    m_sunxi_disp->enable_layer(layer1_hdl);
    getchar();

    m_sunxi_disp->disable_layer(layer1_hdl);
    m_sunxi_disp->release_layer(layer1_hdl);
#endif

    m_sunxi_disp->disable_layer(layer_hdl);
    m_sunxi_disp->release_layer(layer_hdl);
    freePicMem(&m_mem_ops);

    return 0;
}
