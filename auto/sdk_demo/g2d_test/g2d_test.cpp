#include "sunxiMemInterface.h"
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "type_compat.h"
#include "G2dApi.h"

#define LOG_TAG "g2d_test"
#include "sdklog.h"

using namespace g2dapi;

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

static G2dmode g_avail_mode[] = {
#ifdef __T527__
    G2D_DECOMPOSE, G2D_COMPOSE, G2D_ROTATE, G2D_HFLIP, G2D_VFLIP,
#else
    G2D_MODE_NULL,
#endif
};

paramStruct_t m_DispMemOps;
paramStruct_t m_DispMemOps_half0;
paramStruct_t m_DispMemOps_half1;

paramStruct_t m_DispMemOps0;
paramStruct_t m_DispMemOps1;
paramStruct_t m_DispMemOps2;
paramStruct_t m_DispMemOps3;

// 4in1 compose
char *pPicPath0 = "/tmp/g2d_test/video0.yuv";
char *pPicPath1 = "/tmp/g2d_test/video1.yuv";
char *pPicPath2 = "/tmp/g2d_test/video2.yuv";
char *pPicPath3 = "/tmp/g2d_test/video3.yuv";
char *pcompPicPath0 = "/tmp/g2d_test/cvideo.yuv";

// 1to4 decompose
char *pdcompPicPath0 = "/tmp/g2d_test/dcvideo.yuv";
char *pdPicPath0 = "/tmp/g2d_test/dvideo0.yuv";
char *pdPicPath1 = "/tmp/g2d_test/dvideo1.yuv";
char *pdPicPath2 = "/tmp/g2d_test/dvideo2.yuv";
char *pdPicPath3 = "/tmp/g2d_test/dvideo3.yuv";

// format_convert
char *pConvertRgbaInPath = "/tmp/g2d_test/1024x600.rgba";
char *pConvertYuvOutPath = "/tmp/g2d_test/1024x600_convert.yuv";
char *pConvertYuvInPath = "/tmp/g2d_test/1024x600.yuv";
char *pConvertRgbaOutPath = "/tmp/g2d_test/1024x600_convert.rgba";

//  scale
char *pScaleRgbaInPath = "/tmp/g2d_test/1024x600.rgba";
char *pScaleRgbaOutPath = "/tmp/g2d_test/1280x720_scaler.rgba";

//  rotate
char *pRotateYuvInPath = "/tmp/g2d_test/1280x720.yuv";
char *pRotateYuvOutPath = "/tmp/g2d_test/720x1280_rotate_90.yuv";

// flips
char *pFlipsRgbaInPath = "/tmp/g2d_test/1024x600.rgba";
char *pFlipsRgbaOutPath = "/tmp/g2d_test/1024x600_h_flips.rgba";

// test for input
#define CKFILE "/tmp/g2d_test/ck.yuv"

int iWidth = 2560, iHeight = 1440 ;
int iSubWidth = 1280, iSubHeight = 720 ;

void printUsage()
{
    printf("================Usage================\n");
    printf("g2d_test 1	  means: 1024x600*rgba scale to 1280x720*rgba\n");
    printf("g2d_test 2	  means: 1*2560x1440 nv21 decompose to 4*720p nv21\n");
    printf("g2d_test 3	  means: 4*720 nv21 compose to 1*2560x1440 nv21\n");
    printf("g2d_test 4	  means: 1280x720 nv21 rotate to 720x1280 nv21\n");
    printf("g2d_test 5	  means: 1024x600 rgba vertical flips\n");
    printf("g2d_test 6	  means: 1024x600 rgba convert to 1024x600 nv21\n");
    printf("g2d_test 7	  means: 1024x600 nv21 convert to 1024x600 rgba\n");
    printf("g2d_test 8	  means: 1024x600 nv21 convert to 1024x600 rgba by phy address\n");
    printf("================usage================\n");
}

