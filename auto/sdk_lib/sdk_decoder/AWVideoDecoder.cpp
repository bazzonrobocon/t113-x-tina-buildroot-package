
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <queue>

#include "vdecoder.h"
#include "memoryAdapter.h"

#include "AWVideoDecoder.h"
#include "sunxiMemInterface.h"

#define LOG_TAG "AWVideoDecoder"
#include "sdklog.h"

/*-------------------------------------------------------------------------
                                define
-------------------------------------------------------------------------*/
#define CHECK_RETURN(a) \
    do { \
         if(a!=0){\
           ALOGD("[%s] fail! line:%d ret:%d", __FUNCTION__, __LINE__,a);\
           return a; \
           }\
    } while (0)

typedef unsigned char     uint8;
typedef char              int8;

namespace awvideodecoder
{

static const int VE_FREQ = 300 * 1000 * 1000;
static const int NAL_DEF_SIZE = 200 * 1000;

static const int DISP_X_DEFAULT = 0;
static const int DISP_Y_DEFAULT = 0;
static const int DISP_W_DEFAULT = 1024;
static const int DISP_H_DEFAULT = 600;

static const int SCR_ID_DEFAULT = 0;
static const int CHL_ID_DEFAULT = 0;
static const int LYR_ID_DEFAULT = 0;
static const int ZORDER_DEFAULT = 2;

static const int DISP_F_DEFAULT = 0x4d;//DISP_FORMAT_YUV420_SP_VUVU;
static const int VIDEO_QUEUE_WARN_HOLD = 30 * 1;

class AWVideoDecoderImpl: public AWVideoDecoder
{
public:
    AWVideoDecoderImpl(DecodeParam* param);
    AWVideoDecoderImpl();
    ~AWVideoDecoderImpl();

    virtual int init(DecodeParam* param, AWVideoDecoderDataCallback* cbk);
    virtual int decode(const void* inputBuf, unsigned int inputLen,
                       void* outputBuf, unsigned int outputLen);
    virtual int decodeAsync(const void* inputBuf, unsigned int inputLen);
    virtual int requestPicture(AVPacket* picBuf);
    virtual int releasePicture(AVPacket* picBuf);
    virtual int decode(const AVPacket* inPacket) ;
    int stopAndGetLeftFream(void* outputBuf, unsigned int outputLen);

private:
    void finish();

    int writeOutputBuf(void* buf, int len);
    int readInputBuf(void* buf, int len);
    int writeCallback(void* buf, int len, void* buf1 = NULL, int len1 = 0);

    void saveNV21Picture(VideoPicture* pPicture, char* file);
    int getFrameBitSize(int w, int h, PixelFmt fmt);

    VConfig             mVideoConf;
    VideoStreamInfo     mStreamInfo;

    const void* mOutputBuf;
    int mOutputBufLen ;
    int mOutputBufPos;
    const void* mInputBuf;
    int mInputBufLen ;
    int mInputBufPos ;

