#define LOG_TAG "decoder_test"
#include "sdklog.h"

#include "AWVideoDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#define CHECK_DISP_RETURN(a,b) \
    do { \
         if(a<0){\
           ALOGD(HL_RED "[E]" "[%s]%s fail! line:%d " PF_CLR, __FUNCTION__,b, __LINE__);\
           return; \
           }\
    } while (0)

using namespace awvideodecoder;

#define H264_DECODER
#define DEFAULT_BUF_LEN   (500*1024)

#ifdef H264_DECODER
#define INPUT_H264 "/tmp/decoder_test/video_800x480.h264"
#define OUTPUT_YUV "/tmp/decoder_test/video_800x480.yuv"

#else
#define INPUT_JPG "/tmp/decoder_test/pic_1920x1080.jpg"
#define OUTPUT_YUV "/tmp/decoder_test/pic_1920x1080.yuv"
#endif

const char* pixelExt[] = {
    "default",
    "planer_420",
    "planer_422",
    "planer_444",
    "yv12",
    "nv21",
    "nv12",
    "mb32_420",
    "mb32_422",
    "mb32_444",
    "rgba",
    "argb",
    "abgr",
    "bgra",
    "yuyv",
    "yvyu",
    "uyvy",
    "vyuy",
    "planaruv_422",
    "planarvu_422",
    "planaruv_444",
    "planarvu_444",
};

typedef struct TestParm_ {
    char intputFile[256];
    char outputFile[256];
    int  testWay;
    int  testTimes;
} TestParm;

DecodeParam    decodeParam;
TestParm    testParam;

struct ThreadParm {
    int pixel;
};

typedef enum {
    INPUT,
    HELP,
    DECODE_FORMAT,
    PIXEL_FORMAT,
    OUTPUT,
    SRC_W,
    SRC_H,
    TEST_WAY,
    TEST_TIMES,
    INVALID
} ArguType;

typedef struct {
    char option[8];
    char type[128];
    ArguType argument;
    char description[512];
} Argument;

static const Argument ArguTypeMapping[] = {
    {
        "-h",  "--help",    HELP,
        "Print this help"
    },
    {
        "-i",  "--input",   INPUT,
        "Input file path"
    },
    {
        "-f",  "--codecType",  DECODE_FORMAT,
        "0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder"
    },
    {
        "-p",  "--pixelFormat",  PIXEL_FORMAT,
        "0: YUV420SP, 1:YVU420SP, 3:YUV420P"
    },
    {
        "-o",  "--output",  OUTPUT,
        "output file path"
    },
    {
        "-sw",  "--srcw",  SRC_W,
        "SRC_W"
    },
    {
        "-sh",  "--srch",  SRC_H,
        "SRC_H"
    },
    {
        "-w",  "--test way",  TEST_WAY,
        "(0:single test, 1:multiDecoderTest)"
    },
    {
        "-t",  "--test times",  TEST_TIMES,
        " testTimes"
    },
};

static ArguType GetArguType(char* name)
{
    int i = 0;
    int num = sizeof(ArguTypeMapping) / sizeof(Argument);
    while (i < num) {
        if ((0 == strcmp(ArguTypeMapping[i].type, name)) ||
            ((0 == strcmp(ArguTypeMapping[i].option, name)) &&
             (0 != strcmp(ArguTypeMapping[i].option, "--")))) {
            return ArguTypeMapping[i].argument;
        }
        i++;
    }
    return INVALID;
}

static void PrintDemoUsage(void)
{
    int i = 0;
    int num = sizeof(ArguTypeMapping) / sizeof(Argument);
    printf("====================================== Usage ======================================\n");
    while (i < num) {
        printf("decoder_test %s %-16s %s", ArguTypeMapping[i].option, ArguTypeMapping[i].type,
               ArguTypeMapping[i].description);
        printf("\n");
        i++;
    }
    printf("====================================== Usage ======================================\n");
}