int ReadPicFileContent(char *pPicPath, paramStruct_t*pops, int size)
{
    int iRet = 0;

    iRet = allocOpen(MEM_TYPE_CDX_NEW, pops, NULL);
    if (iRet < 0) {
        ALOGD("ion_alloc_open failed");
        return iRet;
    }
    pops->size = size;
    iRet = allocAlloc(MEM_TYPE_CDX_NEW, pops, NULL);
    if (iRet < 0) {
        ALOGD("allocAlloc failed");
        return iRet;
    }

    FILE *fpff = fopen(pPicPath, "rb");
    if (NULL == fpff) {
        fpff = fopen(pPicPath, "rb");
        if (NULL == fpff) {
            ALOGD("fopen %s ERR ", pPicPath);
            allocFree(MEM_TYPE_CDX_NEW, pops, NULL);
            return -1;
        } else {
            ALOGD("fopen %s OK ", pPicPath);
            fread((void *)pops->vir, 1, size, fpff);
            fclose(fpff);
        }
    } else {
        ALOGD("fopen %s OK ", pPicPath);
        fread((void *)pops->vir, 1, size, fpff);
        fclose(fpff);
    }
    flushCache(MEM_TYPE_CDX_NEW, pops, NULL);

    return 0;
}

int WritePicFileContent(char *pPicPath, paramStruct_t*pops, int size)
{
    ALOGD("WritePicFileContent size=%d ", size);
    flushCache(MEM_TYPE_CDX_NEW, pops, NULL);

    FILE *fpff = fopen(pPicPath, "wb");
    if (NULL == fpff) {
        fpff = fopen(pPicPath, "wb");
        if (NULL == fpff) {
            ALOGD("fopen %s ERR ", pPicPath);
            allocFree(MEM_TYPE_CDX_NEW, pops, NULL);
            return -1;
        } else {
            ALOGD("fopen %s OK ", pPicPath);
            fwrite((void *)pops->vir, 1, size, fpff);
            fclose(fpff);
        }
    } else {
        ALOGD("fopen %s OK ", pPicPath);
        fwrite((void *)pops->vir, 1, size, fpff);
        fclose(fpff);
    }


    return 0;
}

int allocPicMem(paramStruct_t*pops, int size)
{
    int iRet = 0;

    iRet = allocOpen(MEM_TYPE_CDX_NEW, pops, NULL);
    if (iRet < 0) {
        ALOGD("ion_alloc_open failed");
        return iRet;
    }
    pops->size = size;
    iRet = allocAlloc(MEM_TYPE_CDX_NEW, pops, NULL);
    if (iRet < 0) {
        ALOGD("allocAlloc failed");
        return iRet;
    }

    return 0;
}

int freePicMem(paramStruct_t*pops)
{
    allocFree(MEM_TYPE_CDX_NEW, pops, NULL);

    return 0;
}

static int checkid(int id)
{
    int m_size;
    int i;

    m_size = sizeof(g_avail_mode)/sizeof(g_avail_mode[0]);

    for (i = 0; i < m_size; i++) {
        if (id == g_avail_mode[i])
            break;
    }

    if (i < m_size)
        return id;

    return G2D_MODE_NULL;
}

