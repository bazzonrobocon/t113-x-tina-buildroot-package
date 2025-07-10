
#define LOG_TAG "record_test"
#include "sdklog.h"
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/fb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <time.h>
#include "aw_ini_parser.h"

#include "CameraDebug.h"
#include "hwdisp2.h"

#include "V4L2CameraDevice2.h"
#include "CallbackNotifier.h"
#include "PreviewWindow.h"
#include "CameraHardware2.h"

#include "audio_hal.h"

#ifndef CDX_V27
#include "log.h"
#endif

#include "vencoder.h"
#include "CdxMuxer.h"
#include "Rtc.h"
#include "StorageManager.h"
#include "DvrFactory.h"
#include "CameraFileCfg.h"
#include <sys/socket.h>
#include <sys/un.h>
#include "Fat.h"
#include "DebugLevelThread.h"
#include "DvrRecordManager.h"

#include "moudle/AutoMount.h"
#include "moudle/Display.h"

#include <mcheck.h>

using namespace android;

#define TEST_H264_OUTPUT

#define YUV_FILE_PATH "/tmp/record_test/720p_record_test.yuv"
#define H264_FILE_PATH "/tmp/record_test/record_out.h264"

void Rec_usernotifyCallback(int32_t msgType, int32_t ext1, int32_t ext2, void *user)
{
    ALOGD("msgType =%d-----data=%p-----%d)", msgType, user);

    if ((msgType & CAMERA_MSG_ERROR) == CAMERA_MSG_ERROR) {
        ALOGE("(msgType =CAMERA_MSG_ERROR)");

    }
#if 0
    if ((msgType & CAMERA_MSG_DVR_NEW_FILE) == CAMERA_MSG_DVR_NEW_FILE) {
        dvr_factory *p_dvr = (dvr_factory *) user;
        prinALOGDtf("(msgType =CAMERA_MSG_DVR_NEW_FILE camera %d idx =%d)", p_dvr->mCameraId, ext1);
    }

    if ((msgType & CAMERA_MSG_DVR_STORE_ERR) == CAMERA_MSG_DVR_STORE_ERR) {
        ALOGD("msgType =CAMERA_MSG_DVR_STORE_ERR)");
        dvr_factory *p_dvr = (dvr_factory *) user;
        p_dvr->mRecordCamera->storage_state = 0;
        p_dvr->stopRecord();
    }
#endif
}

void Rec_userdataCallback(int32_t msgType, char *dataPtr, camera_frame_metadata_t * metadata, void *user)
{

}

void Rec_userdataCallbackTimestamp(nsecs_t timestamp, int32_t msgType, char *dataPtr, void *user)
{
    // V4L2BUF_t *pbuf = (V4L2BUF_t *)(dataPtr);
}

