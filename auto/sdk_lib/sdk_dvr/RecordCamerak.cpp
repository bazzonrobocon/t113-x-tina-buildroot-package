#define LOG_TAG "RecordCameraK"
#include <CameraDebug.h>
#if DBG_CAMERA_RECORD
#define LOG_NDEBUG 0
#endif
#include <CameraLog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include "audio_hal.h"
#include "CdxMuxer.h"
#include "posixTimer.h"
#include "CameraFileCfg.h"
#include "StorageManager.h"
#include "MuxerWriter.h"
#include "RecordCamera.h"
#include "DvrRecordManager.h"

#if 0
#include "rtspapi.h"
#endif

// enable print pts time
#define ENABLE_DEBUG_PTS_TIMEOUT 0

#if ENABLE_DEBUG_PTS_TIMEOUT
#define PTS_TIMEOUT 500          // ms
#define SAVE_FRAME_TIMEOUT 1000  // ms

long long pts_base_time = 0;
int base_pts = 0;
long long frame_base_time = 0;
#endif

/************Macro*********************/
#define _ENCODER_TIME_
#ifdef _ENCODER_TIME_
static long long GetNowUs() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

// long long time1 = 0;
// long long time2 = 0;
// long long time3 = 0;
#endif

/************Variable******************/

/************Function******************/

/**************************************
Function:
Description:
***************************************/

#if DUMP_VIDEOSTREAM
static int saveVideoStream(VencOutputBuffer *pOutputBuffer, const char *out_path, int nTestNum,
                           encode_param_t encode_param) {
    CameraLogD("%s, %d", __func__, __LINE__);
    char name[128];

    // sprintf(name, "%s_%d.jpg", out_path, nTestNum);

    FILE *out_file = NULL;

    if (encode_param.encode_format == VENC_CODEC_JPEG) {
        sprintf(name, "%s_%dx%d_%d.jpg", out_path, encode_param.src_width, encode_param.src_height,
                nTestNum);
        out_file = fopen(name, "wb");
    } else if (encode_param.encode_format == VENC_CODEC_H264) {
        sprintf(name, "%s.h264", out_path);
        out_file = fopen(name, "ab");
    }

    if (out_file == NULL) {
        CameraLogE("open file failed : %s", name);
        return -1;
    }

    fwrite(pOutputBuffer->pData0, 1, pOutputBuffer->nSize0, out_file);

    if (pOutputBuffer->nSize1) {
        fwrite(pOutputBuffer->pData1, 1, pOutputBuffer->nSize1, out_file);
    }

    if (pOutputBuffer->nSize2) {
        fwrite(pOutputBuffer->pData2, 1, pOutputBuffer->nSize2, out_file);
    }

    fclose(out_file);
    return 0;
}
#endif

int RecordCamera::encode(V4L2BUF_t *pdata, char *outbuf, int *outsize, void *user) {
    CameraLogV("%s %d", __FUNCTION__, __LINE__);
    CdxMuxerPacketT *ppkt = (CdxMuxerPacketT *)outbuf;
    VencInputBuffer inputBuffer;
    memset(&inputBuffer, 0, sizeof(VencInputBuffer));
    int ret = 0;
    V4L2BUF_t *pbuf = (V4L2BUF_t *)pdata;

    VencOutputBuffer outputBuffer;
    memset(&outputBuffer, 0, sizeof(VencOutputBuffer));

#if ENC_USE_CAMERA_PHY_ADDR
    inputBuffer.nID = 0;
    inputBuffer.pAddrPhyY = (unsigned char *)pbuf->addrPhyY;
    inputBuffer.pAddrPhyC =
        (unsigned char *)pbuf->addrPhyY + encode_param.src_width * encode_param.src_height;
    inputBuffer.pAddrVirY = (unsigned char *)pbuf->addrVirY;
    inputBuffer.pAddrVirC =
        (unsigned char *)pbuf->addrVirY + encode_param.src_width * encode_param.src_height;

    inputBuffer.nPts = pbuf->timeStamp;
    // CameraLogV("encode -%p %d x %d-----
    // ",pbuf->addrPhyY,encode_param.src_width,encode_param.src_height);
#else
    GetOneAllocInputBuffer(pRecVideoEnc, &inputBuffer);
    memcpy(inputBuffer.pAddrVirY, (unsigned char *)pbuf->addrVirY,
           encode_param.src_width * encode_param.src_height);
    memcpy(inputBuffer.pAddrVirC,
           (unsigned char *)pbuf->addrVirY + encode_param.src_width * encode_param.src_height,
           encode_param.src_width * encode_param.src_height / 2);

    ret = FlushCacheAllocInputBuffer(pRecVideoEnc, &inputBuffer);
#if DUMP_YUV
    FILE *in_file = NULL;
    static int count = 0;
    if (count < DUMP_COUNT) {
        if (in_file == NULL) {
            CameraLogD("---------------input open---------------------");
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "/mnt/UDISK/yuv_%dx%d_%d.yuv", encode_param.src_width,
                    encode_param.src_height, count++);
            in_file = fopen(buffer, "wb");
            if (in_file == NULL) {
                CameraLogE("fopen fail!!!! %s", strerror(errno));
            }
        }

        fwrite(inputBuffer.pAddrVirY, 1, encode_param.src_width * encode_param.src_height, in_file);
        fwrite(inputBuffer.pAddrVirC, 1, encode_param.src_width * encode_param.src_height / 2,
               in_file);

        CameraLogD("---------------input close---------------------");
        fclose(in_file);
    }
#endif
#endif

    // CAMERA_LOG
    inputBuffer.bEnableCorp = 0;
    // inputBuffer.sCropInfo.nLeft =  240;
    // inputBuffer.sCropInfo.nTop  =  240;
    // inputBuffer.sCropInfo.nWidth  =  240;
    // inputBuffer.sCropInfo.nHeight =  240;
    if (encode_param.encode_frame_num % (mDuration * encode_param.frame_rate) == 0) {
        encode_param.pts = 0;  // encord everv a file duration ,reset the pts
    }
#if ENC_USE_CAMERA_PHY_ADDR

    // CameraLogD("--pts=%lld  --rate=%d ",encode_param.pts,encode_param.frame_rate);
    encode_param.pts += 1.0 / encode_param.frame_rate * 1000000;
    // do not use captureThread's timeStamp
    inputBuffer.nPts = systemTime() / 1000000;  // encode_param.pts;//p->nPts;