static void ParseArguType(DecodeParam* decodeParam, TestParm* testParam, char* argument, char* value)
{
    ArguType arg;

    arg = GetArguType(argument);

    switch (arg) {
        case HELP:
            PrintDemoUsage();
            exit(-1);
        case INPUT:
            memset(testParam->intputFile, 0, sizeof(testParam->intputFile));
            sscanf(value, "%255s", testParam->intputFile);
            ALOGD(" get input file: %s ", testParam->intputFile);
            break;
        case DECODE_FORMAT:
            sscanf(value, "%32u", &decodeParam->codecType);
            break;
        case PIXEL_FORMAT:
            sscanf(value, "%32u", &decodeParam->pixelFormat);
            break;
        case OUTPUT:
            memset(testParam->outputFile, 0, sizeof(testParam->outputFile));
            sscanf(value, "%255s", testParam->outputFile);
            ALOGD(" get output file: %s ", testParam->outputFile);
            break;
        case SRC_W:
            sscanf(value, "%32u", &decodeParam->srcW);
            ALOGD(" get srcW: %dp ", decodeParam->srcW);
            break;
        case SRC_H:
            sscanf(value, "%32u", &decodeParam->srcH);
            ALOGD(" get dstH: %dp ", decodeParam->srcH);
            break;
//    case BIT_RATE:
//        sscanf(value, "%32d", &decodeParam->bitRate);
//        ALOGD(" bit rate: %d ", decodeParam->bitRate);
//        break;
//    case ENCODE_FRAME_NUM:
//        sscanf(value, "%32u", &decodeParam->frameCount);
//        break;
        case TEST_WAY:
            sscanf(value, "%32d", &testParam->testWay);
            ALOGD(" test %d->%d times ", testParam->testWay, testParam->testTimes);

        case TEST_TIMES:
            sscanf(value, "%32d", &testParam->testTimes);
            ALOGD(" test %d->%d times ", testParam->testWay, testParam->testTimes);

            break;
        case INVALID:
        default:
            ALOGD("unknowed argument :  %s", argument);
            break;
    }

}

static void printfArgs(DecodeParam    decodeParam)
{
    ALOGD("\t -i:%s", testParam.intputFile);
    ALOGD("\t -s:[%dx%d]", decodeParam.srcW, decodeParam.srcH);
    ALOGD("\t -f:%d", decodeParam.codecType);
    ALOGD("\t -p:%d", decodeParam.pixelFormat);
    ALOGD("\t -o:%s", testParam.outputFile);

}

int find264NALFragment(char* data, size_t size, int* fragSize)
{
    static const char kStartCode[4] = { 0x00, 0x00, 0x00, 0x01 };
    if (size < 4) {
        ALOGD("size %zu < 4", size);
        return -1;
    }

    if (memcmp(kStartCode, data, 4)) {
        ALOGD("StartCode not found in %.2x %.2x %.2x %.2x", data[0], data[1], data[2], data[3]);
        return -2;
    }

    size_t offset = 4;
    while (offset + 3 < size && memcmp(kStartCode, &data[offset], 4)) {
        ++offset;
    }

    if (offset > (size - 4)) {
        ALOGD("offset>(size-4)");
        offset = size;
    }
    *fragSize = offset;

    return (int)(data[4] & 0x1f);
}

int getFrameBitSize(int w, int h, PixelFmt fmt)
{
    int pixelNum = w * h;
    //ALOGD("pic size:[%dx%d]",w,h);
    int frameBitSize = 0;

    switch (fmt) {
        case PIXEL_YV12...PIXEL_NV21:
            frameBitSize = pixelNum * 3 / 2;
            break;
        default:
            frameBitSize = pixelNum * 3 / 2;
            break;
    }
#ifdef BOGUS

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
#endif /* BOGUS */
    return frameBitSize;
}

