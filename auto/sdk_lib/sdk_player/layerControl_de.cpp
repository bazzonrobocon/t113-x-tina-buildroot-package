/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : layerControl_de.cpp
* Description : display DE -- for H3
* History :
*/

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include "cdx_config.h"
#include <cdx_log.h>
#include "layerControl.h"
#include "sunxiMemInterface.h"
#include "cdx_log.h"
#include "sunxi_display2.h"
#include "hwdisp2.h"

#include <sys/sysinfo.h>

#include "iniparserapi.h"
//#include <sdklog.h>
using namespace android;
#include "aut_tr.h"
#include <errno.h>
#define SAVE_PIC (0)

#define GPU_BUFFER_NUM 32
#define USE_LIB_DISP
//static VideoPicture* gLastPicture = NULL;

#define BUF_MANAGE (0)

#define NUM_OF_PICTURES_KEEP_IN_NODE (GetConfigParamterInt("pic_4list_num", 3)+1)

int LayerCtrlHideVideo(LayerCtrl* l);

typedef struct VPictureNode_t VPictureNode;
struct VPictureNode_t {
    VideoPicture* pPicture;
    VPictureNode* pNext;
    int           bUsed;
    int           bValid;
};

typedef struct BufferInfoS {
    VideoPicture pPicture;
    int          nUsedFlag;
} BufferInfoT;

typedef struct LayerContext {
    LayerCtrl            base;
    enum EPIXELFORMAT    eDisplayPixelFormat;
    int                  nWidth;
    int                  nHeight;
    int                  nLeftOff;
    int                  nTopOff;
    int                  nDisplayWidth;
    int                  nDisplayHeight;

    int                  bHoldLastPictureFlag;
    int                  bVideoWithTwoStreamFlag;
    int                  bIsSoftDecoderFlag;

    int                  bLayerInitialized;
    int                  bProtectFlag;

    void*                pUserData;

    //struct ScMemOpsS*    pMemops;

    //* use when render derect to hardware layer.
    VPictureNode*        pPictureListHeader;
    VPictureNode         picNodes[16];

    int                  nGpuBufferCount;
    BufferInfoT          mBufferInfo[GPU_BUFFER_NUM];
    int                  bLayerShowed;

    int                  fdDisplay;
    int                  nScreenWidth;
    int                  nScreenHeight;

    //add by fengkun
    int mCurDispIdx;
    uint8_t* mPreviewAddrVir[4];
    unsigned int mPreviewAddrPhy[4];
    tr_obj  imgtr;
    int dmode;
    //for
    //crop src
    int crop_x, crop_y, crop_w, crop_h;
    int user_ctrl_corp;
    //viewer view
    int view_x, view_y, view_w, view_h;

    //for when pause user canget the cur disp buffer
    uint8_t* mCurDispAddrVirY;

} LayerContext;
LayerContext* g_lc = NULL;
paramStruct_t gLayerDeMemory;
int g_screen0_user_init = 0;
int g_screen0_type = 1;
int g_screen0_mode = 4;
int g_screen0_w = 0;
int g_screen0_h = 0;

int g_screen1_user_init = 0;
int g_screen1_type = 2;
int g_screen1_mode = 14;
int g_screen1_w = 0;
int g_screen1_h = 0;