#if ENABLE_DEBUG_PTS_TIMEOUT
    long long cur_pts_time = inputBuffer.nPts;
    if (cur_pts_time - pts_base_time > PTS_TIMEOUT)
        CameraLogD("[test] ---- encode takes %lld ms, cur time:%lld, pre time:%lld",
                   cur_pts_time - pts_base_time, cur_pts_time, pts_base_time);
    pts_base_time = cur_pts_time;
#endif

#else
    FlushCacheAllocInputBuffer(pRecVideoEnc, &inputBuffer);
    encode_param.pts += 1.0 / encode_param.frame_rate * 1000000;
    inputBuffer.nPts = encode_param.pts;  // p->nPts;

    inputBuffer.nPts = systemTime();  // tag for encode done
#endif

    AddOneInputBuffer(pRecVideoEnc, &inputBuffer);
    VideoEncodeOneFrame(pRecVideoEnc);
    AlreadyUsedInputBuffer(pRecVideoEnc, &inputBuffer);

#if ENC_USE_CAMERA_PHY_ADDR
    // CameraLogD("enc release =%d cid=%d",p->nID,mCameraId);
    // inputBuffer.nPts=systemTime();
#else
    ReturnOneAllocInputBuffer(pRecVideoEnc, &inputBuffer);
#endif

    // CAMERA_LOG
    int result = GetOneBitstreamFrame(pRecVideoEnc, &outputBuffer);
    if (result == -1) {
        CameraLogE("GetOneBitstreamFramec failed in frame");
        return -1;
    }
    // CAMERA_LOG
    // CameraLogE("----send ikey -- %d---",outputBuffer.nFlag);
    if ((outputBuffer.nFlag & 1) == 1) {
        CameraLogD("encode ikey %d-camid=%d", outputBuffer.nFlag, mCameraId);
        ret = 1;
    }

#if DUMP_VIDEOSTREAM
    static int i = 0;
    if (i < DUMP_COUNT) {
        saveVideoStream(&outputBuffer, "/mnt/UDISK/test", i++, encode_param);
    }
#endif
    ppkt->buflen = outputBuffer.nSize0 + outputBuffer.nSize1;
    if ((ppkt->buf = (char *)malloc(ppkt->buflen)) == NULL) {
        CameraLogE("pkt.buf malloc failed in	 frame");
        return -1;
    }
    // CAMERA_LOG
    memcpy(ppkt->buf, outputBuffer.pData0, outputBuffer.nSize0);

    if (outputBuffer.nSize1 > 0) {
        memcpy((char *)ppkt->buf + outputBuffer.nSize0, outputBuffer.pData1, outputBuffer.nSize1);
    }

    if (ppkt->buflen > 250 * 1024) {
        CameraLogW("camid=%d p=%p,size0=%d size1=%d", mCameraId, ppkt->buf, outputBuffer.nSize0,
                   outputBuffer.nSize1);

        // CameraLogW("encode camid=%d p=%p,len=%d pk pts=%lld,inputpts=%lld,outpts=%lld size0=%d
        // size1=%d",
        // mCameraId,ppkt->buf,ppkt->buflen,ppkt->pts,inputBuffer.nPts,outputBuffer.nPts,outputBuffer.nSize0,outputBuffer.nSize1);
    }
    ppkt->pts = outputBuffer.nPts;
    ppkt->duration = 1.0 / encode_param.frame_rate * 1000;

    CameraLogV("-eee-pts=%lld --dur=%lld ", ppkt->pts, ppkt->duration);

    ppkt->type = CDX_MEDIA_VIDEO;
    ppkt->streamIndex = 0;

    encode_param.encode_frame_num++;  // encode a packet and send

    FreeOneBitStreamFrame(pRecVideoEnc, &outputBuffer);
    return ret;
}

int RecordCamera::start_rtsp() {
#if 0
    int ret = init_ringbuf(&rtspQbuf);
    if (ret != 0) {
        CameraLogE("start_rtsp init ringbuf fail");
        return -1;
    }
    if (istestrtsp)
        out_filertsp = fopen("/mnt/UDISK/480p.h264", "wb");

    rtspserver_start(this);
    rtsp_started = 1;
#else
    CameraLogD("%s is not available", __FUNCTION__);
#endif
    return 0;
}

int RecordCamera::stop_rtsp() {
#if 0
    CAMERA_LOG;
    stop_deliverrtsp();
    CAMERA_LOG;
    rtspserver_stop();
    CAMERA_LOG;

    destory_ringbuf(&rtspQbuf);
    rtsp_started = 0;
#else
    CameraLogD("%s is not available", __FUNCTION__);
#endif
    return 0;
}
int RecordCamera::start_deliverrtsp(int disp_width, int disp_height) {
    // request a encoder
    // create_rtsp_encoder(1 * 1024 *
    // 1024,encode_param.src_width,encode_param.src_height,disp_width,disp_height,encode_param.frame_rate);
    rtspQbuf.write_id = 0;
    rtspQbuf.read_id = 0;
    rtspQbuf.buf_unused = ENC_RINGBUF_LEVEL;

    // start deliver
    can_deliver_rtspdata = 1;
    return 0;
}
int RecordCamera::stop_deliverrtsp() {
    // stop deliver
    can_deliver_rtspdata = 0;
    //
    destory_rtsp_encoder();
    if (istestrtsp) {
        fclose(out_filertsp);
        out_filertsp = NULL;
    }
    return 0;
}

