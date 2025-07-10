#define LOG_TAG "autplayer_test"
#include "sdklog.h"

#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "AutPlayer.h"

using namespace android;

#define PCM_SAVE_PATH "/tmp/autplayer/out.pcm"

static int gExitFlag;
static int gComplited = 0;

void printUsage()
{
    printf("================================Usage================================\n");
    printf("autplayer_test <testcase> <mediafile>\n");
    printf("autplayer_test 0 /tmp/autplayer/test.mp4	  means: start play test.mp4, and pause\n");
    printf("autplayer_test 1 /tmp/autplayer/test.mp4	  means: start play test.mp4, and print MediaInfo\n");
    printf("autplayer_test 3 /tmp/autplayer/test.mp4	  means: start play test.mp4, and seekto to 20\n");
    printf("autplayer_test 4 /tmp/autplayer/test.mp4	  means: start play test.mp4, and setSpeed to 2\n");
    printf("================================Usage================================\n");
}

static int saveToFile(char *str, void *p, int length)
{
    FILE *fd;
    fd = fopen(str, "a");

    if (!fd) {
        ALOGD("Open file error");
        return -1;
    }
    if (fwrite(p, 1, length, fd)) {
        fclose(fd);
        return 0;
    } else {
        ALOGD("Write file fail");
        fclose(fd);
        return -1;
    }
    return 0;
}

int cb_func(int32_t msgType, void *user, void*data, int len)
{
    //ALOGD("%s msgType=%d",__FUNCTION__,msgType);
    switch (msgType) {
        case 1:
            //ALOGD("");
            break;
        case 14:
            //ALOGD("");
            break;
        case 28:
            //ALOGD("");
            break;
        case 128:
            static int cnt = 0;
            char buf[64];
            cnt++;
            sprintf(buf, PCM_SAVE_PATH);
            saveToFile(buf, data, len);
            break;
        case AWPLAYER_MEDIA_PLAYBACK_COMPLETE:
            gComplited = 1;
            ALOGD("%s,gComplited=%d ", __FUNCTION__, gComplited);
            break;
        default :
            ALOGD("%s,msgType=%d is not handle", __FUNCTION__, msgType);
            break;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    ALOGD("autplayerTest version:%s", MODULE_VERSION);

    int ret = -1;
    gExitFlag = 0;

    int cmd = 0;
    char url[64];
    memset(url, 0, sizeof(url));

    if (argc == 3) {
        cmd = atoi(argv[1]);
        strcpy(url, argv[2]);
    } else {
        printUsage();
        exit(0);
    }
    ALOGD("set url:%s", url);

    AUTPlayer* player = new AUTPlayer();

    player->setUserCb(cb_func, NULL);
    ret = player->setUrl(url);
    if (ret != 0) {
        ALOGD("set url errror");
        return -1;
    }

    switch (cmd) {
        case 0: {
            ret = player->play();
            if (ret != 0) {
                ALOGD("call play errror");
                return -1;
            }
            sleep(1);
            ret = player->pause();
            if (ret != 0) {
                ALOGD("call play errror");
                return -1;
            }
            ALOGD("autplayer pause...");
            sleep(2);
            ALOGD("autplayer play again");
            ret = player->play();
            if (ret != 0) {
                ALOGD("call play errror");
                return -1;
            }
            break;
        }
        case 1: {
            MediaInfo * m = player->getMediaInfo();
            ALOGD("nVideoStreamNum=%d nAudioStreamNum=%d,nSubtitleStreamNum=%d"
                  "nDurationMs=%d",
                  m->nVideoStreamNum,
                  m->nAudioStreamNum,
                  m->nSubtitleStreamNum,
                  m->nDurationMs);
            if (m->nAudioStreamNum) {
                ALOGD("nSampleRate=%d nBitsPerSample=%d",
                      m->pAudioStreamInfo->nSampleRate,
                      m->pAudioStreamInfo->nBitsPerSample);
            }

            player->play();
            ALOGD("getDuration=%d", player->getDuration());
            sleep(1);
            player->pause();
            ALOGD("CurrentPosition=%d", player->getCurrentPosition());
            sleep(1);
            player->pause();
            break;
        }
        case 2: {
            player->seekto(22);
            player->play();
            sleep(1);
            player->pause();
            sleep(1);
            player->play();
            break;
        }
        case 3: {
            player->setSpeed(2);
            player->play();
            player->pause();
            player->play();
            break;
        }
        default : {
            break;
        }
    }
    while (!gExitFlag) {
        sleep(1);
        if (gComplited) {
            player->play();
            gComplited = 0;
        }
    }
    return 0;
}