//* set usage, scaling_mode, pixelFormat, buffers_geometry, buffers_count, crop
static int setLayerBuffer(LayerContext* lc)
{
    if (lc == NULL) {
        loge("setLayerBuffer error lc=NULL\n");
        return -1;
    }
    logd("setLayerBuffer src: PixelFormat(%d), nW(%d), nH(%d), leftoff(%d), topoff(%d)",
         lc->eDisplayPixelFormat, lc->nWidth,
         lc->nHeight, lc->nLeftOff, lc->nTopOff);
    logd("setLayerBuffer disp: dispW(%d), dispH(%d), buffercount(%d), bProtectFlag(%d),\
          bIsSoftDecoderFlag(%d)",
         lc->nDisplayWidth, lc->nDisplayHeight, lc->nGpuBufferCount,
         lc->bProtectFlag, lc->bIsSoftDecoderFlag);

    //int          pixelFormat;
    unsigned int nGpuBufWidth;
    unsigned int nGpuBufHeight;
    int i = 0;
    char* pVirBuf;
    char* pPhyBuf;
    paramStruct_t dmabuf;
    memcpy(&dmabuf, &gLayerDeMemory, sizeof(paramStruct_t));
    nGpuBufWidth  = lc->nWidth;  //* restore nGpuBufWidth to mWidth;
    nGpuBufHeight = lc->nHeight;

    //* We should double the height if the video with two stream,
    //* so the nativeWindow will malloc double buffer
    if (lc->bVideoWithTwoStreamFlag == 1) {
        nGpuBufHeight = 2 * nGpuBufHeight;
    }

    if (lc->nGpuBufferCount <= 0) {
        loge("error: the lc->nGpuBufferCount[%d] is invalid!", lc->nGpuBufferCount);
        return -1;
    }

    lc->mCurDispAddrVirY = 0;

    for (i = 0; i < lc->nGpuBufferCount; i++) {
        dmabuf.size = nGpuBufWidth * nGpuBufHeight * 3 / 2;
        int ret = allocAlloc(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
        if (ret < 0) {
            loge("setLayerBuffer:allocAlloc failed");
        }
        pVirBuf = (char*)dmabuf.vir;
        pPhyBuf = (char*)dmabuf.phy;

        lc->mBufferInfo[i].nUsedFlag    = 0;

        lc->mBufferInfo[i].pPicture.nWidth  = lc->nWidth;
        lc->mBufferInfo[i].pPicture.nHeight = lc->nHeight;
        lc->mBufferInfo[i].pPicture.nLineStride  = lc->nWidth;
        lc->mBufferInfo[i].pPicture.pData0       = pVirBuf;
        lc->mBufferInfo[i].pPicture.pData1       = pVirBuf + (lc->nHeight * lc->nWidth);
        lc->mBufferInfo[i].pPicture.pData2       =
            lc->mBufferInfo[i].pPicture.pData1 + (lc->nHeight * lc->nWidth) / 4;
        lc->mBufferInfo[i].pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
        lc->mBufferInfo[i].pPicture.nBufFd = dmabuf.ion_buffer.fd_data.aw_fd;
        lc->mBufferInfo[i].pPicture.phyCBufAddr  =
            lc->mBufferInfo[i].pPicture.phyYBufAddr + (lc->nHeight * lc->nWidth);
        lc->mBufferInfo[i].pPicture.nBufId       = i;
        lc->mBufferInfo[i].pPicture.ePixelFormat = lc->eDisplayPixelFormat;

        //logd("\n=== init id:%d pVirBuf: %p\n", i, (uintptr_t)pPhyBuf);
        //logd("=== init id:%d phyYBufAddr: %p\n", i, lc->mBufferInfo[i].pPicture.phyYBufAddr);
    }

#ifdef BOGUS
    lc->dmode = aut_get_disp_rotate_mode();
    if (lc->dmode > 0) {
        aut_imgtr_init(&lc->imgtr);
        lc->mCurDispIdx = 0;
        for (i = 0; i < 4; i++) {
            if (lc->mPreviewAddrVir[i] == 0) {
                dmabuf.size = ALIGN_32B(nGpuBufWidth) * ALIGN_32B(nGpuBufHeight) * 3 / 2;
                int ret = allocAlloc(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                if (ret < 0) {
                    loge("setLayerBuffer:allocAlloc failed");
                }
                lc->mPreviewAddrVir[i] = dmabuf.vir;
                lc->mPreviewAddrPhy[i] = dmabuf.phy;
            }
        }
    }
#endif /* BOGUS */

    return 0;
}
static struct disp_layer_config2 config_scn0;
#ifdef SAVE_FRAME_DATA
static void savePicture(VideoPicture* pPicture)
{

    char path[60];
    struct timeval time;
    gettimeofday(&time, NULL);
    sprintf(path, "/tmp/%ld[%dx%d]nv21.yuv", time.tv_usec,
            pPicture->nWidth, pPicture->nHeight);

    int nSizeY, nSizeUV;
#ifdef STEAM_FILE
    static FILE* fp = NULL;
    if (fp == NULL) {
        fp = fopen(path, "ab");
    }
#else
    FILE* fp = fopen(path, "ab");
#endif
    nSizeY = pPicture->nWidth * pPicture->nHeight ;
    nSizeUV = nSizeY >> 1;
    fwrite(pPicture->pData0, 1, nSizeY, fp);
    fwrite(pPicture->pData1, 1, nSizeUV, fp);

    fclose(fp);

}
#endif
static int SetLayerParam(LayerContext* lc, VideoPicture* pPicture)
{
    struct disp_layer_config2 config;
    //int ret = 0;
    //* close the layer first, otherwise, in case when last frame is kept showing,
    //* the picture showed will not valid because parameters changed.
    //logd("SetLayerParam,lc=%p,pPicture=%p",lc,pPicture);
    if ((lc == NULL) || (pPicture == NULL)) {
        loge("SetLayerParam input param is NULL\n");
        return -1;
    }
    /*
    printf("size:%d,[%d %d %d %d][%dX%d][%d,%dX%d]\n",pPicture->nBufSize,
                    pPicture->nTopOffset,
                    pPicture->nLeftOffset,
                    pPicture->nBottomOffset,
                    pPicture->nRightOffset,
                    pPicture->nWidth,
                    pPicture->nHeight,
                    pPicture->nCurFrameInfo.nVidFrmSize,
                    pPicture->nCurFrameInfo.nVidFrmDisW,
                    pPicture->nCurFrameInfo.nVidFrmDisH);
                    */
    paramStruct_t dmabuf;
    memcpy(&dmabuf, &gLayerDeMemory, sizeof(paramStruct_t));
    memset(&config.info, 0, sizeof(struct disp_layer_info));
    if (lc->bLayerShowed == 1) {
        lc->bLayerShowed = 0;
        //TO DO.
    }

    frm_info_t img;
    //int disp_format;
    int ww;
    int hh;
    unsigned long addry;
    unsigned long addrc;
    if (lc->dmode > 0) {
        //rotate 90 180 270 etc.
        dmabuf.vir = (uint8_t *)pPicture->pData0;
        allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
        addry = dmabuf.phy;
        dmabuf.vir = (uint8_t*)pPicture->pData1;
        allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);

        addrc = dmabuf.phy;
        img.addr0 = &addry;
        img.addr1 = &addrc;
        img.format = TR_FORMAT_YUV420_SP_VUVU;
        img.width = lc->nDisplayWidth;
        img.height = lc->nDisplayHeight;
        img.mode = (tr_mode)lc->dmode;
        int curidx = lc->mCurDispIdx;
        lc->mCurDispIdx++;
        if (lc->mCurDispIdx >= 4)
            lc->mCurDispIdx = 0;
        img.outaddr0 = &lc->mPreviewAddrPhy[curidx];
        ww = ALIGN_32B(lc->nDisplayHeight);
        hh = lc->nDisplayWidth;
        if ((img.mode == TR_ROT_180) || (img.mode == TR_HFLIP) || (img.mode == TR_VFLIP)) {
            ww = ALIGN_32B(lc->nDisplayWidth);
            hh = (lc->nDisplayHeight);
        } else {
            ww = ALIGN_32B(lc->nDisplayHeight);
            hh = lc->nDisplayWidth;
        }
        //aut_imgtr_proc(&lc->imgtr,&img);
        //CdxIonFlushCache((void*)lc->mPreviewAddrVir[curidx], ww*hh*3/2);
        dmabuf.vir = (uint8_t*)lc->mPreviewAddrVir[curidx];
        dmabuf.size = ALIGN_32B(ww) * ALIGN_32B(hh) * 3 / 2;
        flushCache(MEM_TYPE_CDX_NEW, &dmabuf, NULL);

        lc->mCurDispAddrVirY = lc->mPreviewAddrVir[curidx];

        addry = lc->mPreviewAddrPhy[curidx];
        addrc = lc->mPreviewAddrPhy[curidx] + ww * hh;

        config.info.fb.format = DISP_FORMAT_YUV420_P;
#ifdef BOGUS
        config.info.fb.size[0]  = (unsigned long)addry;

        config.info.fb.size[1]  = (unsigned long)addrc;

        config.info.fb.size[2]  = (unsigned long)addry + (ww * hh) + (ww * hh / 4);
#endif /* BOGUS */
        config.info.fb.fd = dmabuf.ion_buffer.fd_data.aw_fd;
        config.info.fb.size[0].width     = ww;
        config.info.fb.size[0].height    = hh;
        config.info.fb.size[1].width     = ww / 2;
        config.info.fb.size[1].height    = hh / 2;
        config.info.fb.size[2].width     = ww / 2;
        config.info.fb.size[2].height    = hh / 2;
    } else {
        //* transform pixel format.
        switch (lc->eDisplayPixelFormat) {
            case PIXEL_FORMAT_YUV_PLANER_420:
                config.info.fb.format = DISP_FORMAT_YUV420_P;
                lc->mCurDispAddrVirY = (uint8_t *)pPicture->pData0;
                /*config.info.fb.size[0] = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData0);
                config.info.fb.size[1] = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData1);
                config.info.fb.size[2] = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData2);*/

#ifdef BOGUS
                dmabuf.vir = (unsigned long)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[0] = dmabuf.phy;

                dmabuf.vir = (unsigned long)pPicture->pData1;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[1] = dmabuf.phy;

                dmabuf.vir = (unsigned long)pPicture->pData2;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[2] = dmabuf.phy;
#endif /* BOGUS */
                dmabuf.vir = (uint8_t *)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.fd = dmabuf.ion_buffer.fd_data.aw_fd;
                config.info.fb.size[0].width     = (lc->nWidth);
                config.info.fb.size[0].height    = lc->nHeight;
                config.info.fb.size[1].width     = (lc->nWidth / 2);
                config.info.fb.size[1].height    = lc->nHeight / 2;
                config.info.fb.size[2].width     = (lc->nWidth / 2);
                config.info.fb.size[2].height    = lc->nHeight / 2;
                break;

            case PIXEL_FORMAT_YV12:
                config.info.fb.format = DISP_FORMAT_YUV420_P;
                lc->mCurDispAddrVirY = (uint8_t *)pPicture->pData0;
                /*config.info.fb.size[0]  = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData0);
                config.info.fb.size[1]  = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData1);
                config.info.fb.size[2]  = (unsigned long )
                                           CdxIonVir2Phy(pPicture->pData2);*/
#ifdef BOGUS
                dmabuf.vir = (unsigned long)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[0] = dmabuf.phy;

                dmabuf.vir = (unsigned long)pPicture->pData1;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[1] = dmabuf.phy;

                dmabuf.vir = (unsigned long)pPicture->pData2;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[2] = dmabuf.phy;
#endif /* BOGUS */
                config.info.fb.size[0].width     = (lc->nWidth);
                config.info.fb.size[0].height    = lc->nHeight;
                config.info.fb.size[1].width     = (lc->nWidth) / 2;
                config.info.fb.size[1].height    = lc->nHeight / 2;
                config.info.fb.size[2].width     = (lc->nWidth) / 2;
                config.info.fb.size[2].height    = lc->nHeight / 2;

                dmabuf.vir = (uint8_t *)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.fd = dmabuf.ion_buffer.fd_data.aw_fd;
                break;

            case PIXEL_FORMAT_NV12:
                config.info.fb.format = DISP_FORMAT_YUV420_SP_UVUV;
                lc->mCurDispAddrVirY = (uint8_t *)pPicture->pData0;
                /* config.info.fb.size[0] = (unsigned long )
                                             CdxIonVir2Phy(pPicture->pData0);
                 config.info.fb.size[1] = (unsigned long )
                                             CdxIonVir2Phy(pPicture->pData1);*/

                dmabuf.vir = (uint8_t*)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);

                //dmabuf.vir = (unsigned long)pPicture->pData1;
                //allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.fd = dmabuf.ion_buffer.fd_data.aw_fd;
                config.info.fb.size[0].width     = (lc->nWidth);
                config.info.fb.size[0].height    = lc->nDisplayHeight;

                config.info.fb.size[1].width     = (lc->nWidth) / 2;
                config.info.fb.size[1].height    = lc->nHeight / 2;


                break;

            case PIXEL_FORMAT_NV21:
                config.info.fb.format = DISP_FORMAT_YUV420_SP_VUVU;
                lc->mCurDispAddrVirY = (uint8_t*)pPicture->pData0;
                /*config.info.fb.size[0] = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData0);
                config.info.fb.size[1] = (unsigned long )
                                            CdxIonVir2Phy(pPicture->pData1);
                                            */
#ifdef BOGUS
                dmabuf.vir = (uint8_t*)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[0] = dmabuf.phy;
                dmabuf.vir = (uint8_t *)pPicture->pData1;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.size[1] = dmabuf.phy;
#endif /* BOGUS */
                dmabuf.vir = (uint8_t*)pPicture->pData0;
                allocVir2phy(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
                config.info.fb.fd = dmabuf.ion_buffer.fd_data.aw_fd;
                config.info.fb.size[0].width     = (lc->nWidth);
                config.info.fb.size[0].height    = lc->nHeight;

                config.info.fb.size[1].width     = (lc->nWidth) / 2;
                config.info.fb.size[1].height    = lc->nHeight / 2;
                break;

            default: {
                loge("unsupported pixel format.");
                lc->mCurDispAddrVirY = 0;
                return -1;
            }
        }
    }
    //* initialize the layerInfo.
    config.info.mode            = LAYER_MODE_BUFFER;
    config.info.zorder          = 1;
    config.info.alpha_mode      = 1;
    config.info.alpha_mode      = 0xff;
    //* image size.
    if ((lc->user_ctrl_corp == 0) || (lc->crop_w == 0) || (lc->crop_h == 0)) { //fix iphone's record mp4 lead to 2 screen
        lc->crop_x = lc->nLeftOff;
        lc->crop_y = lc->nTopOff;
        lc->crop_w = lc->nDisplayWidth;
        lc->crop_h = lc->nDisplayHeight;
    }

    if (lc->dmode == TR_ROT_90) {
        config.info.fb.crop.x = (unsigned long long)lc->crop_x << 32;
        config.info.fb.crop.y = (unsigned long long)lc->crop_y << 32;
        config.info.fb.crop.width   = (unsigned long long)lc->crop_h << 32;
        config.info.fb.crop.height  = (unsigned long long)lc->crop_w << 32;
        //config.info.fb.crop.x = (unsigned long long)lc->crop_x << 32;
        //config.info.fb.crop.y = (unsigned long long)lc->crop_y << 32;
        //config.info.fb.crop.width   = (unsigned long long)lc->crop_w << 32;
        //config.info.fb.crop.height  = (unsigned long long)lc->crop_h << 32;

    } else   if (lc->dmode == TR_ROT_270) {
        config.info.fb.crop.x = (unsigned long long)lc->crop_x << 32;
        config.info.fb.crop.y = (unsigned long long)lc->crop_y << 32;
        config.info.fb.crop.width   = (unsigned long long)lc->crop_h << 32;
        config.info.fb.crop.height  = (unsigned long long)lc->crop_w << 32;


    } else   if (lc->dmode == TR_ROT_180) {
        config.info.fb.crop.x = (unsigned long long)lc->crop_x << 32;
        config.info.fb.crop.y = (unsigned long long)lc->crop_y << 32;
        config.info.fb.crop.width   = (unsigned long long)lc->crop_w << 32;
        config.info.fb.crop.height  = (unsigned long long)lc->crop_h << 32;

    } else {
        config.info.fb.crop.x = (unsigned long long)lc->crop_x << 32;
        config.info.fb.crop.y = (unsigned long long)lc->crop_y << 32;
        config.info.fb.crop.width   = (unsigned long long)lc->crop_w << 32;
        config.info.fb.crop.height  = (unsigned long long)lc->crop_h << 32;
    }
    config.info.fb.color_space  = DISP_BT601;//(lc->nHeight < 720) ? DISP_BT601 : DISP_BT709;

    //* source window.
    if ((lc->view_w == 0) || (lc->view_h == 0)) {
        config.info.screen_win.x        = lc->nLeftOff;
        config.info.screen_win.y        = lc->nTopOff;
        config.info.screen_win.width    = lc->nScreenWidth;
        config.info.screen_win.height   = lc->nScreenHeight;

    } else {
        config.info.screen_win.x        = lc->view_x;//lc->nLeftOff;
        config.info.screen_win.y        = lc->view_y;//lc->nTopOff;
        config.info.screen_win.width    = lc->view_w;//lc->nScreenWidth;
        config.info.screen_win.height   = lc->view_h;//lc->nScreenHeight;
    }

#ifdef SAVE_FRAME_DATA
    savePicture(pPicture);
#endif /* SAVE_FRAME_DATA */
    config.info.alpha_mode          = 1;
    config.info.alpha_value         = 0xff;

    config.channel = 0;
    config.enable = 1;
    config.layer_id = 0;
#ifdef USE_LIB_DISP
    HwDisplay* mcd = NULL;
    mcd = HwDisplay::getInstance();

    if ((g_screen0_user_init + g_screen1_user_init) == 0) {
        mcd->layerObj[0] = mcd->aut_hwd_layer_request(&mcd->Surface[0],
                                                      mcd->ScreenId[0], mcd->ChanelId[0], mcd->LayerId[0]);
        mcd->CameraDispEnableFlag[0] = false;  //player's priority is higher then camera,so stop the camera disp
        mcd->CameraDispEnableFlag[1] = false;
        mcd->aut_hwd_set_layer_info(mcd->layerObj[0], &config.info);
        mcd->aut_hwd_layer_open(mcd->layerObj[0]);
        memcpy(&config_scn0, &config, sizeof(struct disp_layer_config2));
    }

    if (g_screen0_user_init) {
        mcd->ScreenId[0] = 0;
        mcd->ChanelId[0] = 0;
        mcd->LayerId[0] = 0;
        mcd->layerObj[0] = mcd->aut_hwd_layer_request(&mcd->Surface[0],
                                                      mcd->ScreenId[0], mcd->ChanelId[0], mcd->LayerId[0]);
        config.info.screen_win.width = g_screen0_w;
        config.info.screen_win.height = g_screen0_h;

        mcd->aut_hwd_set_layer_info(mcd->layerObj[0], &config.info);
        struct view_info frame = {0, 0, g_screen0_w, g_screen0_h};
        mcd->aut_hwd_layer_sufaceview(mcd->layerObj[0], &frame);
        mcd->aut_hwd_layer_open(mcd->layerObj[0]);
    }

    //---------------video tvout enable----------------------
    if (g_screen1_user_init) {
        mcd->ScreenId[1] = 1;
        mcd->ChanelId[1] = 0;
        mcd->LayerId[1] = 0;
        mcd->layerObj[1] = mcd->aut_hwd_layer_request(&mcd->Surface[1],
                                                      mcd->ScreenId[1], mcd->ChanelId[1], mcd->LayerId[1]);
        config.info.screen_win.width = g_screen1_w;
        config.info.screen_win.height = g_screen1_h;

        mcd->aut_hwd_set_layer_info(mcd->layerObj[1], &config.info);
        struct view_info frame = {0, 0, g_screen1_w, g_screen1_h};
        mcd->aut_hwd_layer_sufaceview(mcd->layerObj[1], &frame);

        //mcd->aut_hwd_layer_set_rect(mcd->layerObj[1],&config.info.fb.crop);

        mcd->aut_hwd_layer_open(mcd->layerObj[1]);


    }
#else
    unsigned int     args[4];
    args[0] = 0;
    args[1] = (unsigned int)(&config);
    args[2] = 1;
    memcpy(&config_scn0, &config, sizeof(struct disp_layer_config2));
    ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG2, (void*)args);
    if (0 != ret)
        loge("fail to set layer info!");
#endif

    return 0;
}