/**************************************
Function:
Description:
***************************************/
int RecordCamera::encode_rtsp(V4L2BUF_t *pdata, char *outbuf, int *outsize, void *user,
                              int freebuf) {
    Mutex::Autolock locker(&mRtspLock);
    if (can_deliver_rtspdata != 1)  // this no sync,so re check
        return 0;

    //#ifdef USE_CDX_LOWLEVEL_API
    CdxMuxerPacketT *ppkt =
        (CdxMuxerPacketT *)&rtspQbuf.bufhead[rtspQbuf.write_id];  // outbuf;//so bad~!!!
    VencInputBuffer inputBuffer;
    VencOutputBuffer outputBuffer;
    char *ptp = (char *)pdata;
    VencInputBuffer *p = (VencInputBuffer *)(ptp + 4);

    GetOneAllocInputBuffer(pVideoEncRtsp, &inputBuffer);

    //  2!cedarx lib problem.so must copy this v4lbuf to inputbuffer
    memcpy(inputBuffer.pAddrVirY, (unsigned char *)p->pAddrVirY,
           encode_param.src_width * encode_param.src_height);
    memcpy(inputBuffer.pAddrVirC,
           (unsigned char *)p->pAddrVirY + encode_param.src_width * encode_param.src_height,
           encode_param.src_width * encode_param.src_height / 2);

#if 0
    if (freebuf) {
        mHardwareCameras->releaseRecordingFrame((const char*)pdata);;
    }
#endif
    inputBuffer.bEnableCorp = 0;
    inputBuffer.sCropInfo.nLeft = 240;
    inputBuffer.sCropInfo.nTop = 240;
    inputBuffer.sCropInfo.nWidth = 240;
    inputBuffer.sCropInfo.nHeight = 240;

    FlushCacheAllocInputBuffer(pVideoEncRtsp, &inputBuffer);

    rtsp_encparm.pts += 1.0 / encode_param.frame_rate * 1000000;

    // inputBuffer.nPts = p->timeStamp;
    inputBuffer.nPts = rtsp_encparm.pts;  // p->nPts;

    AddOneInputBuffer(pVideoEncRtsp, &inputBuffer);

    // time1 = GetNowUs();
    VideoEncodeOneFrame(pVideoEncRtsp);

    // time2 = GetNowUs();
    // CameraLogV("encode frame %d use time is %lldus..",testNumber,(time2-time1));
    // time3 += time2-time1;

    AlreadyUsedInputBuffer(pVideoEncRtsp, &inputBuffer);

    ReturnOneAllocInputBuffer(pVideoEncRtsp, &inputBuffer);

    int result = GetOneBitstreamFrame(pVideoEncRtsp, &outputBuffer);
    if (result == -1) {
        CameraLogE("GetOneBitstreamFramec failed in	 frame");
        return -1;
    }

    memcpy(ppkt->buf, outputBuffer.pData0, outputBuffer.nSize0);

    if (outputBuffer.nSize1 > 0) {
        memcpy((char *)ppkt->buf + outputBuffer.nSize0, outputBuffer.pData1, outputBuffer.nSize1);
    }

    ppkt->buflen = outputBuffer.nSize0 + outputBuffer.nSize1;

    ppkt->pts = outputBuffer.nPts;
    ppkt->duration = 1.0 / encode_param.frame_rate * 1000;
    ppkt->type = 0;
    ppkt->streamIndex = 0;

    push_ringbuf(&rtspQbuf);

    FreeOneBitStreamFrame(pVideoEncRtsp, &outputBuffer);
    if (istestrtsp) fwrite(ppkt->buf, 1, ppkt->buflen, out_filertsp);

    return 0;
}

/**************************************
Function:
Description:
***************************************/
int RecordCamera::create_rtsp_encoder(int bitrate, int in_width, int in_height, int out_width,
                                      int out_height, int framerate) {
#if 1
    CameraLogD("%s is not available", __FUNCTION__);
#else
    Mutex::Autolock locker(&mRtspLock);

    rtsp_encparm.bitrate = bitrate;
    rtsp_encparm.in_width = in_width;
    rtsp_encparm.in_height = in_height;

    rtsp_encparm.out_width = out_width;
    rtsp_encparm.out_height = out_height;
    rtsp_encparm.framerate = framerate;
    rtsp_encparm.eInputFormat = VENC_PIXEL_YVU420SP;  // fucking hard code
    rtsp_encparm.pts = 0;

#if USE_CDX_LOWLEVEL_API

    /********************** Define H264 Paramerter *************************/
    VencH264Param h264Param;
    /********************** Define H264 Paramerter *************************/

    memset(&rtsp_enc_baseConfig, 0, sizeof(VencBaseConfig));
    rtsp_enc_baseConfig.nInputWidth = in_width;
    rtsp_enc_baseConfig.nInputHeight = in_height;
    rtsp_enc_baseConfig.nStride = in_width;

    rtsp_enc_baseConfig.nDstWidth = out_width;
    rtsp_enc_baseConfig.nDstHeight = out_height;
    rtsp_enc_baseConfig.eInputFormat = rtsp_encparm.eInputFormat;  // VENC_PIXEL_YVU420SP;

    paramStruct_t rtsp_encoder_Memops;
    memset(&rtsp_encoder_Memops, 0, sizeof(paramStruct_t));

    CameraLogD("create_rtsp_encoder:%d ", __LINE__);
    usleep(200 * 1000);
    int ret = allocOpen(MEM_TYPE_CDX_NEW, &rtsp_encoder_Memops, NULL);
    if (ret < 0) {
        CameraLogE("create_rtsp_encoder:allocOpen failed");
    }
    rtsp_enc_baseConfig.memops = (struct ScMemOpsS *)rtsp_encoder_Memops.ops;

    /******************************* Set H264 Parameters ****************************/
    h264Param.bEntropyCodingCABAC = 1;
    h264Param.nBitrate = bitrate;
    h264Param.nFramerate = framerate;
    h264Param.nCodingMode = VENC_FRAME_CODING;
    h264Param.nMaxKeyInterval = framerate;
    h264Param.sProfileLevel.nProfile = VENC_H264ProfileMain;
    h264Param.sProfileLevel.nLevel = VENC_H264Level31;
    h264Param.sQPRange.nMinqp = 10;
    h264Param.sQPRange.nMaxqp = 50;
    /******************************* Set H264 Parameters ****************************/

    VencAllocateBufferParam bufferParam;
    memset(&bufferParam, 0, sizeof(VencAllocateBufferParam));
    bufferParam.nSizeY = in_width * in_height;
    bufferParam.nSizeC = in_width * in_height / 2;
    bufferParam.nBufferNum = 4;

    pVideoEncRtsp = VideoEncCreate((VENC_CODEC_TYPE)VENC_CODEC_H264);

    VideoEncSetParameter(pVideoEncRtsp, VENC_IndexParamH264Param, &h264Param);
    int vbvSize = 2 * 1024 * 1024;
    VideoEncSetParameter(pVideoEncRtsp, VENC_IndexParamSetVbvSize, &vbvSize);

    VideoEncInit(pVideoEncRtsp, &rtsp_enc_baseConfig);

    AllocInputBuffer(pVideoEncRtsp, &bufferParam);

    VideoEncGetParameter(pVideoEncRtsp, VENC_IndexParamH264SPSPPS,
                         &rtsp_sps_pps);  // todo: rtsp need it ???
    if (istestrtsp) fwrite(rtsp_sps_pps.pBuffer, 1, rtsp_sps_pps.nLength, out_filertsp);

#endif

#endif

    return 0;
}

