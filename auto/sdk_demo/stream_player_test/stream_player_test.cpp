#define LOG_TAG "stream_player_test"
#include <sdklog.h>

#include "AWStreamPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

using namespace awstreamplayer;

#define DEFAULT_BUF_LEN   (500*1024)
#define MAX_NAL_SIZE  (1024*1024)

#define INPUT_JPG "/tmp/stream_player_test/pic_"
#define INPUT_H264 "/tmp/stream_player_test/video_800x480.h264"
#define INPUT_H265 "/tmp/stream_player_test/video_800x480.h265"
#define OUTPUT_FILE "/tmp/stream_player_test/output"

#define SCREEN_W    (1024)
#define SCREEN_H    (600)
#define TV_W        (720)
#define NTSC_H      (480)
#define PAL_H       (576)
#define TV_H(mode)  ((mode==NTSC)?NTSC_H:PAL_H)

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
    int  testCmd;
} TestParm;

PlayerParam    decodeParam;
TestParm    testParam;
AWStreamPlayer* pPlayer;
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
    TEST_CMD,
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
        "0:h264 decoder, 1:jpeg_decoder, 3:h265 coder"
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
        "-s",  "--srcw",  SRC_W,
        "SRC_W"
    },
    {
        "-d",  "--srch",  SRC_H,
        "SRC_H"
    },
    {
        "-w",  "--test way",  TEST_WAY,
        "(0:single test, 1:multiDecoderTest,2:single h264 fream)"
    },
    {
        "-t",  "--test times",  TEST_TIMES,
        " testTimes"
    },
    {
        "-c",  "--test cmd",  TEST_CMD,
        " testCmd"
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
        printf("stream_player_test %s %-16s %s", ArguTypeMapping[i].option, ArguTypeMapping[i].type,
               ArguTypeMapping[i].description);
        printf("\n");
        i++;
    }
    printf("====================================== Usage ======================================\n");
}

static void ParseArguType(PlayerParam* decodeParam, TestParm* testParam, char* argument, char* value)
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
            sscanf(value, "%32u", &decodeParam->srcW);
            ALOGD(" get dstH: %dp ", decodeParam->srcW);
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
        case TEST_CMD:
            sscanf(value, "%32d", &testParam->testCmd);
            ALOGD(" test cmd:%d", testParam->testCmd);

            break;
        case INVALID:
        default:
            ALOGD("unknowed argument :  %s", argument);
            break;
    }

}