static int __LayerReset(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    logw("__LayerReset.");

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerReset error lc=NULL\n");
        return -1;
    }
    //add by fengkun

#ifdef USE_LIB_DISP
    HwDisplay* mcd = NULL;
    mcd = HwDisplay::getInstance();
    mcd->aut_hwd_layer_close(mcd->layerObj[0]);
    if (g_screen1_type) {
        mcd->aut_hwd_layer_close(mcd->layerObj[1]);
    }
    usleep(100000);//this is abug for disp layer bg green

#else
    unsigned int     args[4];
    args[0] = 0;
    args[1] = (unsigned int)(&config_scn0);
    args[2] = 1;
    if (lc->fdDisplay >= 0) {
        //int t_Ysize = config_scn0.info.fb.size[0].width*config_scn0.info.fb.size[0].height;
        //memset((void*)gLayerDeMemory.vir, 0x00, t_Ysize);   //make it black
        //memset((void*)gLayerDeMemory.vir +t_Ysize, 0x80, t_Ysize>>1);
        //int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
        //if(0 != ret)
        //  loge("fail to set vedio layer black!");
    }

    config_scn0.enable = 0;
    //if(lc->fdDisplay>=0){
    //  int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
    //    if(0 != ret)
    //      loge("fail to Destroy layer info fd=%d!",lc->fdDisplay);
    //  usleep(100000);
    //}
    if (lc->fdDisplay >= 0) {
        loge("dbg1 reset");
        int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
        if (0 != ret)
            loge("fail to reset layer info !");
        usleep(100000);//this is abug for disp layer bg green
    }