void getExpectSize(int* w, int* h)
{
    int dstW = decodeParam.srcW >> decodeParam.scaleRatio;
    int dstH = decodeParam.srcH >> decodeParam.scaleRatio;
    if ((decodeParam.rotation == Angle_90) || (decodeParam.rotation == Angle_270)) {
        * w = dstH;
        * h = dstW;
    } else {
        * w = dstW;
        * h = dstH;
    }
}

class MyDecoderCallback: public AWVideoDecoderDataCallback
{
public:
    MyDecoderCallback(FILE* file): mFile(file) {}
    virtual ~MyDecoderCallback() {}
    virtual int decoderDataReady(AVPacket* packet)
    {
        if ((NULL != packet->pAddrVir0) && (packet->dataLen0 > 0))
            fwrite(packet->pAddrVir0, 1, packet->dataLen0, mFile);

        if ((NULL != packet->pAddrVir1) && (packet->dataLen1 > 0))
            fwrite(packet->pAddrVir1, 1, packet->dataLen1, mFile);
        ALOGD("decode write[%d/%d]", packet->dataLen0, packet->dataLen1);
        return 0;
    };
private:
    FILE* mFile;
};

#define USE_CBK
#ifdef USE_CBK
int singleDecoderTest()
{
    ALOGD("\t\t ================================");
    ALOGD("\t\t ====     CedarC  Decoder    ====");
    ALOGD("\t\t ====  singleDecoderTest cbk ====");
    ALOGD("\t\t ================================");

    int ret = 0;

    FILE* inFile = NULL;
    FILE* outputFile = NULL;
    char* inputBuf = NULL;
    int intputLen = 0;
    char* outputBuf = NULL;

    AWVideoDecoder* pDecoder = NULL;
    MyDecoderCallback* pCallback = NULL;
    int fileSize;

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;
    int count = 0x100;
    int dstW;
    int dstH;
    AVPacket inPacket;

    inFile = fopen(testParam.intputFile, "r");
    if (inFile == NULL) {
        ALOGE("open inFile:%s fail", testParam.intputFile);
        return -1;
    }
    outputFile = NULL;
    getExpectSize(&dstW, &dstH);
    int bitSize = getFrameBitSize(dstW, dstH, decodeParam.pixelFormat);

    inputBuf = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen = DEFAULT_BUF_LEN;
    outputBuf = (char*)malloc(bitSize);

    if (outputFile == NULL) {
        outputFile = fopen(testParam.outputFile, "wb");
        if (outputFile == NULL) {
            ALOGE("open outputFile fail");
            goto EXIT;
        }
    }

    pDecoder = AWVideoDecoder::create();
    pCallback = new MyDecoderCallback(outputFile);
    ret = pDecoder->init(&decodeParam, pCallback);
    if (ret < 0) {
        ALOGE("pDecoder->init fail:%d ", ret);
        goto EXIT;
    }

    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    while (decodePos < fileSize) {
        ALOGD("decode process[%d/%d]", decodePos, fileSize);
        count++;
        readTmpLen = ((fileSize - decodePos) < intputLen) ? (fileSize - decodePos) : intputLen;
        fread(inputBuf, 1, readTmpLen, inFile);
        nalLen = 0;
        ret = find264NALFragment((char*)inputBuf, readTmpLen, &nalLen);
        if (nalLen < 4) {
            ALOGE("find264NALFragment fail ");
            break;
        }

        inPacket.pAddrVir0 = (unsigned char*)inputBuf;
        inPacket.dataLen0 = nalLen;
        inPacket.id = count;
        struct timeval now;
        gettimeofday(&now, NULL);
        inPacket.pts = now.tv_sec * 1000000 + now.tv_usec;

        //ALOGD("fmt_sgl[%s-%d]decode %d bytes.",pixelExt[decodeParam.pixelFormat],data++,nalLen);
        ret = pDecoder->decode(&inPacket);
        if (0 < ret) {
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.

            //ALOGD("fmt_sgl[%s-%d]write %d bytes.",pixelExt[decodeParam.pixelFormat],count++,ret);
        } else {
            ALOGW("decode failed,error code:%d", ret);
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        }
    }

    printfArgs(decodeParam);
    ALOGD("output file is saved:%s", testParam.outputFile);

EXIT:
    delete pCallback;
    AWVideoDecoder::destroy(pDecoder);

    free(inputBuf);
    free(outputBuf);

    if (outputFile)
        fclose(outputFile);
    if (inFile)
        fclose(inFile);
    return 0;
}
#else
int singleDecoderTest()
{
    ALOGD("\t\t ================================");
    ALOGD("\t\t ====     CedarC  Decoder    ====");
    ALOGD("\t\t ====    singleDecoderTest    ====");
    ALOGD("\t\t ================================");

    int ret = 0;
    FILE* inFile = fopen(testParam.intputFile, "r");
    FILE* outFile = fopen(testParam.outputFile, "wb");
    if ((inFile == NULL)) {
        ALOGE("open inFile fail:%s", testParam.intputFile);
        return NULL;
    }
    if ((outFile == NULL)) {
        ALOGE("open outFile fail:%s", testParam.outputFile);
        return NULL;
    }

    char* inputBuf = NULL;
    int intputLen = 0;
    char* outputBuf = NULL;
    int outputLen = 0;

    int dstW;
    int dstH;
    getExpectSize(&dstW, &dstH);
    int bitSize = getFrameBitSize(dstW, dstH, decodeParam.pixelFormat);

    inputBuf = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen = DEFAULT_BUF_LEN;
    outputBuf = (char*)malloc(bitSize);
    outputLen = bitSize;

    AWVideoDecoder* pDecoder = AWVideoDecoder::create(&decodeParam);

    int fileSize;
    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;
    int count = 1;
    int data = 1;

    while (decodePos < fileSize) {
        //ALOGD("decode process[%d/%d]", decodePos,fileSize);
        readTmpLen = ((fileSize - decodePos) < intputLen) ? (fileSize - decodePos) : intputLen;
        fread(inputBuf, 1, readTmpLen, inFile);
        ret = find264NALFragment((char*)inputBuf, readTmpLen, &nalLen);

        if (nalLen < 4) {
            ALOGE("find264NALFragment fail ");
            break;
        }

        //ALOGD("fmt_sgl[%s-%d]decode %d bytes.",pixelExt[decodeParam.pixelFormat],data++,nalLen);
        ret = pDecoder->decodeAsync(inputBuf, nalLen);

        if (ret >= 0) {
            AVPacket picBuf;
            memset(&picBuf, 0, sizeof(AVPacket));
            pDecoder->requestPicture(&picBuf);
            if (NULL != picBuf.handler) {
                fwrite(picBuf.pAddrVir0, 1, picBuf.dataLen0, outFile);
                ret = pDecoder->releasePicture(&picBuf);
                if (ret < 0) {
                    ALOGE("releasePicture error.");
                }
                ALOGD("decodeAsync write %d [%p-%p]bytes.", picBuf.dataLen0, picBuf.pAddrPhy0, picBuf.pAddrPhy1);
                //usleep(50*1000);
            }
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        } else {
            ALOGW("decode failed,error code:%d", ret);
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        }
    }

    printfArgs(decodeParam);
    ALOGD("output file is saved:%s", testParam.outputFile);

    AWVideoDecoder::destroy(pDecoder);

    free(inputBuf);
    free(outputBuf);

    if (outFile)
        fclose(outFile);
    if (inFile)
        fclose(inFile);

}
#endif