int RecordCamera::destory_rtsp_encoder() {
    Mutex::Autolock locker(&mRtspLock);
    CAMERA_LOG;
#if USE_CDX_LOWLEVEL_API
    int result = VideoEncUnInit(pVideoEncRtsp);
    if (result) {
        CameraLogE("VideoEncUnInit error result=%d...", result);
        return -1;
    }
    CAMERA_LOG;
    VideoEncDestroy(pVideoEncRtsp);
#endif
    pVideoEncRtsp = NULL;
    if (rtsp_enc_baseConfig.memops) {
        // CdcMemClose(rtsp_enc_baseConfig.memops);
        paramStruct_t rtsp_encoder_Memops;
        memset(&rtsp_encoder_Memops, 0, sizeof(paramStruct_t));
        rtsp_encoder_Memops.ops = rtsp_enc_baseConfig.memops;
        allocClose(MEM_TYPE_CDX_NEW, &rtsp_encoder_Memops, NULL);
        rtsp_enc_baseConfig.memops = NULL;
    }
    return 0;
}

/**************************************
Function:
Description:
***************************************/
void RecordCamera::dropQueue() {
    int qcnt = OSAL_GetElemNum(&mQueueBufferRecordFile);
    CameraLogW("ff record RECORD_STATE_PAUSED cnt %d sleep...", qcnt);
    media_msg_t *pmsg = NULL;
    if (qcnt > 0) {
        while (qcnt > 0) {
            pmsg = (media_msg_t *)OSAL_Dequeue(&mQueueBufferRecordFile);

            if (pmsg != NULL) {
                CdxMuxerPacketT *eppkt = (CdxMuxerPacketT *)&pmsg->mesg_data[0];

                if (eppkt != NULL) free(eppkt->buf);

                free(pmsg);
            }
            qcnt--;
        }
    }
}

bool RecordCamera::recordThread() {
    int ret;
    int need_new = 0;
    int qcnt;

    media_msg_t amsg;
    media_msg_t *pmsg = &amsg;
    // V4L2BUF_t *p;

    if (recordStat == RECORD_STATE_PAUSED) {
        CameraLogW("ff record RECORD_STATE_PAUSED %d", __LINE__);
        mNeedNewFile = 0;
        closemux();
        sync();  // a long time maybe 300ms above
        mAudioflag = 0;
        if (((mCurRecordFileNameType == CurRecordFileNameType_LOCK) ||
             (mCurRecordFileNameType == CurRecordFileNameType_SAVE))) {
            camera_change_file_name(mCurRecordFileNameType);
            mCurRecordFileNameType = CurRecordFileNameType_NORMAL;
        }

        dropQueue();

        pthread_mutex_lock(&mRecordFileMutex);
        CameraLogW("in RECORD_STATE_PAUSED write for singal %d", recordStat);
        if (mNeedExit) {
            CameraLogW("in RECORD_STATE_PAUSED flash need exit stat =%d", mNeedExit);
            pthread_mutex_unlock(&mRecordFileMutex);
            return false;
        }
        pthread_cond_wait(&mRecordFileCond, &mRecordFileMutex);
        pthread_mutex_unlock(&mRecordFileMutex);
        CameraLogW("in RECORD_STATE_PAUSED recv a msg then stat =%d", recordStat);
        return true;
    }

repeat:

    pmsg = (media_msg_t *)OSAL_Dequeue(&mQueueBufferRecordFile);
    if (pmsg == NULL) {
        qcnt = OSAL_GetElemNum(&mQueueBufferRecordFile);
        if (qcnt > 10) goto repeat;
        // CameraLogV("record queue no buffer, sleep.cnt=%d..id=%d",qcnt,mCameraId);
        pthread_mutex_lock(&mRecordFileMutex);
        pthread_cond_wait(&mRecordFileCond, &mRecordFileMutex);
        pthread_mutex_unlock(&mRecordFileMutex);

        return true;
    }

    if (pmsg->mesg_type == RECORD_CMD_TIMEOUT) {
        CameraLogE("FOUND TIME OUT IN CREATE NEW FILE");
        FileNewStampBase = pmsg->timestamp;
        need_new = 1;
        mNeedNewFile = 0;
        // goto XANEWFILE;
    } else {
        CdxMuxerPacketT *perrppkt = (CdxMuxerPacketT *)&pmsg->mesg_data[0];
        if (mNeedNewFile == 1) {
            if ((perrppkt->type != CDX_MEDIA_AUDIO) && (pmsg->iskeyframe == 1)) {
                FileNewStampBase = pmsg->timestamp;
                need_new = 1;
                mNeedNewFile = 0;

                CameraLogW("-------------new file base is %lld", FileNewStampBase);
            } else {
                // think no video conditon
                if (perrppkt->type == CDX_MEDIA_AUDIO) {
                    mDvrWriteFrameNum++;
                    if (mDvrWriteFrameNum > 60) {
                        FileNewStampBase = pmsg->timestamp;
                        need_new = 1;
                        mNeedNewFile = 0;
                        mDvrWriteFrameNum = 0;
                        CameraLogW("--------no video-----new file base is %lld", FileNewStampBase);
                    }
                }
            }
        }

        // if(perrppkt->type!=CDX_MEDIA_AUDIO){
        perrppkt->pts = pmsg->timestamp - FileNewStampBase;
#if ENABLE_DEBUG_PTS_TIMEOUT
        if (perrppkt->pts - base_pts > PTS_TIMEOUT) {
            CameraLogD("[test] >>>>>>>>>>> warning::: Pts interval:%d, cur pts:%lld, pre pts:%lld",
                       (perrppkt->pts - base_pts), perrppkt->pts, base_pts);
        }
        base_pts = perrppkt->pts;
#endif
    }

    if (need_new) {
        static int cnttt = 0;
        static long long timeends = 0;

        long long timestart = GetNowUs();

        // dumy reset 0
        need_new = 0;
        closemux();
        // mAudioflag = 0;
        if (((mCurRecordFileNameType == CurRecordFileNameType_LOCK) ||
             (mCurRecordFileNameType == CurRecordFileNameType_SAVE)) &&
            (pmsg->iskeyframe)) {
            camera_change_file_name(mCurRecordFileNameType);
            mCurRecordFileNameType = CurRecordFileNameType_NORMAL;
        } else {
            CameraLogW("filetype=%d iskey=%d camera id=%d", mCurRecordFileNameType,
                       pmsg->iskeyframe, mCameraId);
            if (((mCurRecordFileNameType == CurRecordFileNameType_LOCK) ||
                 (mCurRecordFileNameType == CurRecordFileNameType_SAVE))) {
                camera_change_file_name(mCurRecordFileNameType);
            }
            mCurRecordFileNameType = CurRecordFileNameType_NORMAL;
        }

        if (recordStat == RECORD_STATE_PAUSED) {
            CameraLogV("record RECORD_STATE_PAUSED %d", __LINE__);
            mNeedNewFile = 0;
            dropQueue();

            pthread_mutex_lock(&mRecordFileMutex);
            pthread_cond_wait(&mRecordFileCond, &mRecordFileMutex);
            pthread_mutex_unlock(&mRecordFileMutex);
            CameraLogV("record RECORD_STATE_PAUSED record", qcnt);
            return true;
        }
        ret = prepare();

        if (ret < 0) {
            if (mTimeridVaild == 1) {
                stopTimer(mTimerID);
                mCurRecordPos = 0;
            }
            __notify_cb(CAMERA_MSG_DVR_STORE_ERR, 0, 0, mCbUser);
            goto FREEMEM;
        } else {
            mNeedThumbpic = 1;
        }
        long long timeend = GetNowUs();
        cnttt++;
        timeends += timeend - timestart;
        CameraLogV("record us time %lld us avl=%lld", timeend - timestart, timeends / cnttt);
    }
    if (pmsg->mesg_type == RECORD_CMD_TIMEOUT) goto FREEMEM1;
        // recordxxxxxxxx

#if USE_CDX_LOWLEVEL_API
    ret = awrecordwrite(pmsg);  //(pbuf->timestamp,p);//(char*)p->addrVirY,(char*)p->addrPhyY);
    if (ret < 0) {
        CameraLogE("awrecordwrite file notify user");
        if (mTimeridVaild == 1) {
            stopTimer(mTimerID);
            mCurRecordPos = 0;
        }
        __notify_cb(CAMERA_MSG_DVR_STORE_ERR, 0, 0, mCbUser);
    }

FREEMEM : {
    CdxMuxerPacketT *ppkt = (CdxMuxerPacketT *)&pmsg->mesg_data[0];
    if (ppkt->buf != NULL) free(ppkt->buf);
    ppkt->buf = NULL;
}
#endif
FREEMEM1:

    if (pmsg != NULL) free(pmsg);
#if ENABLE_DEBUG_PTS_TIMEOUT
    long long save_frame_time = systemTime() / 1000000;
    if (save_frame_time - frame_base_time > SAVE_FRAME_TIMEOUT) {
        CameraLogD(
            "[test] ---- record file takes %lld ms, cur_time:%lld, pre_time:%lld, queue_num:%d",
            save_frame_time - frame_base_time, save_frame_time, frame_base_time,
            OSAL_GetElemNum(&mQueueBufferRecordFile));
    }
    frame_base_time = save_frame_time;
#endif
    return true;
}