#endif

    //add by fengkun end
    paramStruct_t dmabuf;
    memcpy(&dmabuf, &gLayerDeMemory, sizeof(paramStruct_t));
    for (i = 0; i < lc->nGpuBufferCount; i++) {
        if (lc->mBufferInfo[i].pPicture.pData0 != 0) {
            //CdcMemPfree(lc->pMemops, lc->mBufferInfo[i].pPicture.pData0);
            //CdxIonPfree(lc->mBufferInfo[i].pPicture.pData0);
            //lc->mBufferInfo[i].pPicture.pData0 = 0;
            dmabuf.vir = (uint8_t *)lc->mBufferInfo[i].pPicture.pData0;
            allocFree(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
            lc->mBufferInfo[i].pPicture.pData0 = 0;
        }
    }


    return 0;
}


static void __LayerRelease(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerRelease error lc=NULL\n");
        return ;
    }

    logw("Layer release");
    //add by fengkun

    paramStruct_t dmabuf;
    memcpy(&dmabuf, &gLayerDeMemory, sizeof(paramStruct_t));
#ifdef USE_LIB_DISP
    HwDisplay* mcd = NULL;
    mcd = HwDisplay::getInstance();
    mcd->aut_hwd_layer_close(mcd->layerObj[0]);
    if (g_screen1_type) {
        mcd->aut_hwd_layer_close(mcd->layerObj[1]);
    }
    usleep(100000);