#ifdef H264_DECODER
void mjpegDecodeTest()
{
    ALOGD("\t\t ================================");
    ALOGD("\t\t ====     CedarC  Decoder    ====");
    ALOGD("\t\t ====     mjpegDecodeTest    ====");
    ALOGD("\t\t ================================");

    int ret;
    int fileSize = 0;

    char* inputBuf = NULL;
    char* outputBuf = NULL;
    int outputLen = 0;

    decodeParam.codecType = CODEC_MJPEG;
//    decodeParam.srcW = 1920;
//    decodeParam.srcH = 1080;
//    int dstW = 0;
//    int dstH = 0;
//    getExpectSize(&dstW,&dstH);
    FILE* outputFile = NULL;
    FILE* inputFile = NULL;

    AWVideoDecoder* pDecoder = AWVideoDecoder::create(&decodeParam);

    inputFile = fopen(testParam.intputFile, "rb");
    if (inputFile == NULL) {
        ALOGD("open nal fail:%s", testParam.intputFile);
        return ;
    }

    ALOGD("open file:%s", testParam.intputFile);
    fseek(inputFile, 0L, SEEK_END);
    fileSize = ftell(inputFile);
    fseek(inputFile, 0L, SEEK_SET);

    inputBuf = (char*)malloc(fileSize);
    fread(inputBuf, 1, fileSize, inputFile);
    fclose(inputFile);

    outputLen = getFrameBitSize(1920, 1080, decodeParam.pixelFormat);
RETRY:
    outputBuf = (char*)malloc(outputLen);

    //decode
    ret = pDecoder->decode(inputBuf, fileSize, outputBuf, outputLen);
    if (ret > 0) {
        if (outputFile == NULL) {
            outputFile = fopen(testParam.outputFile, "wb");
            if (outputFile == NULL) {
                ALOGE("open outputFile fail");
            }
        }

        if (ret > outputLen) {
            ALOGW("output buf not enough,re-malloc");
            free(outputBuf);
            outputLen = ret;
            goto RETRY;
        }

        fwrite(outputBuf, 1, ret, outputFile);
    } else {
        ALOGE("decode fail:%s", testParam.intputFile);
    }
    AWVideoDecoder::destroy(pDecoder);

    free(inputBuf);
    free(outputBuf);
    if (outputFile)
        fclose(outputFile);

    ALOGD("output file is saved:%s", testParam.outputFile);
}
#endif