/**************************************
Function:
Description:
***************************************/
int RecordCamera::awrecordwrite(
    media_msg_t *pmsg)  //(nsecs_t timestamp,V4L2BUF_t *p)//char* dataPtr,char *phyY)
{
    CdxMuxerPacketT *ppkt = (CdxMuxerPacketT *)&pmsg->mesg_data[0];
    // CameraLogW("awrecordwritecameraid=%d type=%d pts=%lld dur=%d len=%d
    // success",mCameraId,ppkt->type,ppkt->pts,ppkt->duration,ppkt->buflen);

    if (mux == NULL) {
        // CameraLogE("CdxMuxerWritePacket failddddd-  id=%d",mCameraId);
        return 0;
    }

    int result = CdxMuxerWritePacket(mux, ppkt);
    // fwrite(ppkt->buf, 1, ppkt->buflen, out_file);
    if (result < 0) {
        // CameraLogE("CdxMuxerWritePacket() failed in  frame");
        CameraLogE("CdxMuxerWritePacket fail id=%d", mCameraId);
    } else {
        // CameraLogV("CdxMuxerWritePacket ok id=%d",mCameraId);
    }

    return result;
}

/**************************************
Function:
Description:
***************************************/
int RecordCamera::setVideoEncParm() {
#if USE_CDX_LOWLEVEL_API
    CameraLogD("%s %d", __FUNCTION__, __LINE__);

    paramStruct_t setparmMemops;
    memset(&setparmMemops, 0, sizeof(paramStruct_t));

    CameraLogD("libsdk_dvr version:%s, setVideoEncParm:%d", MODULE_VERSION, __LINE__);
    usleep(200 * 1000);
    int ret = allocOpen(MEM_TYPE_CDX_NEW, &setparmMemops, NULL);
    if (ret < 0) {
        CameraLogE("setparm:allocOpen failed");
    }
    rec_enc_baseConfig.memops = (struct ScMemOpsS *)setparmMemops.ops;

    rec_enc_baseConfig.nInputWidth = encode_param.src_width;
    rec_enc_baseConfig.nInputHeight = encode_param.src_height;
    rec_enc_baseConfig.nStride = encode_param.src_width;

    rec_enc_baseConfig.nDstWidth = encode_param.dst_width;
    rec_enc_baseConfig.nDstHeight = encode_param.dst_height;
    rec_enc_baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;

    VencAllocateBufferParam bufferParam;
    memset(&bufferParam, 0, sizeof(VencAllocateBufferParam));
    bufferParam.nSizeY = encode_param.src_width * ALIGN_16B(encode_param.src_height);
    bufferParam.nSizeC = encode_param.src_width * ALIGN_16B(encode_param.src_height) / 2;
    bufferParam.nBufferNum = 4;
    CameraLogD("nSizeY %d nSizeC %d wxd(%dx%d)", bufferParam.nSizeY, bufferParam.nSizeC,
               encode_param.src_width, encode_param.src_height);
    pRecVideoEnc = VideoEncCreate((VENC_CODEC_TYPE)encode_param.encode_format);
    if (encode_param.encode_format == VENC_CODEC_JPEG) {
        /********************** Define JPEG Paramerter *************************/
        EXIFInfo exifinfo;
        int quality = 20;
        int jpeg_mode = 1;
        /********************** Define JPEG Paramerter *************************/

        /******************************* Set JPEG Parameters ****************************/
        exifinfo.ThumbWidth = 176;
        exifinfo.ThumbHeight = 144;
        strcpy((char *)exifinfo.CameraMake, "allwinner make test");
        strcpy((char *)exifinfo.CameraModel, "allwinner model test");
        strcpy((char *)exifinfo.DateTime, "2014:02:21 10:54:05");
        strcpy((char *)exifinfo.gpsProcessingMethod, "allwinner gps");
        exifinfo.Orientation = 0;
        exifinfo.ExposureTime.num = 2;
        exifinfo.ExposureTime.den = 1000;
        exifinfo.FNumber.num = 20;
        exifinfo.FNumber.den = 10;
        exifinfo.ISOSpeed = 50;
        exifinfo.ExposureBiasValue.num = -4;
        exifinfo.ExposureBiasValue.den = 1;
        exifinfo.MeteringMode = 1;
        exifinfo.FlashUsed = 0;
        exifinfo.FocalLength.num = 1400;
        exifinfo.FocalLength.den = 100;
        exifinfo.DigitalZoomRatio.num = 4;
        exifinfo.DigitalZoomRatio.den = 1;
        exifinfo.WhiteBalance = 1;
        exifinfo.ExposureMode = 1;
        exifinfo.enableGpsInfo = 1;
        exifinfo.gps_latitude = 23.2368;
        exifinfo.gps_longitude = 24.3244;
        exifinfo.gps_altitude = 1234.5;
        exifinfo.gps_timestamp = (long)time(NULL);
        strcpy((char *)exifinfo.CameraSerialNum, "123456789");
        strcpy((char *)exifinfo.ImageName, "exif-name-test");
        strcpy((char *)exifinfo.ImageDescription, "exif-descriptor-test");
        /******************************* Set JPEG Parameters ****************************/
        CameraLogD("create Jpegencoder!");
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamJpegExifInfo, &exifinfo);
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamJpegQuality, &quality);
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamJpegEncMode, &jpeg_mode);
    } else if (encode_param.encode_format == VENC_CODEC_H264) {
        /********************** Define H264 Paramerter *************************/
        // VencHeaderData sps_pps_data;
        VencH264Param h264Param;
        memset(&h264Param, 0, sizeof(VencH264Param));
        /******************************* Set H264 Parameters ****************************/
        h264Param.bEntropyCodingCABAC = 1;
        h264Param.nBitrate = encode_param.bit_rate;
        h264Param.nFramerate = encode_param.frame_rate;
        h264Param.nCodingMode = VENC_FRAME_CODING;
        h264Param.nMaxKeyInterval = encode_param.maxKeyFrame;
        h264Param.sProfileLevel.nProfile = VENC_H264ProfileHigh;
        h264Param.sProfileLevel.nLevel = VENC_H264Level51;
        h264Param.sQPRange.nMinqp = 10;
        h264Param.sQPRange.nMaxqp = 50;  // 40-->50  fix some bug for sensor in light to black
        /******************************* Set H264 Parameters ****************************/

        int value;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamH264Param, &h264Param);

        CameraLogD(
            "--CID =%d\n"
            "h264Param.bEntropyCodingCABAC=%d\n"
            "h264Param.nBitrate=%d\n"
            "h264Param.nFramerate=%d\n"
            "h264Param.nCodingMode=%d\n"
            "h264Param.nMaxKeyInterval =%d\n"
            "h264Param.sProfileLevel.nProfile=%d\n"
            "h264Param.sProfileLevel.nLevel=%d\n"
            "h264Param.sQPRange.nMinqp=%d\n"
            "h264Param.sQPRange.nMaxqp=%d",
            mCameraId, h264Param.bEntropyCodingCABAC, h264Param.nBitrate, h264Param.nFramerate,
            h264Param.nCodingMode, h264Param.nMaxKeyInterval, h264Param.sProfileLevel.nProfile,
            h264Param.sProfileLevel.nLevel, h264Param.sQPRange.nMinqp, h264Param.sQPRange.nMaxqp);

        value = 0;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamIfilter, &value);

        value = 0;  // degree
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamRotation, &value);

        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamH264FixQP, &fixQP);

        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamH264CyclicIntraRefresh,
        // &sIntraRefresh);

        // value = 720/4;
        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamSliceHeight, &value);

        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamROIConfig, &sRoiConfig[0]);
        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamROIConfig, &sRoiConfig[1]);
        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamROIConfig, &sRoiConfig[2]);
        // VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamROIConfig, &sRoiConfig[3]);
        value = 0;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamSetPSkip, &value);
        sps_pps_flag = 0;
        // memset(&h264Param, 0, sizeof(h264Param));
        // VideoEncGetParameter(pRecVideoEnc, VENC_IndexParamH264Param, &h264Param);
        // CameraLogD(
        //     "--CID =%d h264Param.bEntropyCodingCABAC=%d\n"
        //     "h264Param.nBitrate=%d\n"
        //     "h264Param.nFramerate=%d\n"
        //     "h264Param.nCodingMode=%d\n"
        //     "h264Param.nMaxKeyInterval =%d\n"
        //     "h264Param.sProfileLevel.nProfile=%d\n"
        //     "h264Param.sProfileLevel.nLevel=%d\n"
        //     "h264Param.sQPRange.nMinqp=%d\n"
        //     "h264Param.sQPRange.nMaxqp=%d",
        //     mCameraId, h264Param.bEntropyCodingCABAC, h264Param.nBitrate, h264Param.nFramerate,
        //     h264Param.nCodingMode, h264Param.nMaxKeyInterval, h264Param.sProfileLevel.nProfile,
        //     h264Param.sProfileLevel.nLevel, h264Param.sQPRange.nMinqp,
        //     h264Param.sQPRange.nMaxqp);
        //  int vbvSize = 4*1024*1024;
        //  VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamSetVbvSize, &vbvSize);
    } else if (encode_param.encode_format == VENC_CODEC_H265) {
        unsigned int vbv_size = 12 * 1024 * 1024;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);

        VencH265Param h265Param;
        memset(&h265Param, 0, sizeof(h265Param));
        h265Param.nBitrate = encode_param.bit_rate;
        h265Param.nFramerate = encode_param.frame_rate;
        h265Param.sProfileLevel.nProfile = VENC_H265ProfileMain;
        h265Param.sProfileLevel.nLevel = VENC_H265Level41;
        h265Param.sQPRange.nMaxqp = 52;
        h265Param.sQPRange.nMinqp = 10;
        h265Param.nQPInit = 30;
        h265Param.idr_period = 30;
        h265Param.nGopSize = h265Param.idr_period;
        h265Param.nIntraPeriod = h265Param.idr_period;
        // h265Param.bLongTermRef = 1;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamH265Param, &h265Param);

        unsigned int value = 1;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamChannelNum, &value);

        int maxKeyFrame = encode_param.maxKeyFrame;
        VideoEncSetParameter(pRecVideoEnc, VENC_IndexParamMaxKeyInterval, &maxKeyFrame);
    } else {
        CameraLogE("This VENC_CODEC_TYPE has not been set:%d", encode_param.encode_format);
    }
    CameraLogD("Init encoder!");
    VideoEncInit(pRecVideoEnc, &rec_enc_baseConfig);
    CameraLogD("alloc encoder inputbuffer!");
    AllocInputBuffer(pRecVideoEnc, &bufferParam);