#else
    unsigned int     args[4];
    args[0] = 0;
    args[1] = (unsigned int)(&config_scn0);
    args[2] = 1;
    //if(lc->fdDisplay>=0){
    //  int t_Ysize = config_scn0.info.fb.size[0].width*config_scn0.info.fb.size[0].height;
    //  memset((void*)gLayerDeMemory.vir, 0x00, t_Ysize);   //make it black
    //  memset((void*)gLayerDeMemory.vir +t_Ysize, 0x80, t_Ysize>>1);
    //  int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
    //    if(0 != ret)
    //      loge("fail to set vedio layer black!");
    //}

    config_scn0.enable = 0;
    if (lc->fdDisplay >= 0) {
        int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
        if (0 != ret)
            loge("fail to Destroy layer info fd=%d!", lc->fdDisplay);
        usleep(100000);
    }
#endif
    logd(" release layer info sleep after disable layer");
    for (i = 0; i < lc->nGpuBufferCount; i++) {
        if (lc->mBufferInfo[i].pPicture.pData0 != 0) {
            dmabuf.vir = (uint8_t *)lc->mBufferInfo[i].pPicture.pData0;
            allocFree(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
            lc->mBufferInfo[i].pPicture.pData0 = 0;
        }
    }
    //if(lc->dmode>0)
    //aut_imgtr_release(&lc->imgtr);

    for (i = 0; i < 4; i++) {
        if (lc->mPreviewAddrVir[i] != 0) {
            dmabuf.vir = lc->mPreviewAddrVir[i];
            allocFree(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
            lc->mPreviewAddrPhy[i] = 0;
            lc->mPreviewAddrVir[i] = 0;
        }
    }

}

static void __LayerDestroy(LayerCtrl* l)
{
    LayerContext* lc;
    logd("__LayerDestroy ");

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerDestroy error lc=NULL\n");
        return ;
    }

#if 0
    //add by fengkun
    unsigned int     args[4];
    config_scn0.enable = 0;
    args[0] = 0;
    args[1] = (unsigned int)(&config_scn0);
    args[2] = 1;
    int ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG, (void*)args);
    if (0 != ret)
        loge("fail to Destroy layer info !");
    usleep(100000);
    loge(" Destroy layer info sleep after disable layer");
    for (i = 0; i < lc->nGpuBufferCount; i++) {
        if (lc->mBufferInfo[i].pPicture.pData0 != 0) {
            CdcMemPfree(lc->pMemops, lc->mBufferInfo[i].pPicture.pData0);
            lc->mBufferInfo[i].pPicture.pData0 = 0;
        }
    }
    //sleep(1);
    //loge(" Destroy layer info sleep after free  layer mem");
#endif

#ifdef USE_LIB_DISP

#else
    if (lc->fdDisplay == 0)
        logw("__LayerDestroy fd ==0");
    if (lc->fdDisplay >= 0)
        close(lc->fdDisplay);
    lc->fdDisplay = -1;
#endif
    if (gLayerDeMemory.ops)
        allocClose(MEM_TYPE_CDX_NEW, &gLayerDeMemory, NULL);
    lc->crop_w = 0;
    lc->crop_h = 0;
    g_lc = NULL;
    free(lc);
}


static int __LayerSetDisplayBufferSize(LayerCtrl* l, int nWidth, int nHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetDisplayBufferSize error lc=NULL\n");
        return -1;
    }
    logv("__LayerSetDisplayBufferSize,  displayWidth = %d, \
              displayHeight = %d",
         nWidth, nHeight);

    lc->nWidth         = nWidth;
    lc->nHeight        = nHeight;
    lc->nDisplayWidth  = nWidth;
    lc->nDisplayHeight = nHeight;
    lc->nLeftOff       = 0;
    lc->nTopOff        = 0;
    lc->bLayerInitialized = 0;

    if (lc->bVideoWithTwoStreamFlag == 1) {
        //* display the whole buffer region when 3D
        //* as we had init align-edge-region to black. so it will be look ok.
        int nScaler = 2;
        lc->nDisplayHeight = lc->nDisplayHeight * nScaler;
    }

    return 0;
}
int __LayerSetDisplayView(int nLeftOff, int nTopOff,
                          int nDisplayWidth, int nDisplayHeight)
{
    LayerContext* lc = g_lc;
    if (g_lc == NULL) {
        loge("__LayerSetDisplayView error g_lc=NULL\n");
        return -1;
    }
    lc->view_x = nLeftOff;
    lc->view_y = nTopOff;
    lc->view_w = nDisplayWidth;
    lc->view_h = nDisplayHeight;
    return 0;
}

int __LayerSetSrcCrop(int nLeftOff, int nTopOff,
                      int nDisplayWidth, int nDisplayHeight)
{
    LayerContext* lc = g_lc;
    logd(" __LayerSetSrcCrop\n");
    if (g_lc == NULL) {
        loge("__LayerSetSrcCrop error g_lc=NULL\n");
        return -1;
    }
    logd("[%d %d %d %d]\n", nLeftOff, nTopOff, nDisplayWidth, nDisplayHeight);
    lc->crop_x = nLeftOff;
    lc->crop_y = nTopOff;
    lc->crop_w = nDisplayWidth;
    lc->crop_h = nDisplayHeight;

    return 0;


}
int __LayerGetCurBuffInfo(int* with, int* height, uint8_t** vir_y)
{
    LayerContext* lc = g_lc;
    if (lc == NULL) {
        loge("__LayerGetCurBuffInfo error lc=NULL\n");
        return -1;
    }
    *with  = lc->nWidth;
    *height = lc->nHeight;

    *vir_y = lc->mCurDispAddrVirY;

    return 0;
}