    //struct ScMemOpsS*   memops;
    paramStruct_t mSetparmMemops;
    VideoDecoder* pVideoDec;
    VideoPicture* pPicture;
    VideoStreamDataInfo  dataInfo;
    int nValidSize;
    int nRequestDataSize;   //=one NALU frame size
    char* packet_buf;
    int packet_buflen;
    char* packet_ringBuf;
    int packet_ringBufLen;
    int64_t packet_pts;
    int64_t packet_pcr;
    DecodeParam* mParam;
    AWVideoDecoderDataCallback* mDataCbk = NULL;

};

/*-------------------------------------------------------------------------
                                AWVideoDecoder
-------------------------------------------------------------------------*/
AWVideoDecoder* AWVideoDecoder::create(DecodeParam* param)
{
    AWVideoDecoder* pDecoder = (AWVideoDecoder*) new AWVideoDecoderImpl(param);
    return pDecoder;
}

AWVideoDecoder* AWVideoDecoder::create()
{
    AWVideoDecoder* pDecoder = (AWVideoDecoder*) new AWVideoDecoderImpl();
    return pDecoder;
}

void AWVideoDecoder::destroy(AWVideoDecoder* decoder)
{
    delete decoder;
}

/*-------------------------------------------------------------------------
                                AWVideoDecoderImpl
-------------------------------------------------------------------------*/

int AWVideoDecoderImpl::getFrameBitSize(int w, int h, PixelFmt fmt)
{
    int pixelNum = w * h;
    //ALOGD("pic size:[%dx%d]",w,h);
    int frameBitSize = 0;
    switch (fmt) {
        case PIXEL_DEFAULT:
        case PIXEL_YUV_PLANER_420:
        case PIXEL_YUV_MB32_420:
        case PIXEL_YV12...PIXEL_NV12:
            frameBitSize = pixelNum * 3 / 2;
            break;

        case PIXEL_YUV_PLANER_422:
        case PIXEL_YUV_MB32_422:
        case PIXEL_PLANARUV_422:
        case PIXEL_PLANARVU_422:
        case PIXEL_YUYV...PIXEL_VYUY:
            frameBitSize = pixelNum * 2;
            break;

        case PIXEL_YUV_PLANER_444:
        case PIXEL_YUV_MB32_444:
        case PIXEL_PLANARUV_444:
        case PIXEL_PLANARVU_444:
            frameBitSize = pixelNum * 3;
            break;

        case PIXEL_ARGB...PIXEL_BGRA:
            frameBitSize = pixelNum * 4;
            break;
    }
    return frameBitSize;
}

AWVideoDecoderImpl::AWVideoDecoderImpl()
{
    ALOGD("create AWVideoDecoderImpl() version:%s", MODULE_VERSION);
    mOutputBuf = NULL;
    mOutputBufLen = 0;
    mOutputBufPos = 0;
    mInputBuf = NULL;
    mInputBufLen = 0;
    mInputBufPos = 0;
    pVideoDec = NULL;
    pPicture = NULL;
    nValidSize = 0;
    nRequestDataSize = 0;
    packet_buf = NULL;
    packet_buflen = 0;
    packet_ringBuf = NULL;
    packet_ringBufLen = 0;
    packet_pcr = 0;
    packet_pts = 0;
    mParam = NULL;
}

int AWVideoDecoderImpl::init(DecodeParam* param, AWVideoDecoderDataCallback* cbk)
{
    int ret = 0;
    pVideoDec = NULL;
    pPicture = NULL;

    memset((void*)&mVideoConf, 0, sizeof(VConfig));
    memset((void*)&mStreamInfo, 0, sizeof(VideoStreamInfo));

    memset(&mSetparmMemops, 0, sizeof(paramStruct_t));

    ret = allocOpen(MEM_TYPE_CDX_NEW, &mSetparmMemops, NULL);
    if (ret < 0) {
        ALOGE("setparm:allocOpen failed");
    }
    mVideoConf.memops = (struct ScMemOpsS*)mSetparmMemops.ops;
    //memops = MemAdapterGetOpsS();
    if (mVideoConf.memops == NULL) {
        ALOGE("memops is NULL");
        return -1;
    }
    //CdcMemOpen(memops);

    mStreamInfo.eCodecFormat = param->codecType;

    mVideoConf.eOutputPixelFormat  = param->pixelFormat;
    //mVideoConf.memops = memops;
    mVideoConf.nDisplayHoldingFrameBufferNum = 1;
    mVideoConf.nDecodeSmoothFrameBufferNum = 3;
    mVideoConf.nVeFreq = 300 * 1000 * 1000;   //300MH
    //mVideoConf.bIsFullFramePerPiece = 1;

    if (0 != param->scaleRatio) {
        mVideoConf.bScaleDownEn = 1;
        mVideoConf.nHorizonScaleDownRatio = param->scaleRatio;
        mVideoConf.nVerticalScaleDownRatio = param->scaleRatio;
    }
    if (0 != param->rotation) {
        mVideoConf.bRotationEn = 1;
        mVideoConf.nRotateDegree = param->rotation;
    }
    mParam = param;

    pVideoDec = CreateVideoDecoder();                                   //VdecH264:step1
    if (pVideoDec == NULL) {
        ALOGE(" decoder demom CreareateVideoDecoder() error ");
        return -3;
    }

    ret = InitializeVideoDecoder(pVideoDec, &mStreamInfo, &mVideoConf); //VdecH264:step2

    if (ret != 0) {
        ALOGE("decoder demom initialize video decoder fail.");
        DestroyVideoDecoder(pVideoDec);
        pVideoDec = NULL;
        return -4;
    }

    mDataCbk = cbk;
    return 0;
}

AWVideoDecoderImpl::AWVideoDecoderImpl(DecodeParam* param)
{
    init(param, NULL);
}

AWVideoDecoderImpl::~AWVideoDecoderImpl()
{
    ALOGD("~ AWVideoDecoderImpl()");
    finish();
}

void AWVideoDecoderImpl::finish()
{
    if (NULL != pVideoDec) {
        DestroyVideoDecoder(pVideoDec);
        pVideoDec = NULL;
    }
    if (mVideoConf.memops != NULL) {
        //CdcMemClose(baseConfig.memops);
        allocClose(MEM_TYPE_CDX_NEW, &mSetparmMemops, NULL);
        mVideoConf.memops = NULL;
    }
}

int AWVideoDecoderImpl::writeOutputBuf(void* buf, int len)
{
    if (mOutputBufLen < mOutputBufPos + len) {
        ALOGE("writeOutputBuf out of range[%d<%d]!!", mOutputBufLen, mOutputBufPos + len);
        return mOutputBufPos + len;
    }
    memcpy((char*)mOutputBuf + mOutputBufPos, buf, len);
    mOutputBufPos += len;
    return 0;
}

int AWVideoDecoderImpl::readInputBuf(void* buf, int len)
{
    if (mInputBufLen < mInputBufPos + len) {
        ALOGE("readInputBuf out of range!");
        return mInputBufPos + len;
    }

    memcpy(buf, (char*)mInputBuf + mInputBufPos, len);
    mInputBufPos += len;
    return 0;
}

void AWVideoDecoderImpl::saveNV21Picture(VideoPicture* pPicture, char* file)
{
    int nSizeY, nSizeUV;
    static FILE* fp = fopen(file, "ab");

    if (pPicture == NULL) {
        ALOGE(" demo decoder save nv21 picture error, picture pointer equals NULL");
        return;
    }

    if (fp == NULL) {
        ALOGE(" demo decoder save picture error, open file fail ");
        return;
    }

    nSizeY = pPicture->nWidth * pPicture->nHeight ;
    nSizeUV = nSizeY >> 1;
    //ALOGD(" save picture to file: %s, size: %dx%d, top: %d, bottom: %d, left: %d, right: %d",
    //        file, pPicture->nWidth, pPicture->nHeight,pPicture->nTopOffset,
    //        pPicture->nBottomOffset, pPicture->nLeftOffset, pPicture->nRightOffset);

    fwrite(pPicture->pData0, 1, nSizeY, fp);
    fwrite(pPicture->pData1, 1, nSizeUV, fp);

    fclose(fp);

}

int AWVideoDecoderImpl::decodeAsync(const void* inputBuf, unsigned int inputLen)
{
    mInputBuf = inputBuf;
    mInputBufLen = inputLen;
    mInputBufPos = 0;

    if ((NULL == inputBuf)) {
        ALOGE("buffer is NULL!");
        return -1;
    }

    //ALOGW("inputLen = %d,%02x %02x %02x %02x %02x,%02x %02x %02x",inputLen,
    //              data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
    //-----------------------dcoder-------------------

    int nRet;
    nRequestDataSize = inputLen;
    nValidSize = VideoStreamBufferSize(pVideoDec, 0) - VideoStreamDataSize(pVideoDec, 0);
    if (nRequestDataSize > nValidSize) {
        ALOGW("nRequestDataSize > nValidSize");
        return -1;
    }

    nRet = RequestVideoStreamBuffer(pVideoDec,
                                    nRequestDataSize,
                                    (char**)&packet_buf,
                                    &packet_buflen,
                                    (char**)&packet_ringBuf,
                                    &packet_ringBufLen,
                                    0);                                     //VdecH264:step3
    if (nRet != 0) {
        ALOGW(" RequestVideoStreamBuffer fail. request size: %d, valid size: %d ",
              nRequestDataSize, nValidSize);
        return -2;
    }

    if (packet_buflen + packet_ringBufLen < nRequestDataSize) {
        ALOGW(" RequestVideoStreamBuffer fail, require size is too small");
        //goto parser_exit;
    }
    //ALOGE("VdecH264 dbg:RequestVideoStreamBuffer ok");

    //-------------------------fill the NALU data----------------------------------
    if (packet_buf == NULL) {
        ALOGW("packet_buf == NULL");
        return -3;
    }
    //ALOGD("b4 bug[%d<%d]",inputLen,packet_buflen);usleep(10*1000);
    //ALOGD("\t\t\t nal%d<%d[%d+%d]",inputLen,(packet_buflen+packet_ringBufLen), \
    //packet_buflen, packet_ringBufLen);

    if (inputLen <= packet_buflen) {
        //memcpy(packet_buf, inputBuf, inputLen);
        readInputBuf(packet_buf, inputLen);
    } else if (inputLen <= (packet_buflen + packet_ringBufLen)) {
        //memcpy(packet_buf, inputBuf, packet_buflen);
        readInputBuf(packet_buf, packet_buflen);
        readInputBuf(packet_ringBuf, inputLen - packet_buflen);
    } else {
        ALOGE("VideoStreamBuf is not enough:%u>%d[%d+%d]", inputLen, \
              (packet_buflen + packet_ringBufLen), \
              packet_buflen, packet_ringBufLen);
    }

    //ALOGD("af bug");//usleep(10*1000);

    //-------------------------fill the NALU data end------------------------------

    memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
    dataInfo.pData          = packet_buf;
    dataInfo.nLength      = nRequestDataSize;
    dataInfo.nPts          = packet_pts;
    dataInfo.nPcr          = packet_pcr;
    dataInfo.bIsFirstPart = 1;
    dataInfo.bIsLastPart = 1;
    dataInfo.bValid = 1;
    nRet = SubmitVideoStreamData(pVideoDec, &dataInfo, 0);      //VdecH264:step4
    if (nRet != 0) {
        ALOGE(" parser thread  SubmitVideoStreamData() error ");
        //goto parser_exit;
    }
    //ALOGE("VdecH264 dbg:SubmitVideoStreamData ok");

    //-----------------decode begin-------------------------------------------------
    nRet = DecodeVideoStream(pVideoDec, 0 /*eos*/,
                             0/*key frame only*/, 0/*drop b frame*/,
                             0/*current time*/);                                 //VdecH264:step5
    //ALOGD("VdecH264 dbg:DecodeVideoStream return %d", nRet);

    if (!(nRet == VDECODE_RESULT_KEYFRAME_DECODED ||
          nRet == VDECODE_RESULT_FRAME_DECODED ||
          nRet == VDECODE_RESULT_NO_BITSTREAM)) {
        ALOGW("DecodeVideoStream invalid! nRet:%d", nRet);
        return -4;
    }
    //-----------------decode end-------------------------------------------------

    //-----------------get the yuv pic begin--------------------------------------
    nRet = ValidPictureNum(pVideoDec, 0);
    return nRet;
}

int AWVideoDecoderImpl::requestPicture(AVPacket* picBuf)
{
    int ret = -1;
    VideoPicture* pic = RequestPicture(pVideoDec, 0/*the major stream*/); //VdecH264:step6
    if (pic != NULL) {
        int bitSize = getFrameBitSize(pic->nWidth, pic->nHeight,
                                      (PixelFmt)mVideoConf.eOutputPixelFormat);
        picBuf->pAddrPhy0 = (unsigned char*)pic->phyYBufAddr;
        picBuf->pAddrPhy1 = (unsigned char*)pic->phyCBufAddr;
        picBuf->pAddrVir0 = (unsigned char*)pic->pData0;
        picBuf->pAddrVir1 = (unsigned char*)pic->pData1;
        picBuf->dataLen0 = bitSize;
        picBuf->handler = (unsigned char*)pic;
        ret = 0;
        ALOGD("%s %p", __FUNCTION__, picBuf->handler);
    }
    return ret;
}
int AWVideoDecoderImpl::releasePicture(AVPacket* picBuf)
{
    int ret = 0;
    ret = ReturnPicture(pVideoDec, (VideoPicture*)picBuf->handler);                         //VdecH264:step7
    ALOGD("%s %p,ret:%d", __FUNCTION__, picBuf->handler, ret);
    return ret;
}

int AWVideoDecoderImpl::decode(const void* inputBuf, unsigned int inputLen,
                               void* outputBuf, unsigned int outputLen)
{
    mInputBuf = inputBuf;
    mInputBufLen = inputLen;
    mInputBufPos = 0;

    mOutputBuf = outputBuf;
    mOutputBufLen = outputLen;
    mOutputBufPos = 0;

    if ((NULL == inputBuf) || (NULL == outputBuf)) {
        ALOGE("buffer is NULL!");
        return -1;
    }

    //ALOGW("inputLen = %d,%02x %02x %02x %02x %02x,%02x %02x %02x",inputLen,
    //              data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
    //-----------------------dcoder-------------------
    //ALOGD("\t\t\t YGC Get inputLen:%d", inputLen);

    int nRet;
    nRequestDataSize = inputLen;
    nValidSize = VideoStreamBufferSize(pVideoDec, 0) - VideoStreamDataSize(pVideoDec, 0);
    if (nRequestDataSize > nValidSize) {
        ALOGE("nRequestDataSize > nValidSize");
        return -1;
    }

    nRet = RequestVideoStreamBuffer(pVideoDec,
                                    nRequestDataSize,
                                    (char**)&packet_buf,
                                    &packet_buflen,
                                    (char**)&packet_ringBuf,
                                    &packet_ringBufLen,
                                    0);                                     //VdecH264:step3
    if (nRet != 0) {
        ALOGE(" RequestVideoStreamBuffer fail. request size: %d, valid size: %d ",
              nRequestDataSize, nValidSize);
        return -2;
    }

    if (packet_buflen + packet_ringBufLen < nRequestDataSize) {
        ALOGW(" RequestVideoStreamBuffer fail, require size is too small");
        //goto parser_exit;
    }
    //ALOGE("VdecH264 dbg:RequestVideoStreamBuffer ok");

    //-------------------------fill the NALU data----------------------------------
    if (packet_buf == NULL) {
        ALOGE("packet_buf == NULL");
        return -3;
    }
    //ALOGD("b4 bug[%d<%d]",inputLen,packet_buflen);usleep(10*1000);
    //ALOGD("\t\t\t nal%d<%d[%d+%d]",inputLen,(packet_buflen+packet_ringBufLen), \
    //packet_buflen, packet_ringBufLen);

    if (inputLen <= packet_buflen) {
        //memcpy(packet_buf, inputBuf, inputLen);
        readInputBuf(packet_buf, inputLen);
    } else if (inputLen <= (packet_buflen + packet_ringBufLen)) {
        //memcpy(packet_buf, inputBuf, packet_buflen);
        readInputBuf(packet_buf, packet_buflen);
        readInputBuf(packet_ringBuf, inputLen - packet_buflen);
    } else {
        ALOGE("VideoStreamBuf is not enough:%u>%d[%d+%d]", inputLen, \
              (packet_buflen + packet_ringBufLen), \
              packet_buflen, packet_ringBufLen);
    }

    memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
    dataInfo.pData          = packet_buf;
    dataInfo.nLength      = nRequestDataSize;
    dataInfo.nPts          = packet_pts;
    dataInfo.nPcr          = packet_pcr;
    dataInfo.bIsFirstPart = 1;
    dataInfo.bIsLastPart = 1;
    dataInfo.bValid = 1;
    nRet = SubmitVideoStreamData(pVideoDec, &dataInfo, 0);      //VdecH264:step4
    if (nRet != 0) {
        ALOGE(" parser thread  SubmitVideoStreamData() error ");
        //goto parser_exit;
    }
    //ALOGE("VdecH264 dbg:SubmitVideoStreamData ok");

    //-----------------decode begin-------------------------------------------------
    nRet = DecodeVideoStream(pVideoDec, 0 /*eos*/,
                             0/*key frame only*/, 0/*drop b frame*/,
                             0/*current time*/);                                 //VdecH264:step5
    //ALOGD("VdecH264 dbg:DecodeVideoStream return %d", nRet);

    if (!(nRet == VDECODE_RESULT_KEYFRAME_DECODED ||
          nRet == VDECODE_RESULT_FRAME_DECODED ||
          nRet == VDECODE_RESULT_NO_BITSTREAM)) {
        //usleep(1000);
        ALOGW("DecodeVideoStream invalid! nRet:%d", nRet);
        return -4;
    }
    //-----------------decode end-------------------------------------------------

    //-----------------get the yuv pic begin--------------------------------------
    int nValidPicNum = ValidPictureNum(pVideoDec, 0);
    if (nValidPicNum < 0) {
        nRet = 0;
        ALOGE("nValidPicNum<0,DecodeVideoStream ret:%d", nValidPicNum);
        return nRet;
    } else if (nValidPicNum > 1) {
        ALOGW("ValidPictureNum:%d,may be loss frame.", nValidPicNum);
    }

    pPicture = RequestPicture(pVideoDec, 0/*the major stream*/); //VdecH264:step6
    if (pPicture != NULL) {
#ifdef BOGUS
        if (pPicture->nWidth * pPicture->nHeight > 0) {
            char path[60];
            struct timeval time;
            gettimeofday(&time, NULL);
            static int a = 1;
            sprintf(path, "/tmp/output/[%dx%d]nv21.yuv",
                    pPicture->nWidth, pPicture->nHeight);
            saveNV21Picture(pPicture, path);
        }
#endif /* BOGUS */
        mParam->dstW = pPicture->nWidth;
        mParam->dstH = pPicture->nHeight;
        int bitSize = getFrameBitSize(pPicture->nWidth, pPicture->nHeight,
                                      (PixelFmt)mVideoConf.eOutputPixelFormat);

        if (outputLen < bitSize) {
            ALOGW("output buffer is not enough current:%u,need:%d!", outputLen, bitSize);
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            return bitSize;
        }

        nRet = writeOutputBuf(pPicture->pData0, pPicture->nWidth * pPicture->nHeight);
        if (nRet != 0) {
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            CHECK_RETURN(nRet);
        }

        nRet = writeOutputBuf(pPicture->pData1, pPicture->nWidth * pPicture->nHeight / 2);
        if (nRet != 0) {
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            CHECK_RETURN(nRet);
        }

        nRet =  mOutputBufPos;
        ReturnPicture(pVideoDec, pPicture);                         //VdecH264:step7
    } else {
        ALOGD("pPicture == NULL");
        return -6;

    }
    return nRet;
}

int AWVideoDecoderImpl::decode(const AVPacket* inPacket)
{
    //ALOGW("inputLen = %d,%02x %02x %02x %02x %02x,%02x %02x %02x",inputLen,
    //              data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
    //-----------------------dcoder-------------------
    //ALOGD("\t\t\t YGC Get inputLen:%d", inputLen);

    int nRet;
    nRequestDataSize = inPacket->dataLen0;
    nValidSize = VideoStreamBufferSize(pVideoDec, 0) - VideoStreamDataSize(pVideoDec, 0);
    if (nRequestDataSize > nValidSize) {
        ALOGE("nRequestDataSize > nValidSize");
        return -1;
    }

    nRet = RequestVideoStreamBuffer(pVideoDec,
                                    nRequestDataSize,
                                    (char**)&packet_buf,
                                    &packet_buflen,
                                    (char**)&packet_ringBuf,
                                    &packet_ringBufLen,
                                    0);                                     //VdecH264:step3
    if (nRet != 0) {
        ALOGW(" RequestVideoStreamBuffer fail. request size: %d, valid size: %d ",
              nRequestDataSize, nValidSize);
        return -2;
    }

    if (packet_buflen + packet_ringBufLen < nRequestDataSize) {
        ALOGW(" RequestVideoStreamBuffer fail, require size is too small");
        //goto parser_exit;
    }
    //ALOGE("VdecH264 dbg:RequestVideoStreamBuffer ok");

    //-------------------------fill the NALU data----------------------------------
    if (packet_buf == NULL) {
        ALOGW("packet_buf == NULL");
        return -3;
    }
    //ALOGD("b4 bug[%d<%d]",inputLen,packet_buflen);usleep(10*1000);
    //ALOGD("\t\t\t nal%d<%d[%d+%d]",inputLen,(packet_buflen+packet_ringBufLen), \
    //packet_buflen, packet_ringBufLen);

    if (inPacket->dataLen0 <= packet_buflen) {
        memcpy(packet_buf, inPacket->pAddrVir0, inPacket->dataLen0);
        //readInputBuf(packet_buf, inPacket->dataLen0);
    } else if ((NULL != packet_ringBuf) && (inPacket->dataLen0 <= (packet_buflen + packet_ringBufLen))) {
        memcpy(packet_buf, inPacket->pAddrVir0, packet_buflen);
        memcpy(packet_ringBuf, inPacket->pAddrVir0 + packet_buflen, inPacket->dataLen0 - packet_buflen);
        //readInputBuf(packet_buf, packet_buflen);
        //readInputBuf(packet_ringBuf, inPacket->dataLen0 - packet_buflen);
    } else {
        ALOGE("VideoStreamBuf is not enough:%d>%d[%d+%d]", inPacket->dataLen0, \
              (packet_buflen + packet_ringBufLen), \
              packet_buflen, packet_ringBufLen);
    }

    //ALOGD("af bug");//usleep(10*1000);

    //-------------------------fill the NALU data end------------------------------

    memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
    dataInfo.nID          = inPacket->id;
    dataInfo.nPts          = inPacket->pts;
    dataInfo.pData          = packet_buf;
    dataInfo.nLength      = nRequestDataSize;
    dataInfo.nPcr          = packet_pcr;
    dataInfo.bIsFirstPart = 1;
    dataInfo.bIsLastPart = 1;
    dataInfo.bValid = 1;

    nRet = SubmitVideoStreamData(pVideoDec, &dataInfo, 0);      //VdecH264:step4
    if (nRet != 0) {
        ALOGW(" parser thread  SubmitVideoStreamData() error ");
        //goto parser_exit;
    }

    //-----------------decode begin-------------------------------------------------
    nRet = DecodeVideoStream(pVideoDec, 0 /*eos*/,
                             0/*key frame only*/, 0/*drop b frame*/,
                             0/*current time*/);                                 //VdecH264:step5
    //ALOGD("VdecH264 dbg:DecodeVideoStream return %d", nRet);

    if (!(nRet == VDECODE_RESULT_KEYFRAME_DECODED ||
          nRet == VDECODE_RESULT_FRAME_DECODED ||
          nRet == VDECODE_RESULT_NO_BITSTREAM)) {
        //usleep(1000);
        ALOGW("DecodeVideoStream invalid! nRet:%d", nRet);
        return -4;
    }
    //-----------------decode end-------------------------------------------------

    //-----------------get the yuv pic begin--------------------------------------
    int nValidPicNum = ValidPictureNum(pVideoDec, 0);
    if (nValidPicNum < 0) {
        nRet = 0;
        ALOGE("nValidPicNum<0,DecodeVideoStream ret:%d", nValidPicNum);
        return nRet;
    } else if (nValidPicNum > 1) {
        ALOGW("ValidPictureNum:%d,may be loss frame nal:%d.", nValidPicNum, inPacket->dataLen0);
    } else if (nValidPicNum == 0) {
        ALOGW("ValidPictureNum:%d,may be no frame nal:%d.", nValidPicNum, inPacket->dataLen0);
    }

    pPicture = RequestPicture(pVideoDec, 0/*the major stream*/); //VdecH264:step6
    if (pPicture != NULL) {
        mParam->dstW = pPicture->nWidth;
        mParam->dstH = pPicture->nHeight;
        int bitSize = getFrameBitSize(pPicture->nWidth, pPicture->nHeight,
                                      (PixelFmt)mVideoConf.eOutputPixelFormat);

        AVPacket outPacket;
        outPacket.id = pPicture->nID;
        outPacket.pts = pPicture->nPts;

        outPacket.pAddrPhy0 = (unsigned char*)pPicture->phyYBufAddr;
        outPacket.pAddrVir0 = (unsigned char*)pPicture->pData0;
        outPacket.dataLen0 = pPicture->nWidth * pPicture->nHeight;

        outPacket.pAddrPhy1 = (unsigned char*)pPicture->phyCBufAddr;
        outPacket.pAddrVir1 = (unsigned char*)pPicture->pData1;
        outPacket.dataLen1 = outPacket.dataLen0 / 2;

        if (NULL != mDataCbk) {
            mDataCbk->decoderDataReady(&outPacket);
        }

        ReturnPicture(pVideoDec, pPicture);                         //VdecH264:step7
    } else {
        ALOGW("pPicture == NULL,nal:%d", inPacket->dataLen0);
        return -6;

    }
    return nRet;
}

int AWVideoDecoderImpl::stopAndGetLeftFream(void* outputBuf, unsigned int outputLen)
{
    int nRet;
    mOutputBuf = outputBuf;
    mOutputBufLen = outputLen;
    mOutputBufPos = 0;

    if ((NULL == outputBuf)) {
        ALOGE("buffer is NULL!");
        return -1;
    }

    int leftFream = VideoStreamFrameNum(pVideoDec, 0);
    ALOGD("VideoStreamFrameNum:%d", leftFream);
    if (leftFream > 0) {
        int waitLoop = 0;
        while (leftFream > 0) {
            nRet = DecodeVideoStream(pVideoDec,
                                     1 /*eos*/,
                                     0/*key frame only*/,
                                     0/*drop b frame*/,
                                     0/*current time*/);
            ALOGD("DecodeVideoStream:%d", nRet);
            if ((nRet == VDECODE_RESULT_NO_FRAME_BUFFER) ||
                (nRet == VDECODE_RESULT_NO_BITSTREAM)) {
                break;
            }

            usleep(10 * 1000);
            waitLoop++;
            if (waitLoop > 50) {
                ALOGW("decode eos time out > 10 ms!");
                break;
            }
        }
    } else {
        ALOGD("There is no left fream to decode!");
    }

    int nValidPicNum = ValidPictureNum(pVideoDec, 0);
    if (nValidPicNum < 0) {
        nRet = -2;
        ALOGW("nValidPicNum:%d, no pic left.", nValidPicNum);
        return nRet;
    }

    pPicture = RequestPicture(pVideoDec, 0/*the major stream*/); //VdecH264:step6
    if (pPicture != NULL) {
        ALOGD("RequestPicture.");
        mParam->dstW = pPicture->nWidth;
        mParam->dstH = pPicture->nHeight;
        int bitSize = getFrameBitSize(pPicture->nWidth, pPicture->nHeight,
                                      (PixelFmt)mVideoConf.eOutputPixelFormat);

        if (outputLen < bitSize) {
            ALOGE("output buffer is not enough current:%u,need:%d!", outputLen, bitSize);
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            return bitSize;
        }

        nRet = writeOutputBuf(pPicture->pData0, pPicture->nWidth * pPicture->nHeight);
        if (nRet != 0) {
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            CHECK_RETURN(nRet);
        }

        nRet = writeOutputBuf(pPicture->pData1, pPicture->nWidth * pPicture->nHeight / 2);
        if (nRet != 0) {
            ReturnPicture(pVideoDec, pPicture);                     //VdecH264:step7
            CHECK_RETURN(nRet);
        }

        nRet =  mOutputBufPos;
        ReturnPicture(pVideoDec, pPicture);                         //VdecH264:step7
    } else {
        ALOGW("pPicture == NULL.");
        return -2;

    }
    return nRet;
}
}