#endif
    return 0;
}

int JpegEnc(void *pBufOut, int *bufSize, JPEG_ENC_t *jpeg_enc) {
    CameraLogV("%s %d pBufOut %p", __FUNCTION__, __LINE__, pBufOut);
    VencBaseConfig baseConfig;
    VencAllocateBufferParam bufferParam;
    VideoEncoder *pVideoEnc = NULL;
    VencInputBuffer inputBuffer;
    VencOutputBuffer outputBuffer;
    int result = 0;
    int result1 = 0;
    long long pts = 0;
    long long time1 = 0;
    long long time2 = 0;
    long long time3 = 0;

    /********************** Define JPEG Paramerter *************************/
    EXIFInfo exifinfo;
    int quality = jpeg_enc->quality;
    int jpeg_mode = 1;
    /********************** Define JPEG Paramerter *************************/

    memset(&baseConfig, 0, sizeof(VencBaseConfig));
    memset(&bufferParam, 0, sizeof(VencAllocateBufferParam));

    baseConfig.nInputWidth = jpeg_enc->src_w;   // encode_param.src_width;
    baseConfig.nInputHeight = jpeg_enc->src_h;  // encode_param.src_height;
    baseConfig.nStride = jpeg_enc->src_w;       // encode_param.src_width;

    baseConfig.nDstWidth = jpeg_enc->pic_w;   // encode_param.dst_width;
    baseConfig.nDstHeight = jpeg_enc->pic_h;  // encode_param.dst_height;

    baseConfig.eInputFormat = (jpeg_enc->colorFormat == JPEG_COLOR_YUV420_NV21)
                                  ? VENC_PIXEL_YVU420SP
                                  : VENC_PIXEL_YUV420SP;

    paramStruct_t jpegMemops;
    memset(&jpegMemops, 0, sizeof(paramStruct_t));

    int ret = allocOpen(MEM_TYPE_CDX_NEW, &jpegMemops, NULL);
    if (ret < 0) {
        CameraLogE("JpegEnc:allocOpen failed");
    }
    baseConfig.memops = (struct ScMemOpsS *)jpegMemops.ops;

    bufferParam.nSizeY = ALIGN_32B(baseConfig.nInputWidth) * ALIGN_32B(baseConfig.nInputHeight);
    bufferParam.nSizeC = ALIGN_32B(baseConfig.nInputWidth) * ALIGN_32B(baseConfig.nInputHeight) / 2;
    bufferParam.nBufferNum = 1;
    CameraLogD("nSizeY %d nSizeC %d", bufferParam.nSizeY, bufferParam.nSizeC);

    /******************************* Set JPEG Parameters ****************************/
    exifinfo.ThumbWidth = jpeg_enc->thumbWidth;    // 176;
    exifinfo.ThumbHeight = jpeg_enc->thumbHeight;  // 144;
    strcpy((char *)exifinfo.CameraMake, jpeg_enc->CameraMake);
    strcpy((char *)exifinfo.CameraModel, jpeg_enc->CameraModel);
    strcpy((char *)exifinfo.DateTime, jpeg_enc->DateTime);
    strcpy((char *)exifinfo.gpsProcessingMethod, jpeg_enc->gps_processing_method);
    exifinfo.Orientation = 0;
    exifinfo.ExposureTime.num = 2;
    exifinfo.ExposureTime.den = 1000;
    exifinfo.FNumber.num = 20;
    exifinfo.FNumber.den = 10;
    exifinfo.ISOSpeed = 50;
    exifinfo.ExposureBiasValue.num = -4;
    exifinfo.ExposureBiasValue.den = 1;
    exifinfo.MeteringMode = 1;
    exifinfo.FlashUsed = 0;
    exifinfo.FocalLength.num = 1400;
    exifinfo.FocalLength.den = 100;
    exifinfo.DigitalZoomRatio.num = 4;
    exifinfo.DigitalZoomRatio.den = 1;
    exifinfo.WhiteBalance = 1;
    exifinfo.ExposureMode = 1;
    exifinfo.enableGpsInfo = jpeg_enc->enable_gps;
    exifinfo.gps_latitude = 23.2368;
    exifinfo.gps_longitude = 24.3244;
    exifinfo.gps_altitude = 1234.5;
    exifinfo.gps_timestamp = (long)time(NULL);
    strcpy((char *)exifinfo.CameraSerialNum, "123456789");
    strcpy((char *)exifinfo.ImageName, "exif-name-t2l");
    strcpy((char *)exifinfo.ImageDescription, "exif-descriptor-t2l");
    /******************************* Set JPEG Parameters ****************************/

    pVideoEnc = VideoEncCreate((VENC_CODEC_TYPE)VENC_CODEC_JPEG);
    if (pVideoEnc == NULL) {
        // if(memops) CdcMemClose(memops);
        if (jpegMemops.ops) allocClose(MEM_TYPE_CDX_NEW, &jpegMemops, NULL);
        CameraLogE("VideoEncCreate error ...");
        return -2;
    }
    result = VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegExifInfo, &exifinfo);
    result += VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &quality);
    result += VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegEncMode, &jpeg_mode);

    result += VideoEncInit(pVideoEnc, &baseConfig);
    if (result < 0) {
        CameraLogE("VideoEncInit error ...");
        goto out1;
    }

    result = AllocInputBuffer(pVideoEnc, &bufferParam);
    if (result < 0) {
        CameraLogE("AllocInputBuffer error ...");
        goto out;
    }