void* decodeThread(void* arg)
{
    DecodeParam* parm = (DecodeParam*)arg;

    FILE* inFile = fopen(testParam.intputFile, "r");
    if (inFile == NULL) {
        ALOGE("open inFile fail");
        return NULL;
    }

    FILE* outputFile = NULL;

    int ret = 0;
    int ExpectW;
    int ExpectH;
    getExpectSize(&ExpectW, &ExpectH);
    int bitSize = getFrameBitSize(ExpectW, ExpectH, decodeParam.pixelFormat);
    char* inputFile = (char*)malloc(DEFAULT_BUF_LEN);
    int inputLen = DEFAULT_BUF_LEN;
    char* outputBuf = (char*)malloc(bitSize);
    int outputLen = bitSize;

    parm->pixelFormat = PIXEL_NV21;//only support PIXEL_NV21 now
    AWVideoDecoder* pDecoder = AWVideoDecoder::create(parm);

    int fileSize;
    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;

    while (decodePos < fileSize) {
        //ALOGD("decode process[%d/%d]", decodePos,fileSize);
        readTmpLen = ((fileSize - decodePos) < inputLen) ? (fileSize - decodePos) : inputLen;
        fread(inputFile, 1, readTmpLen, inFile);

        nalLen = 0;
        ret = find264NALFragment((char*)inputFile, readTmpLen, &nalLen);
        if (nalLen < 4) {
            ALOGE("find264NALFragment fail ");
            break;
        }

        //ALOGD("fmt[%s-%d]decode %d bytes.",pixelExt[fmt],data++,nalLen);
        ret = pDecoder->decode(inputFile, nalLen, outputBuf, outputLen);
        //ALOGD("pDecoder->decode [%d<=%d]??? ",ret,outputLen);
        if ((0 < ret) && (ret <= outputLen)) {
            if (outputFile == NULL) {
                outputFile = fopen(testParam.outputFile, "wb");
                if (outputFile == NULL) {
                    ALOGE("open outputFile fail");
                    fclose(inFile);
                    return NULL;
                }
            }
            fwrite(outputBuf, 1, ret, outputFile);
            //ALOGD("fmt[%s-%d]write %d bytes.",pixelExt[fmt],count++,ret);
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        } else if (ret > outputLen) {
            ALOGW("output buf not enough,re-malloc and decode agian.");
            free(outputBuf);
            outputBuf = (char*)malloc(ret);
            outputLen = ret;
            fseek(inFile, decodePos, SEEK_SET); //stay the current decode position.
            continue;
        } else {
            ALOGE("decode failed,error code:%d", ret);
            decodePos += nalLen;
            fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        }
    }

    printfArgs(*parm);
    ALOGD("output file is saved:%s", testParam.outputFile);

    AWVideoDecoder::destroy(pDecoder);

    free(inputFile);
    free(outputBuf);

    if (outputFile)
        fclose(outputFile);
    if (inFile)
        fclose(inFile);
    return NULL;
}