int main(int argc, char *argv[])
{
    printf("g2d test version:%s\n", MODULE_VERSION);
    struct timeval t1, t2;

    if (argc < 2) {
        printUsage();
        exit(-1);
    }
    sdk_log_setlevel(6);
    ALOGD("arc=%d, testid=%s", argc, argv[1]);

    int testid = checkid(atoi(argv[1]));

    switch (testid) {
        case G2D_SCALE: {
            /** func: RGBA 1024x600 scale to 1280x720 */

            iSubWidth = 1024;
            iSubHeight = 600;
            int inPicSize = iSubWidth * iSubHeight * 4;
            ReadPicFileContent(pScaleRgbaInPath, &m_DispMemOps0, inPicSize);

            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);

            // reroll back
            // WritePicFileContent(CKFILE, &m_DispMemOps0, inPicSize);

            //alloc output buff
            int outW = 1280;
            int outH = 720;
            int outPicSize = outW * outH * 4;
            allocPicMem(&m_DispMemOps, outPicSize); //output buf
            ALOGD("alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps.ion_buffer.fd_data.aw_fd);
            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start rgb scaler =============");
            gettimeofday(&t1, NULL);

            int src_fd = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            int dst_fd = m_DispMemOps.ion_buffer.fd_data.aw_fd;

            int flag = (int) G2D_BLT_NONE_H;
            int fmt = (int) G2D_FORMAT_ABGR8888;
            int ret = g2dClipByFd(g2dfd, src_fd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                                  dst_fd, fmt, outW, outH, 0, 0, outW, outH);

            gettimeofday(&t2, NULL);
            ALOGD("g2d scale ret:%d, use time =%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            WritePicFileContent(pScaleRgbaOutPath, &m_DispMemOps, outPicSize);

            g2dUnit(g2dfd);

            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }

        case G2D_DECOMPOSE: {
            /** func: 1*2560x1440 NV21 decompose to 4*720p NV21 */

            //read a yuv file to sunximem
            ReadPicFileContent(pdcompPicPath0, &m_DispMemOps, iWidth * iHeight * 3 / 2);

            //alloc de-compse buff
            allocPicMem(&m_DispMemOps0, iSubWidth * iSubHeight * 3 / 2);
            allocPicMem(&m_DispMemOps1, iSubWidth * iSubHeight * 3 / 2);
            allocPicMem(&m_DispMemOps2, iSubWidth * iSubHeight * 3 / 2);
            allocPicMem(&m_DispMemOps3, iSubWidth * iSubHeight * 3 / 2);

            // decompose
            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start yuv 1to4 decompose =============");
            gettimeofday(&t1, NULL);

            /* pass buffer with fd */
            int infd = m_DispMemOps.ion_buffer.fd_data.aw_fd;
            int outfd[4];
            outfd[0] = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            outfd[1] = m_DispMemOps1.ion_buffer.fd_data.aw_fd;
            outfd[2] = m_DispMemOps2.ion_buffer.fd_data.aw_fd;
            outfd[3] = m_DispMemOps3.ion_buffer.fd_data.aw_fd;
            int ret = -1;
            int flag = (int)G2D_ROT_0;
            int fmt = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;
            ret = g2dClipByFd(g2dfd, infd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iWidth, iHeight,
                              outfd[0], fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight);

            ret |= g2dClipByFd(g2dfd, infd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iWidth, iHeight,
                               outfd[1], fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight);

            ret |= g2dClipByFd(g2dfd, infd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iWidth, iHeight,
                               outfd[2], fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight);

            ret |= g2dClipByFd(g2dfd, infd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iWidth, iHeight,
                               outfd[3], fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight);

            gettimeofday(&t2, NULL);
            ALOGD("g2d decompose ret:%d, use time =%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            g2dUnit(g2dfd);

            //save de-compose buffer
            WritePicFileContent(pdPicPath0, &m_DispMemOps0, iSubWidth * iSubHeight * 3 / 2);
            WritePicFileContent(pdPicPath1, &m_DispMemOps1, iSubWidth * iSubHeight * 3 / 2);
            WritePicFileContent(pdPicPath2, &m_DispMemOps2, iSubWidth * iSubHeight * 3 / 2);
            WritePicFileContent(pdPicPath3, &m_DispMemOps3, iSubWidth * iSubHeight * 3 / 2);

            //free mem
            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            freePicMem(&m_DispMemOps1);
            freePicMem(&m_DispMemOps2);
            freePicMem(&m_DispMemOps3);
            break;
        }

        case G2D_COMPOSE: {
            /** func: 4*720 NV21 compose to 1*2560x1440 NV21 */

            //read four file to sunximem
            ReadPicFileContent(pPicPath0, &m_DispMemOps0, iSubWidth * iSubHeight * 3 / 2);
            ReadPicFileContent(pPicPath1, &m_DispMemOps1, iSubWidth * iSubHeight * 3 / 2);
            ReadPicFileContent(pPicPath2, &m_DispMemOps2, iSubWidth * iSubHeight * 3 / 2);
            ReadPicFileContent(pPicPath3, &m_DispMemOps3, iSubWidth * iSubHeight * 3 / 2);

            // reroll back
            // WritePicFileContent(CKFILE, &m_DispMemOps0, iSubWidth * iSubHeight * 3 / 2);

            //alloc compose buff
            allocPicMem(&m_DispMemOps, iWidth * iHeight * 3 / 2);

            //compose
            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start yuv 4in1 compose =============");
            gettimeofday(&t1, NULL);

            int outfd = m_DispMemOps.ion_buffer.fd_data.aw_fd;
            int infd[4];
            infd[0] = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            infd[1] = m_DispMemOps1.ion_buffer.fd_data.aw_fd;
            infd[2] = m_DispMemOps2.ion_buffer.fd_data.aw_fd;
            infd[3] = m_DispMemOps3.ion_buffer.fd_data.aw_fd;
            int ret = -1;
            int flag = (int)G2D_ROT_0;
            int fmt = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;
            ret = g2dClipByFd(g2dfd, infd[0], flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                              outfd, fmt, iSubWidth, iSubHeight, 0, 0, iWidth, iHeight);
            ret |= g2dClipByFd(g2dfd, infd[1], flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                               outfd, fmt, iSubWidth, iSubHeight, iSubWidth, 0, iWidth, iHeight);
            ret |= g2dClipByFd(g2dfd, infd[2], flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                               outfd, fmt, iSubWidth, iSubHeight, 0, iSubHeight, iWidth, iHeight);
            ret |= g2dClipByFd(g2dfd, infd[3], flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                               outfd, fmt, iSubWidth, iSubHeight, iSubWidth, iSubHeight, iWidth, iHeight);

            gettimeofday(&t2, NULL);
            ALOGD("g2d compose ret:%d, use time =%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            //save compose buffer
            WritePicFileContent(pcompPicPath0, &m_DispMemOps, iWidth * iHeight * 3 / 2);

            g2dUnit(g2dfd);

            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            freePicMem(&m_DispMemOps1);
            freePicMem(&m_DispMemOps2);
            freePicMem(&m_DispMemOps3);
            break;
        }

        case G2D_ROTATE: {
            /** func: 1280x720 NV21 rotate to 720x1280 NV21 */

            iSubWidth = 1280;
            iSubHeight = 720;
            int inPicSize = iSubWidth * iSubHeight * 3 / 2;
            ReadPicFileContent(pRotateYuvInPath, &m_DispMemOps0, inPicSize);

            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);

            // reroll back
            // WritePicFileContent(CKFILE, &m_DispMemOps0, inPicSize);

            //alloc output buff
            int outW = 720;
            int outH = 1280;
            int outPicSize = outW * outH * 3 / 2;
            allocPicMem(&m_DispMemOps, outPicSize); //output buf
            ALOGD("alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps.ion_buffer.fd_data.aw_fd);
            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start yuv rotate =============");
            gettimeofday(&t1, NULL);

            int src_fd = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            int dst_fd = m_DispMemOps.ion_buffer.fd_data.aw_fd;

            int flag = (int)G2D_ROT_90;
            int fmt = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;
            int ret = g2dClipByFd(g2dfd, src_fd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                                  dst_fd, fmt, outW, outH, 0, 0, outW, outH);

            gettimeofday(&t2, NULL);
            ALOGD("g2d rotate ret:%d, use time =%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            WritePicFileContent(pRotateYuvOutPath, &m_DispMemOps, outPicSize);

            g2dUnit(g2dfd);

            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }

        case G2D_HFLIP: {
            /** func: 1024x600 rgba horizontal flips */

            iSubWidth = 1024;
            iSubHeight = 600;
            int inPicSize = iSubWidth * iSubHeight * 4;
            ReadPicFileContent(pFlipsRgbaInPath, &m_DispMemOps0, inPicSize);

            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);

            // reroll back
            // WritePicFileContent(CKFILE, &m_DispMemOps0, inPicSize);

            //alloc output buff
            int outW = 1024;
            int outH = 600;
            int outPicSize = outW * outH * 4;
            allocPicMem(&m_DispMemOps, outPicSize); //output buf
            ALOGD("alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps.ion_buffer.fd_data.aw_fd);
            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start rgb v flips =============");
            gettimeofday(&t1, NULL);

            int src_fd = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            int dst_fd = m_DispMemOps.ion_buffer.fd_data.aw_fd;

            int flag = (int)G2D_ROT_H;
            int fmt = (int)G2D_FORMAT_RGBA8888;
            int ret = g2dClipByFd(g2dfd, src_fd, flag, fmt, iSubWidth, iSubHeight, 0, 0, iSubWidth, iSubHeight,
                                  dst_fd, fmt, outW, outH, 0, 0, outW, outH);

            gettimeofday(&t2, NULL);
            ALOGD("g2d v flips ret:%d, use time:%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            WritePicFileContent(pFlipsRgbaOutPath, &m_DispMemOps, outPicSize);

            g2dUnit(g2dfd);

            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }
        case G2D_FORMAT_CONVERT_RGBA2YUV_ByFd: {
            /** func: 1024x600 rgba convert to 1024x600 yuv(NV21) */

            /* input param */
            int src_width = 1024;
            int src_height = 600;
            int src_format = (int)G2D_FORMAT_ABGR8888;
            int src_size = src_width * src_height * 4;

            /* alloc input buffer */
            ReadPicFileContent(pConvertRgbaInPath, &m_DispMemOps0, src_size);
            int src_fd = m_DispMemOps0.ion_buffer.fd_data.aw_fd;
            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);

            /* reroll back */
            // WritePicFileContent(CKFILE, &m_DispMemOps0, src_size);

            /* output param */
            int dst_width = 1024;
            int dst_height = 600;
            int dst_format = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;//G2D_FORMAT_RGBA8888;//
            int dst_size = src_width * src_height * 3 / 2;

            /* alloc output buffer */
            allocPicMem(&m_DispMemOps, dst_size);
            memset((void*)m_DispMemOps.vir, 0xff, dst_size);
            ALOGD("alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps.ion_buffer.fd_data.aw_fd);
            int dst_fd = m_DispMemOps.ion_buffer.fd_data.aw_fd;

            /* commond flag */
            int flag = (int) G2D_BLT_NONE_H;

            int g2dfd = g2dInit();

            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start rgb convert to yuv =============");
            gettimeofday(&t1, NULL);

            int ret = g2dClipByFd(g2dfd, src_fd, flag, src_format, src_width, src_height, 0, 0, src_width, src_height,
                                  dst_fd, dst_format, dst_width, dst_height, 0, 0, dst_width, dst_height);

            gettimeofday(&t2, NULL);
            ALOGD("g2d convert ret:%d, use time:%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));
            WritePicFileContent(pConvertYuvOutPath, &m_DispMemOps, dst_size);
            g2dUnit(g2dfd);

            /* free memory */
            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }

        case G2D_FORMAT_CONVERT_YUV2RGBA_ByFd: {
            /** func: 1024x600 yuv convert to 1024x600 agba */

            /* input param */
            int src_width = 1024;
            int src_height = 600;
            int src_format = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;
            int src_size = src_width * src_height * 3 / 2;

            /* output param */
            int dst_width = 1024;
            int dst_height = 600;
            int dst_format = (int)G2D_FORMAT_ABGR8888;
            int dst_size = src_width * src_height * 4;

            /* commond flag */
            int flag = (int)G2D_BLT_NONE_H;

            /* alloc input buffer */
            ReadPicFileContent(pConvertYuvInPath, &m_DispMemOps0, src_size);
            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);
            int src_fd = m_DispMemOps0.ion_buffer.fd_data.aw_fd;

            /* reroll back */
            // WritePicFileContent(CKFILE, &m_DispMemOps0, src_size);

            /* alloc output buffer */
            allocPicMem(&m_DispMemOps, dst_size);
            memset((void*)m_DispMemOps.vir, 0xff, dst_size);
            ALOGD("alloc m_DispMemOps.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps.ion_buffer.fd_data.aw_fd);
            int dst_fd = m_DispMemOps.ion_buffer.fd_data.aw_fd;

            int g2dfd = g2dInit();
            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start yuv convert to rgb =============");
            gettimeofday(&t1, NULL);

            int ret = g2dClipByFd(g2dfd, src_fd, flag, src_format, src_width, src_height, 0, 0, src_width, src_height,
                                  dst_fd, dst_format, dst_width, dst_height, 0, 0, dst_width, dst_height);
            gettimeofday(&t2, NULL);

            ALOGD("g2d convert ret:%d, use time:%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));
            WritePicFileContent(pConvertRgbaOutPath, &m_DispMemOps, dst_size);

            g2dUnit(g2dfd);

            /* free memory */
            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }

        case G2D_FORMAT_CONVERT_YUV2RGBA_ByPhy:
            /** func: 1024x600 yuv convert to 1024x600 agba by phy address*/
        {
            /* input param */
            int src_width = 1024;
            int src_height = 600;
            int src_format = (int)G2D_FORMAT_YUV420UVC_U1V1U0V0;
            int src_size = src_width * src_height * 3 / 2;

            /* output param */
            int dst_width = 1024;
            int dst_height = 600;
            int dst_format = (int)G2D_FORMAT_ABGR8888;
            int dst_size = src_width * src_height * 4;

            /* alloc input buffer */
            ReadPicFileContent(pConvertYuvInPath, &m_DispMemOps0, src_size);
            ALOGD("alloc m_DispMemOps0.ion_buffer.fd_data.aw_fd=%d", m_DispMemOps0.ion_buffer.fd_data.aw_fd);

            /* reroll back */
            // WritePicFileContent(CKFILE, &m_DispMemOps0, src_size);

            /* alloc output buffer */
            allocPicMem(&m_DispMemOps, dst_size);
            memset((void*)m_DispMemOps.vir, 0xff, dst_size);

            unsigned long psrc = m_DispMemOps0.phy;
            unsigned long pdst = m_DispMemOps.phy;

            int g2dfd = g2dInit();
            t1.tv_sec = t1.tv_usec = 0;
            t2.tv_sec = t2.tv_usec = 0;
            ALOGD("================== start yuv convert to rgb by phy addr =============");
            gettimeofday(&t1, NULL);

            flushCache(MEM_TYPE_CDX_NEW, &m_DispMemOps0, NULL);
            int ret = g2dFormatConv(g2dfd, (void *)psrc, src_format, src_width, src_height, 0, 0, src_width, src_height,
                                    (void *)pdst, dst_format, dst_width, dst_height, 0, 0, dst_width, dst_height);
            gettimeofday(&t2, NULL);
            flushCache(MEM_TYPE_CDX_NEW, &m_DispMemOps, NULL);

            ALOGD("g2d convert by phy address ret:%d, use time:%lld us", ret,
                  int64_t(t2.tv_sec) * 1000000LL + int64_t(t2.tv_usec) - (int64_t(t1.tv_sec) * 1000000LL + int64_t(t1.tv_usec)));

            WritePicFileContent(pConvertRgbaOutPath, &m_DispMemOps, dst_size);

            g2dUnit(g2dfd);

            /* free memory */
            freePicMem(&m_DispMemOps);
            freePicMem(&m_DispMemOps0);
            break;
        }

        default:
            ALOGE("Invid Param! Platform may not support this mode!\r\n");
            break;
    }

    return 0;
}