//* Description: set initial param -- display region
static int __LayerSetDisplayRegion(LayerCtrl* l, int nLeftOff, int nTopOff,
                                   int nDisplayWidth, int nDisplayHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetDisplayRegion error lc=NULL\n");
        return -1;
    }

    logv("__LayerSetDisplayRegion, leftOffset = %d, topOffset = %d, displayWidth = %d, \
              displayHeight = %d",
         nLeftOff, nTopOff, nDisplayWidth, nDisplayHeight);

    if (nDisplayWidth == 0 && nDisplayHeight == 0) {
        return -1;
    }

    lc->nDisplayWidth     = nDisplayWidth;
    lc->nDisplayHeight    = nDisplayHeight;
    lc->nLeftOff          = nLeftOff;
    lc->nTopOff           = nTopOff;

    if (lc->bVideoWithTwoStreamFlag == 1) {
        //* display the whole buffer region when 3D
        //* as we had init align-edge-region to black. so it will be look ok.
        int nScaler = 2;
        lc->nDisplayHeight = lc->nHeight * nScaler;
    }

    return 0;
}

//* Description: set initial param -- display pixelFormat
static int __LayerSetDisplayPixelFormat(LayerCtrl* l, enum EPIXELFORMAT ePixelFormat)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetDisplayPixelFormat error lc=NULL\n");
        return -1;
    }

    logv("Layer set expected pixel format, format = %d", (int)ePixelFormat);

    if (ePixelFormat == PIXEL_FORMAT_NV12 ||
        ePixelFormat == PIXEL_FORMAT_NV21 ||
        ePixelFormat == PIXEL_FORMAT_YV12) {         //* add new pixel formats supported by gpu here.
        lc->eDisplayPixelFormat = ePixelFormat;
    } else {
        logv("receive pixel format is %d, not match.", lc->eDisplayPixelFormat);
        return -1;
    }

    return 0;
}
#if 0
//* Description: set initial param -- deinterlace flag
static int __LayerSetDeinterlaceFlag(LayerCtrl* l, int bFlag)
{
    LayerContext* lc;
    (void)bFlag;
    lc = (LayerContext*)l;

    return 0;
}
#endif

//* Description: set buffer timestamp -- set this param every frame
static int __LayerSetBufferTimeStamp(LayerCtrl* l, int64_t nPtsAbs)
{
    //LayerContext* lc;
    //(void)nPtsAbs;
    //
    //lc = (LayerContext*)l;

    return 0;
}
#if 0
static int __LayerGetRotationAngle(LayerCtrl* l)
{
    LayerContext* lc;
    //int nRotationAngle = 0;

    lc = (LayerContext*)l;

    return 0;
}
#endif
static int __LayerCtrlShowVideo(LayerCtrl* l)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->bLayerShowed = 1;

    return 0;
}

static int __LayerCtrlHideVideo(LayerCtrl* l)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->bLayerShowed = 0;

    return 0;
}

static int __LayerCtrlIsVideoShow(LayerCtrl* l)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerCtrlIsVideoShow error lc=NULL\n");
        return -1;
    }

    return lc->bLayerShowed;
}

static int  __LayerCtrlHoldLastPicture(LayerCtrl* l, int bHold)
{
    logd("LayerCtrlHoldLastPicture, bHold = %d", bHold);

    //LayerContext* lc;
    //lc = (LayerContext*)l;

    return 0;
}

static int __LayerDequeueBuffer(LayerCtrl* l, VideoPicture** ppVideoPicture, int bInitFlag)
{
    LayerContext* lc;
    int i = 0;
    VPictureNode*     nodePtr;
    //BufferInfoT bufInfo;
    VideoPicture* pPicture = NULL;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerDequeueBuffer error lc=NULL\n");
        return -1;
    }

    if (lc->bLayerInitialized == 0) {
        if (setLayerBuffer(lc) != 0) {
            loge("can not initialize layer.");
            return -1;
        }

        lc->bLayerInitialized = 1;
    }

    if (bInitFlag == 1) {
        for (i = 0; i < lc->nGpuBufferCount; i++) {
            if (lc->mBufferInfo[i].nUsedFlag == 0) {
                //* set the buffer address
                pPicture = *ppVideoPicture;
                pPicture = &lc->mBufferInfo[i].pPicture;

                lc->mBufferInfo[i].nUsedFlag = 1;
                break;
            }
        }
    } else {
        while (lc->pPictureListHeader == NULL) {
            logd(" lc->pPictureListHeader == NULL");
            usleep(2000);
        }

        if (lc->pPictureListHeader != NULL) {
            nodePtr = lc->pPictureListHeader;
            i = 1;
            while (nodePtr->pNext != NULL) {
                i++;
                nodePtr = nodePtr->pNext;
            }

#ifdef USE_LIB_DISP
            HwDisplay* mcd = NULL;
            mcd = HwDisplay::getInstance();
            int screenId = 0;
            int ch = 0;
            int layerId = 0;
#endif
            //* return one picture.
            if (i > GetConfigParamterInt("pic_4list_num", 3)) {
                nodePtr = lc->pPictureListHeader;
                lc->pPictureListHeader = lc->pPictureListHeader->pNext;
                //---------------------------
                if (nodePtr->bValid) {
                    int nCurFrameId;
                    int nWaitTime;
                    //int disp0 = 0;
                    //int disp1 = 0;

                    nWaitTime = 50000;  //* max frame interval is 1000/24fps = 41.67ms,
                    //  we wait 50ms at most.

                    do {
#ifdef USE_LIB_DISP
                        nCurFrameId = mcd->getLayerFrameId(screenId, ch, layerId);
#else
                        unsigned int args[3];
                        args[1] = 0; //chan(0 for video)
                        args[2] = 0; //layer_id
                        args[0] = 0; //disp
                        nCurFrameId = ioctl(lc->fdDisplay, DISP_LAYER_GET_FRAME_ID, args);
#endif
                        //logv("nCurFrameId hdmi = %d, pPicture id = %d",
                        //      nCurFrameId, nodePtr->pPicture->nID);
                        if (nCurFrameId != nodePtr->pPicture->nID) {
                            break;
                        } else {
                            if (nWaitTime <= 0) {
                                //loge("check frame id fail, maybe something error happen.currFrameId=%d,nodePicId=%d",nCurFrameId,nodePtr->pPicture->nID);
                                break;
                            } else {
                                usleep(5000);
                                nWaitTime -= 5000;
                            }
                        }
                    } while (1);
                }
                pPicture = nodePtr->pPicture;
                nodePtr->bUsed = 0;
                i--;

            }
        }
    }

    //logv("** dequeue  pPicture(%p), id(%d)", pPicture, pPicture->nBufId);
    *ppVideoPicture = pPicture;
    return 0;
}