int  multiDecoderTest()
{
    ALOGD("\t\t ================================");
    ALOGD("\t\t ====    CedarC   Decoder    ====");
    ALOGD("\t\t ====    multiDecoderTest    ====");
    ALOGD("\t\t ================================");

    decodeParam.rotation = Angle_0;
    decodeParam.scaleRatio = ScaleOneHalf;

    int startFmt = PIXEL_YUV_PLANER_420;
    int stopFmt = PIXEL_YUV_MB32_420;

    DecodeParam mDecodeParam[PIXEL_P010_VU];
    pthread_t threah[PIXEL_P010_VU];

    memset(mDecodeParam, 0, sizeof(mDecodeParam));
    for (int i = startFmt; i <= stopFmt; i++) {
        mDecodeParam[i].srcW = decodeParam.srcW;
        mDecodeParam[i].srcH = decodeParam.srcH;
        mDecodeParam[i].rotation = decodeParam.rotation;
        mDecodeParam[i].codecType = decodeParam.codecType;
        mDecodeParam[i].scaleRatio = decodeParam.scaleRatio;
        mDecodeParam[i].pixelFormat = PixelFmt(i);
    }

    for (int i = startFmt; i <= stopFmt; i++) {
        pthread_create(&threah[i], 0, decodeThread, (void*)&mDecodeParam[i]);
    }

    for (int i = startFmt; i <= stopFmt; i++) {
        pthread_join(threah[i], NULL);
    }
    return 0;
}

int main(int argc, char** argv)
{
    ALOGD("decoder_test version:%s", MODULE_VERSION);
    /******** begin set the default encode param ********/
    memset(&decodeParam, 0, sizeof(decodeParam));
    memset(&testParam, 0, sizeof(testParam));

    decodeParam.srcW = 800;
    decodeParam.srcH = 480;
    decodeParam.dstW = 800;
    decodeParam.dstH = 480;
    decodeParam.rotation = Angle_0;
    decodeParam.scaleRatio = ScaleNone;
    decodeParam.codecType = CODEC_H264;
    decodeParam.pixelFormat = PIXEL_NV21;

    testParam.testTimes = 1;
    testParam.testWay = 0;
    strcpy((char*)testParam.intputFile, INPUT_H264);
    strcpy((char*)testParam.outputFile, OUTPUT_YUV);

    /******** begin parse the config paramter ********/
    int i;
    if (argc >= 2) {
        for (i = 1; i < (int)argc; i += 2) {
            ParseArguType(&decodeParam, &testParam, argv[i], argv[i + 1]);
        }
    }

    for (int k = 1; k <= testParam.testTimes; k++) {
        if (testParam.testWay == 0) {
            singleDecoderTest();
            //mjpegDecodeTest();
        } else {
            multiDecoderTest();
        }
        ALOGD("Test %d times.", k);
    }

    return 0;
}

