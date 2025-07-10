#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <sys/ioctl.h>
//#include <g2d_driver_enh.h>
#include "g2d_driver_enh.h"

#include "G2dApi.h"

#define TAG_LOG "G2DApi"
#include "sdklog.h"

namespace g2dapi
{

int g2dInit()
{
    ALOGD("libsdk_g2d version:%s", MODULE_VERSION);
    int g2dHandle = open("/dev/g2d", O_RDWR, 0);
    if (g2dHandle < 0) {
        ALOGE("open /dev/g2d failed");
        return -1;
    }

    return g2dHandle;
}

int g2dUnit(int g2dHandle)
{
    if (g2dHandle > 0) {
        close(g2dHandle);
    }
    return 0;
}

#if 0
#include "sunxiMemInterface.h"

paramStruct_t *memOps;

int g2dAllocBuff(stV4L2BUF* bufHandle, int width, int hight)
{
    memset(memOps, 0, sizeof(paramStruct_t));
    int ret = allocGetOpsParam(MEM_TYPE_DMA, memOps, NULL);

    if (memOps == NULL) {
        ALOGE("g2dAllocBuff memOps is NULL");
        return -1;
    }
    ret = allocOpen(MEM_TYPE_DMA, memOps, NULL);
    if (ret < 0) {
        ALOGE("g2dAllocBuff ion_alloc_open failed");
        return -1;
    }
    int size = width * hight * 3 / 2;

    memOps->size = size;
    ret = allocAlloc(MEM_TYPE_DMA, memOps, NULL);
    if (ret < 0) {
        ALOGE("g2d_func_open:allocAlloc failed");
    }
    //mG2dAddrVir = mCameraMemops.vir;
    //mG2dAddrPhy = mCameraMemops.phy;

    bufHandle->addrVirY = (unsigned long) memOps->vir;//palloc_cam(size, SHARE_BUFFER_FD,&bufHandle->nShareBufFd);
    bufHandle->addrPhyY = (unsigned long) memOps->phy;//cpu_get_phyaddr_cam((void*)bufHandle->addrVirY);
    memset((void*)bufHandle->addrVirY, 0x10, width * hight);
    memset((void*)(bufHandle->addrVirY + width * hight),
           0x80, width * hight / 2);
    ALOGD("g2dAllocBuff ion_alloc success");
    return 0;
}

int g2dFreeBuff(stV4L2BUF* bufHandle)
{
    //struct ScCamMemOpsS* memOps  = MemCamAdapterGetOpsS();

    if (memOps == NULL) {
        ALOGE("g2dAllocBuff memOps is NULL");
        return -1;
    }

    mCameraMemops.vir = mG2dAddrVir;
    allocFree(MEM_TYPE_DMA, memOps, NULL);
    mG2dAddrPhy = 0;

    memOps->pfree_cam((void*)bufHandle->addrVirY);
    bufHandle->addrPhyY = 0;
    memOps->close_cam();
    return 0;
}

#endif

int g2dScale(int g2dHandle, unsigned char *src, int src_width, int src_height, unsigned char *dst, int dst_width, int dst_height)
{
    g2d_stretchblt scale;

    if (g2dHandle <= 0) {
        return -1;
    }
    scale.flag = G2D_BLT_NONE;//G2D_BLT_NONE;//G2D_BLT_PIXEL_ALPHA|G2D_BLT_ROTATE90;
    scale.src_image.addr[0] = (unsigned long) src;
    scale.src_image.addr[1] = (unsigned long) src + src_width * src_height;
    scale.src_image.w = src_width;
    scale.src_image.h = src_height;
    scale.src_image.format = (g2d_data_fmt)G2D_FMT_PYUV420UVC;
    scale.src_image.pixel_seq = G2D_SEQ_NORMAL;
    scale.src_rect.x = 0;
    scale.src_rect.y = 0;
    scale.src_rect.w = src_width;
    scale.src_rect.h = src_height;

    scale.dst_image.addr[0] = (unsigned long)dst;
    scale.dst_image.addr[1] = (unsigned long)dst + dst_width * dst_height;
    //scale.dst_image.addr[2] = (int)dst + dst_width * dst_height * 5 / 4;
    scale.dst_image.w = dst_width;
    scale.dst_image.h = dst_height;
    scale.dst_image.format = (g2d_data_fmt)G2D_FMT_PYUV420UVC; // G2D_FMT_PYUV420UVC;
    scale.dst_image.pixel_seq = G2D_SEQ_NORMAL;
    scale.dst_rect.x = 0;
    scale.dst_rect.y = 0;
    scale.dst_rect.w = dst_width;
    scale.dst_rect.h = dst_height;
    scale.color = 0xff;
    scale.alpha = 0xff;

    if (ioctl(g2dHandle, G2D_CMD_STRETCHBLT, &scale) < 0) {
        ALOGE("g2dScale: failed to call G2D_CMD_STRETCHBLT!!!");
        return -1;
    }
    return 0;
}

int g2dClip(int g2dHandle, void* psrc, int src_w, int src_h, int src_x, int src_y, int width, int height, void* pdst, int dst_w, int dst_h, int dst_x, int dst_y)
{
    g2d_blt     blit_para;
    int         err;

    if (g2dHandle <= 0)
        return -1;
    blit_para.src_image.addr[0]      = (__u64)psrc;
    blit_para.src_image.addr[1]      = (__u64)psrc + src_w * src_h;
    blit_para.src_image.w            = src_w;
    blit_para.src_image.h            = src_h;
    blit_para.src_image.format       = (g2d_data_fmt)G2D_FMT_PYUV420UVC;
    blit_para.src_image.pixel_seq    = G2D_SEQ_NORMAL;
    blit_para.src_rect.x             = src_x;
    blit_para.src_rect.y             = src_y;
    blit_para.src_rect.w             = width;
    blit_para.src_rect.h             = height;

    blit_para.dst_image.addr[0]      = (__u64)pdst;
    blit_para.dst_image.addr[1]      = (__u64)pdst + dst_w * dst_h;
    blit_para.dst_image.w            = dst_w;
    blit_para.dst_image.h            = dst_h;
    blit_para.dst_image.format       = (g2d_data_fmt)G2D_FMT_PYUV420UVC;
    blit_para.dst_image.pixel_seq    = G2D_SEQ_NORMAL;
    blit_para.dst_x                  = dst_x;
    blit_para.dst_y                  = dst_y;
    blit_para.color                  = 0xff;
    blit_para.alpha                  = 0xff;

    blit_para.flag = G2D_BLT_NONE;

    err = ioctl(g2dHandle, G2D_CMD_BITBLT, (unsigned long)&blit_para);
    if (err < 0) {
        ALOGE("g2dClip: failed to call G2D_CMD_BITBLT!!!");
        return -1;
    }

    return 0;
}

int g2dClipByFd(int g2dHandle, int inFd, int flag, int infmt, int src_w, int src_h, int src_x, int src_y, int src_width, int src_height,
                int outFd, int outfmt, int dst_w, int dst_h, int dst_x, int dst_y, int dst_width, int dst_height)
{
    g2d_blt_h blit_para;
    int         err;

    //ALOGD("g2dClipByFd inputFd:%d,outputFd:%d",inputFd,outputFd);
    memset(&blit_para,0x0,sizeof(blit_para));
    blit_para.flag_h = (g2d_blt_flags_h)flag;
    blit_para.src_image_h.format = (g2d_fmt_enh)infmt;
    blit_para.src_image_h.fd = inFd;
    blit_para.src_image_h.width = src_width;
    blit_para.src_image_h.height = src_height;
    blit_para.src_image_h.clip_rect.x = src_x;
    blit_para.src_image_h.clip_rect.y = src_y;
    blit_para.src_image_h.clip_rect.w = src_w;
    blit_para.src_image_h.clip_rect.h = src_h;
#ifdef TARGET_KERNEL_4_9
    blit_para.src_image_h.mode = G2D_GLOBAL_ALPHA;
    blit_para.src_image_h.alpha = 255;
    blit_para.src_image_h.color = 0xee8899;
#endif
    blit_para.dst_image_h.fd = outFd;
    blit_para.dst_image_h.format = (g2d_fmt_enh)outfmt;
    blit_para.dst_image_h.width = dst_width;
    blit_para.dst_image_h.height = dst_height;
    blit_para.dst_image_h.clip_rect.x = dst_x;
    blit_para.dst_image_h.clip_rect.y = dst_y;
    blit_para.dst_image_h.clip_rect.w = dst_w;
    blit_para.dst_image_h.clip_rect.h = dst_h;
#ifdef TARGET_KERNEL_4_9
    blit_para.dst_image_h.mode = G2D_GLOBAL_ALPHA;
    blit_para.dst_image_h.alpha = 255;
    blit_para.dst_image_h.color = 0xee8899;
#endif
    err = ioctl(g2dHandle, G2D_CMD_BITBLT_H, (unsigned long)&blit_para);
    if (err < 0) {
        ALOGE("g2dClipByFd: failed to call G2D_CMD_BITBLT:%d!!! err:%d str:%s\n",inFd,err,strerror(errno));
        return -1;
    }

    return 0;
}

int g2d_clip(int devfd, int src_fd, int width, int height, int src_x, int src_y, int src_w, int src_h, int dst_fd, int dst_x, int dst_y, int dst_w, int dst_h,g2d_fmt_enh pix_fmt)
{
    g2d_blt_h blt;
    int ret;

    memset(&blt, 0, sizeof(g2d_blt_h));

    /* configure src image */
    blt.src_image_h.fd           = src_fd;
    blt.src_image_h.use_phy_addr = 0;
    blt.src_image_h.format       = pix_fmt;
    blt.src_image_h.laddr[0]     = 0;
    blt.src_image_h.laddr[1]     = 0;
    blt.src_image_h.laddr[2]     = 0;
    blt.src_image_h.haddr[0]     = 0;
    blt.src_image_h.haddr[1]     = 0;
    blt.src_image_h.haddr[2]     = 0;
    blt.src_image_h.width        = width;
    blt.src_image_h.height       = height;
    blt.src_image_h.align[0]     = 0;
    blt.src_image_h.align[1]     = 0;
    blt.src_image_h.align[2]     = 0;
    blt.src_image_h.clip_rect.x  = src_x;
    blt.src_image_h.clip_rect.y  = src_y;
    blt.src_image_h.clip_rect.w  = src_w;
    blt.src_image_h.clip_rect.h  = src_h;
    blt.src_image_h.bbuff        = 1;

    /* configure dst image */
    blt.dst_image_h.bbuff        = 1;
    blt.dst_image_h.fd           = dst_fd;
    blt.dst_image_h.format       = pix_fmt;
    blt.dst_image_h.laddr[0]     = 0;
    blt.dst_image_h.laddr[1]     = 0;
    blt.dst_image_h.laddr[2]     = 0;
    blt.dst_image_h.haddr[0]     = 0;
    blt.dst_image_h.haddr[1]     = 0;
    blt.dst_image_h.haddr[2]     = 0;
    blt.dst_image_h.width        = dst_w;
    blt.dst_image_h.height       = dst_h;

    blt.dst_image_h.align[0]     = 0;
    blt.dst_image_h.align[1]     = 0;
    blt.dst_image_h.align[2]     = 0;
    blt.dst_image_h.clip_rect.x  = dst_x;
    blt.dst_image_h.clip_rect.y  = dst_y;
    blt.dst_image_h.clip_rect.w  = src_w;
    blt.dst_image_h.clip_rect.h  = src_h;
    blt.dst_image_h.bbuff        = 1;

    blt.flag_h = G2D_ROT_0;
    blt.dst_image_h.use_phy_addr = 0;

    ret = ioctl(devfd, G2D_CMD_BITBLT_H, (void *)(&blt));
    if (ret != 0) {
		ALOGE("g2d_compose: failed to call g2dYuvspClip!!! %s \n",strerror(errno));
        return ret;
    }

    return ret;
}

int g2d_xCompose(int devfd,int src_w, int src_h, int src_fd1, int src_fd2, int src_fd3, int src_fd4, int dst_fd,g2d_fmt_enh pix_fmt)
{
    int ret = -1;

    int dst_w = src_w * 2;
    int dst_h = src_h * 2;

    ret  = g2d_clip(devfd, src_fd1, src_w, src_h, 0, 0, src_w, src_h, dst_fd, 0, 0, dst_w, dst_h,pix_fmt);
    ret |= g2d_clip(devfd, src_fd2, src_w, src_h, 0, 0, src_w, src_h, dst_fd, src_w, 0, dst_w, dst_h,pix_fmt);
    ret |= g2d_clip(devfd, src_fd3, src_w, src_h, 0, 0, src_w, src_h, dst_fd, 0, src_h, dst_w, dst_h,pix_fmt);
    ret |= g2d_clip(devfd, src_fd4, src_w, src_h, 0, 0, src_w, src_h, dst_fd, src_w, src_h, dst_w, dst_h,pix_fmt);

    if (ret != 0) {
		ALOGE("g2d_compose: failed to call g2dYuvspClip!!! %s \n",strerror(errno));
        return ret;
    }
    return 0;
}

int g2dRotate(int g2dHandle, g2dRotateAngle angle, unsigned char *src, int src_width, int src_height, unsigned char *dst, int dst_width, int dst_height)
{
    g2d_stretchblt str;

    if (g2dHandle <= 0)
        return -1;
    switch (angle) {
        case G2D_ROTATE90:
            str.flag = G2D_BLT_ROTATE90;
            break;
        case G2D_ROTATE180:
            str.flag = G2D_BLT_ROTATE180;
            break;
        case G2D_ROTATE270:
            str.flag = G2D_BLT_ROTATE270;
            break;
        case G2D_FLIP_HORIZONTAL:
            str.flag = G2D_BLT_FLIP_HORIZONTAL;
            break;
        case G2D_FLIP_VERTICAL:
            str.flag = G2D_BLT_FLIP_VERTICAL;
            break;
        case G2D_MIRROR45:
            str.flag = G2D_BLT_MIRROR45;
            break;
        case G2D_MIRROR135:
            str.flag = G2D_BLT_MIRROR135;
            break;
    }
    //str.flag = G2D_BLT_NONE;//G2D_BLT_NONE;//G2D_BLT_PIXEL_ALPHA|G2D_BLT_ROTATE90;
    str.src_image.addr[0] = (unsigned long)src;
    str.src_image.addr[1] = (unsigned long)src + src_width * src_height;
    str.src_image.w = src_width;
    str.src_image.h = src_height;
    str.src_image.format = (g2d_data_fmt)G2D_FMT_PYUV420UVC;
    str.src_image.pixel_seq = G2D_SEQ_NORMAL;
    str.src_rect.x = 0;
    str.src_rect.y = 0;
    str.src_rect.w = src_width;
    str.src_rect.h = src_height;

    str.dst_image.addr[0] = (unsigned long)dst;
    str.dst_image.addr[1] = (unsigned long)dst + dst_width * dst_height;
    //scale.dst_image.addr[2] = (int)dst + dst_width * dst_height * 5 / 4;
    str.dst_image.w = dst_width;
    str.dst_image.h = dst_height;
    str.dst_image.format = (g2d_data_fmt)G2D_FMT_PYUV420UVC;// G2D_FMT_PYUV420UVC;
    str.dst_image.pixel_seq = G2D_SEQ_NORMAL;
    str.dst_rect.x = 0;
    str.dst_rect.y = 0;
    str.dst_rect.w = dst_width;
    str.dst_rect.h = dst_height;
    str.color = 0xff;
    str.alpha = 0xff;

    if (ioctl(g2dHandle, G2D_CMD_STRETCHBLT, &str) < 0) {
        ALOGE("g2dRotate: failed to call G2D_CMD_STRETCHBLT!!!");
        return -1;
    }
    return 0;
}

int g2dFormatConv(int g2dHandle, void* psrc, int src_format, int src_w, int src_h,
                  int src_crop_x, int src_crop_y, int src_crop_w, int src_crop_h,
                  void* pdst, int dst_format, int dst_w, int dst_h,
                  int dst_crop_x, int dst_crop_y, int dst_crop_w, int dst_crop_h)
{
    int err;
    g2d_blt_h blit_para;
    if (g2dHandle <= 0)
        return -1;

    //ALOGD("g2dClipByFd inFd:%d,outFd:%d",inFd,outFd);
    memset(&blit_para, 0x0, sizeof(blit_para));

    if (src_format <= G2D_FORMAT_BGRA1010102) {
        blit_para.src_image_h.laddr[0] = (unsigned long)psrc;
    } else {
        blit_para.src_image_h.laddr[0] = (unsigned long)psrc;
        blit_para.src_image_h.laddr[1] = (unsigned long)psrc + src_w * src_h;
    }

    if (dst_format <= G2D_FORMAT_BGRA1010102) {
        blit_para.dst_image_h.laddr[0] = (unsigned long)pdst;
    } else {
        blit_para.dst_image_h.laddr[0] = (unsigned long)pdst;
        blit_para.dst_image_h.laddr[1] = (unsigned long)pdst + dst_w * dst_h;
    }

    blit_para.flag_h = G2D_BLT_NONE_H;
    blit_para.src_image_h.format = (g2d_fmt_enh)src_format;//G2D_FORMAT_YUV420UVC_V1U1V0U0;//G2D_FORMAT_YUV420UVC_V1U1V0U0;
    blit_para.src_image_h.use_phy_addr = 1;
    blit_para.src_image_h.width = src_w;
    blit_para.src_image_h.height = src_h;
    blit_para.src_image_h.clip_rect.x = src_crop_x;
    blit_para.src_image_h.clip_rect.y = src_crop_y;
    blit_para.src_image_h.clip_rect.w = src_crop_w;
    blit_para.src_image_h.clip_rect.h = src_crop_h;
    blit_para.src_image_h.mode = (g2d_alpha_mode_enh)G2D_PIXEL_ALPHA;
    blit_para.src_image_h.alpha = 255;
    blit_para.src_image_h.color = 0xee8899;

    blit_para.dst_image_h.use_phy_addr = 1;
    blit_para.dst_image_h.format = (g2d_fmt_enh)dst_format;//G2D_FORMAT_YUV420UVC_V1U1V0U0;//G2D_FORMAT_YUV420UVC_V1U1V0U0;
    blit_para.dst_image_h.width = dst_w;
    blit_para.dst_image_h.height = dst_h;
    blit_para.dst_image_h.clip_rect.x = dst_crop_x;
    blit_para.dst_image_h.clip_rect.y = dst_crop_y;
    blit_para.dst_image_h.clip_rect.w = dst_crop_w;
    blit_para.dst_image_h.clip_rect.h = dst_crop_h;
    blit_para.dst_image_h.mode = (g2d_alpha_mode_enh)G2D_PIXEL_ALPHA;
    blit_para.dst_image_h.alpha = 255;
    blit_para.dst_image_h.color = 0xee8899;
    err = ioctl(g2dHandle, G2D_CMD_BITBLT_H, (unsigned long)&blit_para);
    if (err < 0) {
        printf("g2dFormatConv: failed to call G2D_CMD_BITBLT_H!!!");
        return -1;
    }
    return 0;
}

#if 0
struct G2dOpsS _G2dOpsS = {
    .fpG2dInit = g2dInit,
    .fpG2dUnit = g2dUnit,
    .fpG2dScale = g2dScale,
    .fpG2dClip = g2dClip,
    .fpG2dRotate = g2dRotate
};

struct G2dOpsS* GetG2dOpsS()
{
    ALOGD("---------------------------_GetG2dOpsS ---------------------------");
    return &_G2dOpsS;
}
#endif
}
//};