// this method should block here,
static int __LayerQueueBuffer(LayerCtrl* l, VideoPicture* pBuf, int bValid)
{
    LayerContext* lc  = NULL;

    int               i;
    VPictureNode*     newNode;
    VPictureNode*     nodePtr;
    //BufferInfoT    bufInfo;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerQueueBuffer error lc=NULL\n");
        return -1;
    }

    //logv("** queue , pPicture(%p)", pBuf);

    if (lc->bLayerInitialized == 0) {
        if (setLayerBuffer(lc) != 0) {
            loge("can not initialize layer.");
            return -1;
        }

        lc->bLayerInitialized = 1;
    }

    if (bValid) {
        if (SetLayerParam(lc, pBuf) != 0) {
            loge("can not initialize layer.");
            return -1;
        }
    }

    // *****************************************
    // *****************************************
    //  TODO: Display this video picture here () blocking
    //
    // *****************************************

    newNode = NULL;
    for (i = 0; i < NUM_OF_PICTURES_KEEP_IN_NODE; i++) {
        if (lc->picNodes[i].bUsed == 0) {
            newNode = &lc->picNodes[i];
            newNode->pNext = NULL;
            newNode->bUsed = 1;
            newNode->pPicture = pBuf;
            newNode->bValid = bValid;
            break;
        }
    }
    if (i == NUM_OF_PICTURES_KEEP_IN_NODE) {
        loge("*** picNode is full when queue buffer");
        return -1;
    }

    if (lc->pPictureListHeader != NULL) {
        nodePtr = lc->pPictureListHeader;
        while (nodePtr->pNext != NULL) {
            nodePtr = nodePtr->pNext;
        }
        nodePtr->pNext = newNode;
    } else {
        lc->pPictureListHeader = newNode;
    }

    return 0;
}

static int __LayerSetDisplayBufferCount(LayerCtrl* l, int nBufferCount)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetDisplayBufferCount error lc=NULL\n");
        return -1;
    }

    logv("LayerSetBufferCount: count = %d", nBufferCount);

    lc->nGpuBufferCount = nBufferCount;

    if (lc->nGpuBufferCount > GPU_BUFFER_NUM)
        lc->nGpuBufferCount = GPU_BUFFER_NUM;

    return lc->nGpuBufferCount;
}

static int __LayerGetBufferNumHoldByGpu(LayerCtrl* l)
{
    (void)l;
    return GetConfigParamterInt("pic_4list_num", 3);
}

static int __LayerGetDisplayFPS(LayerCtrl* l)
{
    (void)l;
    return 60;
}

static void __LayerResetNativeWindow(LayerCtrl* l, void* pNativeWindow)
{
    logd("LayerResetNativeWindow : %p ", pNativeWindow);

    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerResetNativeWindow error lc=NULL\n");
        return ;
    }

    lc->bLayerInitialized = 0;

    return ;
}

static VideoPicture* __LayerGetBufferOwnedByGpu(LayerCtrl* l)
{
    LayerContext* lc;
    VideoPicture* pPicture = NULL;
    BufferInfoT bufInfo;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerGetBufferOwnedByGpu error lc=NULL\n");
        return NULL;
    }

    int i;
    for (i = 0; i < lc->nGpuBufferCount; i++) {
        bufInfo = lc->mBufferInfo[i];
        if (bufInfo.nUsedFlag == 1) {
            bufInfo.nUsedFlag = 0;
            pPicture = &bufInfo.pPicture;
            break;
        }
    }
    return pPicture;
}

static int __LayerSetVideoWithTwoStreamFlag(LayerCtrl* l, int bVideoWithTwoStreamFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetVideoWithTwoStreamFlag error lc=NULL\n");
        return -1;
    }
    logv("LayerSetIsTwoVideoStreamFlag, flag = %d", bVideoWithTwoStreamFlag);
    lc->bVideoWithTwoStreamFlag = bVideoWithTwoStreamFlag;

    return 0;
}

static int __LayerSetIsSoftDecoderFlag(LayerCtrl* l, int bIsSoftDecoderFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetIsSoftDecoderFlag error lc=NULL\n");
        return -1;
    }

    logv("LayerSetIsSoftDecoderFlag, flag = %d", bIsSoftDecoderFlag);
    lc->bIsSoftDecoderFlag = bIsSoftDecoderFlag;

    return 0;
}

//* Description: the picture buf is secure
static int __LayerSetSecure(LayerCtrl* l, int bSecure)
{
    logv("__LayerSetSecure, bSecure = %d", bSecure);
    //*TODO
    LayerContext* lc;

    lc = (LayerContext*)l;
    if (lc == NULL) {
        loge("__LayerSetSecure error lc=NULL\n");
        return -1;
    }

    lc->bProtectFlag = bSecure;

    return 0;
}

static int __LayerReleaseBuffer(LayerCtrl* l, VideoPicture* pPicture)
{
    logw("***LayerReleaseBuffer");
#if 0
    LayerContext* lc;
    lc = (LayerContext*)l;
    if (pPicture == NULL) {
        loge("__LayerReleaseBuffer error pPicture=NULL\n");
        return -1;
    }
#endif
    paramStruct_t dmabuf;
    memcpy(&dmabuf, &gLayerDeMemory, sizeof(paramStruct_t));
    dmabuf.vir = (uint8_t *)pPicture->pData0;
    allocFree(MEM_TYPE_CDX_NEW, &dmabuf, NULL);
    return 0;
}

static int __LayerControl(LayerCtrl* l, int cmd, void* para)
{
    LayerContext* lc = (LayerContext*)l;

    CDX_UNUSE(para);

    switch (cmd) {

        case CDX_LAYER_CMD_SET_HDR_INFO: {
            //LayerSetHdrInfo(l, (const FbmBufInfo *)para);
            printf("LayerSetHdrInfo,no impliment in linux\n");
            break;
        }

        case CDX_LAYER_CMD_SET_NATIVE_WINDOW: {
            lc->base.pNativeWindow = (void*)para;
            printf("SET_NATIVE_WINDOW,no impliment in linux\n");
            break;
        }
        default:
            break;
    }

    return 0;
}
#if 0