int main(int argc, char *argv[])
{
    printf("recordTest version:%s\n", MODULE_VERSION);

    int ret = 0;
    V4L2BUF_t v4l2_buf;
    unsigned int memtype = MEM_TYPE_CDX_NEW;

    DvrRecordManagerInit();
    int mCameraId  = 0;
    int recordwith = config_get_width(mCameraId);
    int recordheith = config_get_heigth(mCameraId);

    RecordCamera *pRecordCamera = new RecordCamera(mCameraId);
    pRecordCamera->videoEncParmInit(recordwith, recordheith,
                                    recordwith, recordheith, 8, 25, VENC_CODEC_H264);

    pRecordCamera->setCallbacks(Rec_usernotifyCallback, pRecordCamera);

    pRecordCamera->setDuration(1 * 60);

    pRecordCamera->start();
    pRecordCamera->startRecord();

    dma_mem_des_t recordMem;
    memset(&recordMem, 0, sizeof(dma_mem_des_t));
    ret = allocOpen(memtype, &recordMem, NULL);
    if (ret < 0) {
        ALOGE("ion_alloc_open failed");
        return -1;
    }
    recordMem.size = recordwith * recordheith * 3 / 2;
    ret = allocAlloc(memtype, &recordMem, NULL);
    if (ret < 0) {
        ALOGE("allocAlloc failed");
        return -1;
    }

    int retsize = 0;
    static int s_ppsFlag = 0;

    FILE *inputfd = fopen(YUV_FILE_PATH, "rb");
    if (NULL == inputfd) {
        ALOGE("fopen %s faile", YUV_FILE_PATH);
        return -1;
    } else {
        ALOGD("fopen %s OK", YUV_FILE_PATH);
    }

#ifdef TEST_H264_OUTPUT
    FILE *outputfd = fopen(H264_FILE_PATH, "a+");
    if (NULL == outputfd) {
        ALOGE("fopen %s faile", H264_FILE_PATH);
    } else {
        ALOGD("fopen %s OK", H264_FILE_PATH);
    }
#endif

    while (1) {
        retsize = fread((void *)recordMem.vir, 1, recordMem.size, inputfd);
        if (retsize <= 0) {
            ALOGD("---------------input close---------------------");
            fclose(inputfd);
            inputfd = NULL;
            break;
        }
        flushCache(memtype, &recordMem, NULL);
        v4l2_buf.width = recordwith;
        v4l2_buf.height = recordheith;
        v4l2_buf.addrPhyY = recordMem.phy;
        v4l2_buf.addrPhyC = recordMem.phy + v4l2_buf.width * v4l2_buf.height;
        v4l2_buf.addrVirY = recordMem.vir;
        v4l2_buf.addrVirC = recordMem.vir + v4l2_buf.width * v4l2_buf.height;
        v4l2_buf.dmafd = recordMem.ion_buffer.fd_data.aw_fd;
        v4l2_buf.timeStamp = (int64_t) systemTime();

        CdxMuxerPacketT pkt;

        struct timeval lRunTimeEnd;
        gettimeofday(&lRunTimeEnd, NULL);
        long long time1 = lRunTimeEnd.tv_sec * 1000000 + lRunTimeEnd.tv_usec;
        long long time2 = 0;
        long long time3 = 0 ;

#ifdef TEST_H264_OUTPUT
        //encode will malloc pkt.buflen size buf to pkt.buf!we need to free it at h264 test
        ret = pRecordCamera->encode(&v4l2_buf, (char *) &pkt, NULL, NULL);
        if (ret < 0) {
            ALOGW("encode fail");
            return -1;
        }

        if (s_ppsFlag == 0) {
            s_ppsFlag = 1;
            VencHeaderData sps_pps_data;
            VideoEncGetParameter(pRecordCamera->pRecVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
            if (outputfd) {
                fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, outputfd);
            }
        }

        if (outputfd) {
            fwrite((void *)pkt.buf, 1, pkt.buflen, outputfd);
        }
        free(pkt.buf);
        pkt.buf = NULL;
#else
        //output is mp4 in /mnt/sdcard/mmcblk1p1/ dir
        pRecordCamera->dataCallbackTimestamp(v4l2_buf.timeStamp, CAMERA_MSG_VIDEO_FRAME,
                                             (char *)&v4l2_buf, NULL);
#endif

        gettimeofday(&lRunTimeEnd, NULL);
        time2 = lRunTimeEnd.tv_sec * 1000000 + lRunTimeEnd.tv_usec;
        time3 = time2 - time1 ;
        ALOGD("encode use runtime %lldus(%lldms)", time3, time3 / 1000);
        time1 = time2;
    }

    allocFree(memtype, &recordMem, NULL);
    ALOGD("stop record");

    pRecordCamera->stopRecord();
    pRecordCamera->stop();
    pRecordCamera->videoEncDeinit();
    delete pRecordCamera;
    pRecordCamera = NULL;

    if (inputfd) {
        fclose(inputfd);
        inputfd = NULL;
    }

#ifdef TEST_H264_OUTPUT
    if (outputfd) {
        fclose(outputfd);
        outputfd = NULL;
    }
#endif

    ALOGD("recordTest run finish\n");

    return 0;
}