#if USE_SVC
    // used for throw frame test with SVC
    int TemporalLayer = -1;
    char p9bytes[9] = {0};
#endif
    result = GetOneAllocInputBuffer(pVideoEnc, &inputBuffer);
    if (result < 0) {
        CameraLogE("GetOneAllocInputBuffer error ...");
        goto out;
    }

    CameraLogD("jpeg_enc addrY %p addrC %p", jpeg_enc->addrY, jpeg_enc->addrC);
    CameraLogD("inputBuffer addrY %p addrC %p", inputBuffer.pAddrVirY, inputBuffer.pAddrVirC);

    CameraLogD("before memcpy Y (%dx%d)!!!", baseConfig.nInputWidth, baseConfig.nInputHeight);
    memcpy(inputBuffer.pAddrVirY, (const void *)jpeg_enc->addrY,
           baseConfig.nInputWidth * baseConfig.nInputHeight);
    CameraLogD("after memcpy Y!!!");
    memcpy(inputBuffer.pAddrVirC, (const void *)jpeg_enc->addrC,
           baseConfig.nInputWidth * baseConfig.nInputHeight / 2);
    CameraLogD("after memcpy C!!!");

    inputBuffer.bEnableCorp = jpeg_enc->enable_crop;
    inputBuffer.sCropInfo.nLeft = jpeg_enc->crop_x;
    inputBuffer.sCropInfo.nTop = jpeg_enc->crop_y;
    inputBuffer.sCropInfo.nWidth = jpeg_enc->crop_w;
    inputBuffer.sCropInfo.nHeight = jpeg_enc->crop_h;

    result = FlushCacheAllocInputBuffer(pVideoEnc, &inputBuffer);
    if (result < 0) {
        CameraLogE("FlushCacheAllocInputBuffer error ...");
        goto out;
    }

    inputBuffer.nPts = pts;

    AddOneInputBuffer(pVideoEnc, &inputBuffer);

    time1 = GetNowUs();
    CameraLogD("before VideoEncodeOneFrame");
    VideoEncodeOneFrame(pVideoEnc);
    CameraLogD("after VideoEncodeOneFrame");

    time2 = GetNowUs();
    CameraLogV("encode frame use time is %lldus..", (time2 - time1));
    time3 += time2 - time1;

    AlreadyUsedInputBuffer(pVideoEnc, &inputBuffer);

    ReturnOneAllocInputBuffer(pVideoEnc, &inputBuffer);

    result = GetOneBitstreamFrame(pVideoEnc, &outputBuffer);
    if (result < 0) {
        CameraLogE("GetOneBitstreamFrame error ...");
        goto out;
    }

    *bufSize = outputBuffer.nSize0 + outputBuffer.nSize1;
    memcpy(pBufOut, outputBuffer.pData0, outputBuffer.nSize0);

    if (outputBuffer.nSize1 > 0) {
        memcpy((char *)pBufOut + outputBuffer.nSize0, outputBuffer.pData1, outputBuffer.nSize1);
    }

    FreeOneBitStreamFrame(pVideoEnc, &outputBuffer);
    ReleaseAllocInputBuffer(pVideoEnc);