static int __LayerControl(LayerCtrl* l, int cmd, void* para)
{
    LayerContext* lc = (LayerContext*)l;

    CDX_UNUSE(para);

    switch (cmd) {
        default:
            break;
    }
    return 0;
}
#endif
static LayerControlOpsT mLayerControlOps = {
release:
    __LayerRelease,

setSecureFlag:
    __LayerSetSecure,
setDisplayBufferSize:
    __LayerSetDisplayBufferSize,
setDisplayBufferCount:
    __LayerSetDisplayBufferCount,
setDisplayRegion:
    __LayerSetDisplayRegion,
setDisplayPixelFormat:
    __LayerSetDisplayPixelFormat,
setVideoWithTwoStreamFlag:
    __LayerSetVideoWithTwoStreamFlag,
setIsSoftDecoderFlag:
    __LayerSetIsSoftDecoderFlag,
setBufferTimeStamp:
    __LayerSetBufferTimeStamp,

resetNativeWindow :
    __LayerResetNativeWindow,
getBufferOwnedByGpu:
    __LayerGetBufferOwnedByGpu,
getDisplayFPS:
    __LayerGetDisplayFPS,
getBufferNumHoldByGpu:
    __LayerGetBufferNumHoldByGpu,

ctrlShowVideo :
    __LayerCtrlShowVideo,
ctrlHideVideo:
    __LayerCtrlHideVideo,
ctrlIsVideoShow:
    __LayerCtrlIsVideoShow,
ctrlHoldLastPicture :
    __LayerCtrlHoldLastPicture,

dequeueBuffer:
    __LayerDequeueBuffer,
queueBuffer:
    __LayerQueueBuffer,
releaseBuffer:
    __LayerReleaseBuffer,
reset:
    __LayerReset,
destroy:
    __LayerDestroy,
control:
    __LayerControl
};

LayerCtrl* LayerCreate_DE()
{
    LayerContext* lc;
    struct disp_layer_info layerInfo;

    int screen_id;

    logd("libsdk_player version:%s, LayerCreate\n", MODULE_VERSION);

    lc = (LayerContext*)malloc(sizeof(LayerContext));
    if (lc == NULL) {
        loge("malloc memory fail.");
        return NULL;
    }
    memset(lc, 0, sizeof(LayerContext));
    lc->imgtr.trfd = -1;
    lc->fdDisplay = -1;
#ifdef USE_LIB_DISP
    HwDisplay* mcd = NULL;
    mcd = HwDisplay::getInstance();

    mcd->ScreenId[0] = 0;
    mcd->ChanelId[0] = 0;
    mcd->LayerId[0] = 0;
    //mcd->layerObj[0] = mcd->aut_hwd_layer_request(&mcd->Surface[0],
    //                   mcd->ScreenId[0], mcd->ChanelId[0], mcd->LayerId[0]);
#if 0
    if (g_screen1_type) {
        mcd->ScreenId[1] = 1;
        mcd->ChanelId[1] = 0;
        mcd->LayerId[1] = 0;
        mcd->layerObj[1] = mcd->aut_hwd_layer_request(&mcd->Surface[1],
                                                      mcd->ScreenId[1], mcd->ChanelId[1], mcd->LayerId[1]);
        mcd->hwd_other_screen(1, g_screen1_type, g_screen1_mode);
    }
#endif

#else
    unsigned int args[4];
    lc->fdDisplay = open("/dev/disp", O_RDWR);
    if (lc->fdDisplay < 0) {
        loge("open disp failed ret=%d err:%s", lc->fdDisplay, strerror(errno));
        free(lc);
        return NULL;
    }
#endif

    lc->base.ops = &mLayerControlOps;
    lc->eDisplayPixelFormat = PIXEL_FORMAT_NV21;

    memset(&layerInfo, 0, sizeof(struct disp_layer_info));

    //  get screen size.
#ifdef USE_LIB_DISP
    screen_id = 0;
    lc->nScreenWidth = mcd->getScreenWidth(screen_id);
    lc->nScreenHeight = mcd->getScreenHeight(screen_id);
    logd("screen:w %d, screen:h %d disp", lc->nScreenWidth, lc->nScreenHeight);
#else
    args[0] = 0;
    lc->nScreenWidth  = ioctl(lc->fdDisplay, DISP_GET_SCN_WIDTH, (void*)args);
    lc->nScreenHeight = ioctl(lc->fdDisplay, DISP_GET_SCN_HEIGHT, (void*)args);
    logd("screen:w %d, screen:h %d disp fd=%d", lc->nScreenWidth, lc->nScreenHeight, lc->fdDisplay);
#endif
    //CdxIonOpen();
    memset(&gLayerDeMemory, 0, sizeof(paramStruct_t));

    int ret = allocOpen(MEM_TYPE_CDX_NEW, &gLayerDeMemory, NULL);
    if (ret < 0) {
        loge("allocOpen failed");
    }

    g_lc = lc;
    return &lc->base;
}

int set_second_output(int type, int mode)
{
    g_screen1_type = type;
    g_screen1_mode = mode;
    if (NULL != g_lc) {
        loge("set_second_output failed\n");
        return -1;
    }
    return 0;
}

typedef struct __screen_param {
    int enable;
    int screen;
    int screenW;
    int screenH;
    int type;
    int mode;
} sc_param;

int set_output(void * param)
{
#ifdef USE_LIB_DISP
    if (NULL == param) {
        printf("set_output failed\n");
    }

    sc_param *sc_p = (sc_param *)param;

    HwDisplay* mcd = NULL;
    mcd = HwDisplay::getInstance();
    if (mcd == NULL) {
        printf("mcd is NULL");
        return -1;
    }

    int screen_id = sc_p->screen;
    int ch_id = 0;
    int lyr_id = 0;

    if (0 == sc_p->screen) {
        g_screen0_user_init = sc_p->enable;
        g_screen0_w = sc_p->screenW;
        g_screen0_h = sc_p->screenH;
        g_screen0_type = sc_p->type;
        g_screen0_mode = sc_p->mode;
    } else if (1 == sc_p->screen) {
        g_screen1_user_init = sc_p->enable;
        g_screen1_w = sc_p->screenW;
        g_screen1_h = sc_p->screenH;
        g_screen1_type = sc_p->type;
        g_screen1_mode = sc_p->mode;
    } else {
        return -1;
    }

    mcd->hwd_other_screen(sc_p->screen, sc_p->type, sc_p->mode);

    struct view_info frame = {0, 0, sc_p->screenW, sc_p->screenH};
    int layer0 = mcd->aut_hwd_layer_request(&frame, screen_id, ch_id, lyr_id);
    mcd->aut_hwd_layer_sufaceview(layer0, &frame);

#endif

    return 0;
}

int set_corp_ctrl_by_user(int corp_ctrl)
{
    LayerContext* lc = g_lc;
    logd(" __LayerSetSrcCrop\n");
    if (g_lc == NULL) {
        loge("__LayerSetSrcCrop error g_lc=NULL\n");
        return -1;
    }
    lc->user_ctrl_corp = corp_ctrl;
    return 0;
}