static void printfArgs(PlayerParam    decodeParam)
{
    ALOGD("\t -i:%s", testParam.intputFile);
    ALOGD("\t -s:[%dx%d]", decodeParam.srcW, decodeParam.srcH);
    ALOGD("\t -f:%d", decodeParam.codecType);
    ALOGD("\t -o:%s", testParam.outputFile);
    ALOGD("\t -c:%d", testParam.testCmd);
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

void CenterAlignment(int w, int h)
{
    float scale = w / h;
    if (scale < 1) { //Center alignment,If you don't need it, please mark it.
        decodeParam.dispH = SCREEN_H;
        decodeParam.dispW = (decodeParam.dispH * w) / h;
        decodeParam.dispX = (SCREEN_W - decodeParam.dispW) / 2;

        decodeParam.tvDispH = TV_H(decodeParam.tvOut);
        decodeParam.tvDispW = (decodeParam.tvDispH * w) / h;;
        decodeParam.tvDispX = (TV_W - decodeParam.tvDispW) / 2;
    } else {
        decodeParam.dispW = w;
        decodeParam.dispH = h;
        decodeParam.dispX = 0;

        decodeParam.tvDispH = TV_H(decodeParam.tvOut);
        decodeParam.tvDispW = TV_W;
        decodeParam.tvDispX = 0;
    }
}

int h264OneFrameTest()
{
    ALOGD("\t ================================");
    ALOGD("\t ====     CedarC  Decoder    ====");
    ALOGD("\t ====        h264Test        ====");
    ALOGD("\t ================================");

    decodeParam.codecType = CODEC_H264;
    decodeParam.dispX = 0;
    decodeParam.dispW = 1280;
    decodeParam.dispH = 720;
    decodeParam.rotation = Angle_180;
    decodeParam.tvOut = PAL;

    int ret = 0;
    FILE* inFile = fopen(testParam.intputFile, "r");
    if (inFile == NULL) {
        ALOGD("open inFile:%s fail", testParam.intputFile);
        return -1;
    }

    char* inputBuf = NULL;
    int intputLen = 0;

    inputBuf = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen = DEFAULT_BUF_LEN;

    pPlayer = AWStreamPlayer::create(&decodeParam);
    pPlayer->startThread();

    int fileSize;
    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;
    int count = 0;

    while (decodePos < fileSize) {
        readTmpLen = ((fileSize - decodePos) < intputLen) ? (fileSize - decodePos) : intputLen;
        fread(inputBuf, 1, readTmpLen, inFile);
        nalLen = 0;
        ret = find264NALFragment((char*)inputBuf, readTmpLen, &nalLen);
        if (nalLen < 4) {
            ALOGD("find264NALFragment fail nalLen:%d,ret:%d", nalLen, ret);
            break;
        }
        decodePos += nalLen;
        ALOGD("decode  process[%d/%d]", decodePos, fileSize);

        //ALOGD("fmt_sgl[%s-%d]decode %d bytes.",pixelExt[decodeParam.pixelFormat],data++,nalLen);
        ret = pPlayer->sendFrameToThread((unsigned char*)inputBuf, nalLen);
        fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        if (count++ >= 3)break;
        usleep(5 * 1000 * 1000);
    }

    printfArgs(decodeParam);

    pPlayer->stopThread();
    AWStreamPlayer::destroy(pPlayer);

    free(inputBuf);

    if (inFile)
        fclose(inFile);
    return 0;
}

int h264Test()
{
    ALOGD("\t ================================");
    ALOGD("\t ====     CedarC  Decoder    ====");
    ALOGD("\t ====        h264Test        ====");
    ALOGD("\t ================================");

    decodeParam.codecType = CODEC_H264;
    decodeParam.dispX = 0;
    decodeParam.dispW = 1024;
    decodeParam.dispH = 600;
    decodeParam.rotation = Angle_0;
    decodeParam.tvOut = NONE;
    decodeParam.zorder = 20;
    strcpy((char*)testParam.intputFile, INPUT_H264);

    int ret = 0;
    FILE* inFile = fopen(testParam.intputFile, "r");
    if (inFile == NULL) {
        ALOGD("open inFile:%s fail", testParam.intputFile);
        return -1;
    }

    char* inputBuf = NULL;
    int intputLen = 0;

    inputBuf = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen = DEFAULT_BUF_LEN;

    pPlayer = AWStreamPlayer::create(&decodeParam);
    pPlayer->startThread();

    int fileSize;
    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;

    while (decodePos < fileSize) {
        ALOGD("decode  process[%d/%d]", decodePos, fileSize);
        readTmpLen = ((fileSize - decodePos) < intputLen) ? (fileSize - decodePos) : intputLen;
        fread(inputBuf, 1, readTmpLen, inFile);
        nalLen = 0;
        ret = find264NALFragment((char*)inputBuf, readTmpLen, &nalLen);
        if (nalLen < 4) {
            ALOGD("find264NALFragment fail nalLen:%d,ret:%d", nalLen, ret);
            break;
        }

        //ALOGD("fmt_sgl[%s-%d]decode %d bytes.",pixelExt[decodeParam.pixelFormat],data++,nalLen);
        ret = pPlayer->sendFrameToThread((unsigned char*)inputBuf, nalLen);
        // ret = pPlayer->decodeFrameDrectly((unsigned char *)inputBuf, nalLen);
        decodePos += nalLen;
        fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        usleep(30 * 1000);
    }

    printfArgs(decodeParam);

    pPlayer->stopThread();
    AWStreamPlayer::destroy(pPlayer);

    free(inputBuf);

    if (inFile)
        fclose(inFile);
    return 0;
}

int decodeStreamFile()
{
    ALOGD("***************************");
    ALOGD("***   decodeStreamFile    ***");
    ALOGD("***************************");

    //static AWStreamPlayer* pPlayer = NULL;

    decodeParam.rotation = Angle_0;
    decodeParam.scaleRatio = ScaleNone;
    decodeParam.codecType = CODEC_H264;
    decodeParam.zorder = 2;

    CenterAlignment(720, 1280);
    ALOGD("reset panel:(%d,%d)[%d:%d]", decodeParam.dispX, decodeParam.dispY, decodeParam.dispW, decodeParam.dispH);
    ALOGD("reset cvbs:(%d,%d)[%d:%d]", decodeParam.tvDispX, decodeParam.tvDispY, decodeParam.tvDispW, decodeParam.tvDispH);
    pPlayer = AWStreamPlayer::create(&decodeParam);
    pPlayer->startThread();

    int fileSize = 0;
    int readSize = 0;
    char file[60] = {0};
    int fileLoop = 30;

    char* nalbuf = (char*)malloc(MAX_NAL_SIZE);
    for (int i = 0; i <= fileLoop; i++) {
        sprintf(file, "%s%03d.h264", "/tmp/input/", i);

        FILE* fp = NULL;
        fp = fopen(file, "rb");
        if (fp == NULL) {
            ALOGD("open nal fail[%d]:%s", i, file);
            continue;
        }
        fseek(fp, 0L, SEEK_END);
        fileSize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        if (fileSize > MAX_NAL_SIZE) {
            ALOGD("error,decode buf not enough!");
            continue;
        }
        readSize = fread(nalbuf, fileSize, 1, fp);
        fclose(fp);

        pPlayer->sendFrameToThread((unsigned char*)nalbuf, fileSize);

        ALOGD("read [%d] %s->%s read:%d tol:%d ", i, testParam.intputFile, file, readSize, fileSize);
        usleep(15 * 1000);
    }

    free(nalbuf);
    pPlayer->stopThread();
    if (NULL != pPlayer) {
        AWStreamPlayer::destroy(pPlayer);
        pPlayer = NULL;
    }
    return 0;
}

int h265Test()
{
    ALOGD("\t ================================");
    ALOGD("\t ====     CedarC  Decoder    ====");
    ALOGD("\t ====        h265Test        ====");
    ALOGD("\t ================================");

    decodeParam.codecType = CODEC_H265;
    decodeParam.dispX = 212;
    decodeParam.dispY = 124;
    decodeParam.dispW = 600;
    decodeParam.dispH = 351;
    decodeParam.rotation = Angle_0;
    strcpy((char*)testParam.intputFile, INPUT_H265);

    int ret = 0;
    FILE* inFile = fopen(testParam.intputFile, "r");
    if (inFile == NULL) {
        ALOGD("open inFile fail");
        return -1;
    }

    char* inputBuf = NULL;
    int intputLen = 0;

    inputBuf = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen = DEFAULT_BUF_LEN;

    pPlayer = AWStreamPlayer::create(&decodeParam);
    pPlayer->startThread();

    int fileSize;
    fseek(inFile, 0L, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0L, SEEK_SET);

    int decodePos = 0;
    int readTmpLen = 0;
    int nalLen = 0;

    while (decodePos < fileSize) {
        //ALOGD("decode process[%d/%d]", decodePos,fileSize);
        readTmpLen = ((fileSize - decodePos) < intputLen) ? (fileSize - decodePos) : intputLen;
        fread(inputBuf, 1, readTmpLen, inFile);
        nalLen = 0;
        ret = find264NALFragment((char*)inputBuf, readTmpLen, &nalLen);
        if (nalLen < 4) {
            ALOGD("find264NALFragment fail ret:%d", ret);
            break;
        }

        //ALOGD("fmt_sgl[%s-%d]decode %d bytes.",pixelExt[decodeParam.pixelFormat],data++,nalLen);
        ret = pPlayer->sendFrameToThread((unsigned char*)inputBuf, nalLen);
        decodePos += nalLen;
        fseek(inFile, decodePos, SEEK_SET); //go to the real decode position.
        usleep(30 * 1000);
    }

    printfArgs(decodeParam);

    pPlayer->stopThread();
    AWStreamPlayer::destroy(pPlayer);

    free(inputBuf);

    if (inFile)
        fclose(inFile);
    return 0;
}

void mjpegTest()
{
    ALOGD("\t ================================");
    ALOGD("\t ====     CedarC  Decoder    ====");
    ALOGD("\t ====     mjpegDecodeTest    ====");
    ALOGD("\t ================================");

    int fileCount = 13;
    char inFilePath[60] = {0};
    int fileSize = 0;

    char* inputBuf = NULL;

    decodeParam.dispX = 0;
    decodeParam.dispW = 1024;
    decodeParam.dispH = 600;
    decodeParam.rotation = Angle_0;
    decodeParam.codecType = CODEC_MJPEG;
//    decodeParam.srcW = 1920;
//    decodeParam.srcH = 1080;
//    int dstW = 0;
//    int dstH = 0;
//    getExpectSize(&dstW,&dstH);
    FILE* inputFile = NULL;

    ALOGD("disp:(%d,%d)[%d:%d]", decodeParam.dispX, decodeParam.dispY, decodeParam.dispW, decodeParam.dispH);
    pPlayer = AWStreamPlayer::create(&decodeParam);
    pPlayer->startThread();

    for (int i = 1; i <= fileCount; i++) {

        sprintf(inFilePath, "%s%d.jpg", INPUT_JPG, i);

        inputFile = fopen(inFilePath, "rb");
        if (inputFile == NULL) {
            ALOGD("open nal fail[%d]:%s", i, inFilePath);
            continue;
        }

        ALOGD("open file:%s", inFilePath);
        fseek(inputFile, 0L, SEEK_END);
        fileSize = ftell(inputFile);
        fseek(inputFile, 0L, SEEK_SET);

        inputBuf = (char*)malloc(fileSize);
        fread(inputBuf, 1, fileSize, inputFile);
        fclose(inputFile);

        //decode
        pPlayer->sendFrameToThread((unsigned char*)inputBuf, fileSize);

        free(inputBuf);
        usleep(30 * 1000);
    }
    pPlayer->stopThread();

    AWStreamPlayer::destroy(pPlayer);

}

static void terminate(int sig_no)
{
    ALOGD("Got signal %d, exiting ...", sig_no);

    pPlayer->stopThread();

    AWStreamPlayer::destroy(pPlayer);
    system("cat /sys/class/disp/disp/attr/sys");
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

int main(int argc, char** argv)
{
    ALOGD("StreamPlayer test version:%s", MODULE_VERSION);
    /******** begin set the default encode param ********/
    memset(&decodeParam, 0, sizeof(decodeParam));
    memset(&testParam, 0, sizeof(testParam));

    decodeParam.srcW = 800;
    decodeParam.srcH = 480;
    decodeParam.dispW = SCREEN_W;
    decodeParam.dispH = SCREEN_H;

    decodeParam.tvOut = PAL;
    decodeParam.tvDispW = TV_W;
    decodeParam.tvDispH = TV_H(decodeParam.tvOut);

    decodeParam.rotation = Angle_0;
    decodeParam.scaleRatio = ScaleNone;
    decodeParam.codecType = CODEC_H264;
    decodeParam.zorder = 2;

    testParam.testTimes = 1;
    testParam.testWay = 0;

    /******** begin parse the config paramter ********/
    int i;
    if (argc >= 2) {
        for (i = 1; i < (int)argc; i += 2) {
            ParseArguType(&decodeParam, &testParam, argv[i], argv[i + 1]);
        }
    }

    for (int k = 1; k <= testParam.testTimes; k++) {
        if (testParam.testCmd == 1) {
            h264Test();
        } else if (testParam.testCmd == 2) {
            h265Test();
        } else if (testParam.testCmd == 3) {
            mjpegTest();
        } else {
            h264Test();
        }
        ALOGD("cmd:%d Test way:%d with %d times.", testParam.testCmd, testParam.testWay, k);
    }

    return 0;
}