out:
    CameraLogD("before VideoEncUnInit %s %d", __FUNCTION__, __LINE__);
    result1 = VideoEncUnInit(pVideoEnc);
    if (result1) {
        CameraLogE("VideoEncUnInit error result=%d...", result1);
    }
out1:
    VideoEncDestroy(pVideoEnc);
    // if(memops) CdcMemClose(memops);
    if (jpegMemops.ops) allocClose(MEM_TYPE_CDX_NEW, &jpegMemops, NULL);
    pVideoEnc = NULL;
    return result;
}

int init_ringbuf(ringbufQ_t *rbuf) {
    int i;
    CdxMuxerPacketT *p;
    rbuf->write_id = 0;
    rbuf->read_id = 0;
    rbuf->buf_unused = ENC_RINGBUF_LEVEL;
    for (i = 0; i < ENC_RINGBUF_LEVEL; i++) {
        p = &rbuf->bufhead[i];
        p->buf = (void *)malloc(ENC_RINGBUF_DATALEN);
        if (p->buf == NULL) {
            CameraLogE("%s %d malloc fail init fail", __FUNCTION__, __LINE__);
            return -1;
        }
        p->buflen = 0;
    }
    return 0;
}

int destory_ringbuf(ringbufQ_t *rbuf) {
    int i;
    CdxMuxerPacketT *p;

    for (i = 0; i < ENC_RINGBUF_LEVEL; i++) {
        p = &rbuf->bufhead[i];
        free(p->buf);
    }

    return 0;
}

CdxMuxerPacketT *push_ringbuf(ringbufQ_t *rbuf) {
    int write_id = rbuf->write_id;
    int buf_unused = rbuf->buf_unused;

    if (buf_unused != 0) {
        // CameraLogE("push  unuse=%d wid=%d rid=%d",rbuf->buf_unused,rbuf->write_id,rbuf->read_id);
        rbuf->buf_unused--;
        rbuf->write_id++;
        rbuf->write_id %= ENC_RINGBUF_LEVEL;
    } else {
        CameraLogE("push_ringbuf underrun unuse=%d wid=%d rid=%d", rbuf->buf_unused, rbuf->write_id,
                   rbuf->read_id);
        rbuf->buf_unused = ENC_RINGBUF_LEVEL - 1;  // re push ,overwrite old data
        rbuf->write_id++;
        rbuf->write_id %= ENC_RINGBUF_LEVEL;
    }

    return &rbuf->bufhead[write_id];
}

CdxMuxerPacketT *pop_ringbuf(ringbufQ_t *rbuf) {
    int read_id = rbuf->read_id;
    int buf_unused = rbuf->buf_unused;

    if (buf_unused == ENC_RINGBUF_LEVEL) {
        CameraLogE("pop_ringbuf overrun");
        return 0;
    }
    if (rbuf->bufhead[read_id].buflen == 0) {
        CameraLogE("pop_ringbuf not ready");
        return 0;
    }
    // CameraLogE("pop unuse=%d wid=%d rid=%d",rbuf->buf_unused,rbuf->write_id,rbuf->read_id);

    rbuf->buf_unused++;
    rbuf->read_id++;
    rbuf->read_id %= ENC_RINGBUF_LEVEL;
    return &rbuf->bufhead[read_id];
}

/**************************************
end
***************************************/
