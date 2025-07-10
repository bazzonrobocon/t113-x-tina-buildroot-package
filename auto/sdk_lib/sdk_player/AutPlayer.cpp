#if 1

#define LOG_TAG "AutPlayer"
#include <sdklog.h>

#include "AutPlayer.h"
#include "soundControl_tinyalsa.h"

extern "C" LayerCtrl* LayerCreate();
//extern "C"  SoundCtrl* TinaSoundDeviceInit(int srates,int chnum,int alsa_format,char *devname);
//extern "C"  SoundCtrl* TinaSoundDeviceInit(audio_stream_type type);
//extern "C" int TinaSoundDeviceSetCallback(SoundCtrl* s, void *callback, void **pUserData);

extern  "C" Deinterlace* DeinterlaceCreate();

#if 0
extern  "C" LayerCtrl* LayerCreate_DE();

extern "C" int __LayerSetDisplayView(int nLeftOff, int nTopOff,
                                     int nDisplayWidth, int nDisplayHeight);
extern "C" int __LayerSetSrcCrop(int nLeftOff, int nTopOff,
                                 int nDisplayWidth, int nDisplayHeight);
extern "C" int __LayerGetCurBuffInfo(int *with, int*height, unsigned int* vir_y);
#else
extern LayerCtrl* LayerCreate_DE();

extern  int __LayerSetDisplayView(int nLeftOff, int nTopOff,
                                  int nDisplayWidth, int nDisplayHeight);
extern  int __LayerSetSrcCrop(int nLeftOff, int nTopOff,
                              int nDisplayWidth, int nDisplayHeight);
extern int __LayerGetCurBuffInfo(int *with, int*height, uint8_t** vir_y);
extern int set_corp_ctrl_by_user(int corp_ctrl);

#endif
using namespace android;

#define F_LOG ALOGV("%s, line: %d", __FUNCTION__, __LINE__);


int AUTPlayer::CallbackForAwPlayer(void* pUserData, int msg, int ext1, void* param)
{
    if (!pUserData) {
        ALOGE("AUTPlayer CallbackForAwPlayer error,pUserData=null\n");
        return -1;
    }

    AUTPlayer *p = (AUTPlayer*)pUserData;

    AUTPlayerContext* pDemoPlayer = (AUTPlayerContext*)&p->mPlayer;

    autCallback_func cbUser = p->cbUser;
    switch (msg) {
        case AWPLAYER_MEDIA_INFO: {
            switch (ext1) {
                case AW_MEDIA_INFO_NOT_SEEKABLE: {
                    pDemoPlayer->mSeekable = 0;
                    ALOGW("AUTPlayer info: media source is unseekable.\n");
                    if (cbUser != 0) {
                        cbUser(msg, p->datUser, 0, 0);
                    }
                    break;
                }
                case AW_MEDIA_INFO_RENDERING_START: {
                    printf("info: start to show pictures.\n");
                    break;
                }
            }
            break;
        }

        case AWPLAYER_MEDIA_ERROR: {
            ALOGW("AUTPlayer error: AWPLAYER_MEDIA_ERROR fail.\n");
            pthread_mutex_lock(&pDemoPlayer->mMutex);
            pDemoPlayer->mStatus = STATUS_STOPPED;
            pDemoPlayer->mPreStatus = STATUS_STOPPED;

            pthread_mutex_unlock(&pDemoPlayer->mMutex);
            ALOGW("AUTPlayer error: AWPLAYER_MEDIA_ERROR failxxxxx.\n");
            pDemoPlayer->mError = 1;
            if (cbUser != 0) {
                cbUser(msg, p->datUser, param, ext1);
            }
            ALOGW("AUTPlayer error: AWPLAYER_MEDIA_ERROR fail111111.\n");
            break;
        }

        case AWPLAYER_MEDIA_PREPARED: {
            //pthread_mutex_lock(&pDemoPlayer->mMutex);
            pDemoPlayer->mPreStatus = pDemoPlayer->mStatus;
            pDemoPlayer->mStatus = STATUS_PREPARED;
            ALOGW("AUTPlayer info: prepare ok.\n");
//      pthread_mutex_unlock(&pDemoPlayer->mMutex);
//      outFp = fopen("/mnt/UDISK/mb32.h264", "wb");
            if (cbUser != 0) {
                cbUser(msg, p->datUser, 0, 0);
            }

            break;
        }

        case AWPLAYER_MEDIA_BUFFERING_UPDATE: {
            //int nTotalPercentage   = ((int*)param)[0];     //* read positon to total file size.
            //int nBufferPercentage  = ((int*)param)[1];     //* cache buffer fullness.
            //int nLoadingPercentage = ((int*)param)[2];     //* loading percentage to start play.

            int nBufferedFilePos;
            int nBufferFullness;
            ALOGW("AUTPlayer info: buffer %d percent of the media file, buffer fullness = %d percent.\n",
                  nBufferedFilePos, nBufferFullness);
            if (cbUser != 0) {
                cbUser(msg, p->datUser, 0, 0);
            }
            break;
        }

        case AWPLAYER_MEDIA_PLAYBACK_COMPLETE: {
            //* stop the player.
            //* TODO
            ALOGW("AUTPlayer info: NOTIFY_PLAYBACK_COMPLETE.22\n");
            if (cbUser != 0) {
                cbUser(msg, p->datUser, 0, 0);
            }
            break;
        }
        case AWPLAYER_MEDIA_SEEK_COMPLETE: {
            pthread_mutex_lock(&pDemoPlayer->mMutex);
            pDemoPlayer->mStatus = pDemoPlayer->mPreStatus;
            ALOGW("AUTPlayer info: seek ok.\n");
            pthread_mutex_unlock(&pDemoPlayer->mMutex);
            if (cbUser != 0) {
                cbUser(msg, p->datUser, 0, 0);
            }

            break;
        }

        case AWPLAYER_MEDIA_SET_VIDEO_SIZE:
        case AWPLAYER_MEDIA_STARTED:
        case AWPLAYER_MEDIA_PAUSED:
        case AWPLAYER_MEDIA_STOPPED:
        case AWPLAYER_MEDIA_SKIPPED:
        case AWPLAYER_MEDIA_TIMED_TEXT:
        case AWPLAYER_MEDIA_SUBTITLE_DATA:
        case AWPLAYER_MEDIA_LOG_RECORDER:
        case AWPLAYER_EXTEND_MEDIA_INFO: {
            if (cbUser != 0) {
                cbUser(msg, p->datUser, param, ext1);
            }

            break;
        }
        default: {
            ALOGW("AUTPlayer -------warning: unknown callback msg %d from AwPlayer.\n", msg);
            if (cbUser != 0) {
                cbUser(msg, p->datUser, param, ext1);
            }
            break;
        }
    }

    return 0;
}

int AUTPlayer::setVolume(int l, int r)
{
    printf("%s is not available\n", __FUNCTION__);
    //TinaSoundDeviceSetVol(sound, l,r);
    return 0;
}
int AUTPlayer::getVolume(int *l, int *r)
{
    printf("%s is not available\n", __FUNCTION__);
    //TinaSoundDeviceGetVol(sound, l,r);
    return 0;
}

int  AUTPlayer::setsubtitleUrl(char *suburl)
{
    ALOGV("AUTPlayer setsubtitleUrl\n");
    if ((!suburl) || (!mPlayer.mAwPlayer)) {
        ALOGE("SsetsubtitleUrl,suburl or mPlayer.mAwPlayer is null\n");
        return -1;
    }

    int ret = XPlayerSetExternalSubUrl(mPlayer.mAwPlayer, suburl);
    if (ret < 0) {
        ALOGE("Set extSub error\n");
        return -1;
    } else {
        return 0;
    }
}

int AUTPlayer::setsubtitleUrlFd(char *suburl, char *idxurl)
{
    int ret;
    int64_t nOffset = 0;
    int64_t nLength = 0;

    if ((suburl == NULL) || (idxurl == NULL)) {
        printf("suburl or idxurl == NULL\n");
        return -1;
    }
    ALOGV("AUTPlayer setsubtitleUrlFd\n");
    FILE *fdSub = fopen(suburl, "rb");
    if (!fdSub) {
        printf("suburl=%s\nfdSub open error %d\n", suburl, errno);
        return -1;
    }

    FILE *fd = fopen(idxurl, "rb");
    if (!fd) {
        fclose(fdSub);
        printf("idxurl=%s\nsub idx fd open error %d\n", idxurl, errno);
        return -1;
    }

    fseek(fd, 0, SEEK_END);
    nLength = ftell(fd);
    rewind(fd);

    if (!mPlayer.mAwPlayer) {
        ALOGE("setsubtitleUrlFd error,mPlayer.mAwPlayer=null\n");
        fclose(fdSub);
        fclose(fd);
        return -1;
    }

    ret = XPlayerSetExternalSubFd(mPlayer.mAwPlayer, fileno(fd), nOffset, nLength, fileno(fdSub));
    if (ret < 0) {
        ALOGE("XPlayerSetExternalSubFd error\n");
        fclose(fdSub);
        fclose(fd);
        return -1;
    }
    return 0;
}

int AUTPlayer::switchSubtitle(int index)
{
    if (!mPlayer.mAwPlayer) {
        ALOGE("switchSubtitle error,mPlayer.mAwPlayer=null\n");
        return -1;
    }

    if (index < 0) {
        ALOGE("switchSubtitle input err\n");
    }
    int ret = XPlayerSwitchSubtitle(mPlayer.mAwPlayer, index);
    return ret;
}

int AUTPlayer::switchAudio(int index)
{
    if (index < 0) {
        printf("switchAudio input err\n");
    }

    return XPlayerSwitchAudio(mPlayer.mAwPlayer, index);
}

static int SubCallbackProcess(void* pUser, int eMessageId, void* param)
{
    if (!pUser) {
        ALOGE("SubCallbackProcess error,pUser=null\n");
        return -1;
    }

    AUTPlayer* p = (AUTPlayer*)pUser;
    //int msg = eMessageId;

    unsigned int nSubtitleId = (unsigned int)((uintptr_t*)param)[0];
    unsigned int pItem = (unsigned int)((uintptr_t*)param)[1];
    if (p->cbUser != 0) {
        p->cbUser(eMessageId, p->datUser, (void*)pItem, nSubtitleId);
    }
    return 0;

}

AUTPlayer::AUTPlayer(SoundCtrl* sndCtrl, LayerCtrl* layerCtrl, SubCtrl* subCtrl):
    cbUser(NULL), mSampRates(44100), mAudioChNum(2), mAlsaFormat(0)
{
    //* create a player.
    memset(&mPlayer, 0, sizeof(AUTPlayerContext));
    pthread_mutex_init(&mPlayer.mMutex, NULL);
    mPlayer.mAwPlayer = XPlayerCreate();//new AwPlayer();
    if (mPlayer.mAwPlayer == NULL) {
        ALOGE("AUTPlayer can not create AwPlayer, quit.\n");
        return ;
    }
    memset(mSndDevName, 0, 64);
    strcpy(mSndDevName, "default");

    //* set callback to player.
    XPlayerSetNotifyCallback(mPlayer.mAwPlayer, CallbackForAwPlayer, (void*)&mPlayer);

    //* check if the player work.
    if (XPlayerInitCheck(mPlayer.mAwPlayer) != 0) {
        ALOGE("AUTPlayer initCheck of the player fail, quit.\n");
        XPlayerDestroy(mPlayer.mAwPlayer);
        mPlayer.mAwPlayer = NULL;

    }

    if (sndCtrl == NULL) {
        mSoundCtrl = SoundDeviceCreate();
        tinyalsaSetCallback(mSoundCtrl, &cbUser, &datUser);
    } else {
        mSoundCtrl = sndCtrl;
    }
    if (!mSoundCtrl) {
        ALOGE("SoundDeviceCreate failed.3\n");
    } else {
        XPlayerSetAudioSink(mPlayer.mAwPlayer, mSoundCtrl);
    }

    if (layerCtrl == NULL) {
        mLayerCtrl = LayerCreate_DE();
    } else {
        mLayerCtrl = layerCtrl;
    }
    if (!mLayerCtrl) {
        ALOGE("LayerCreate_DE failed.3\n");
    } else {
        XPlayerSetVideoSurfaceTexture(mPlayer.mAwPlayer, mLayerCtrl);
    }

    if (subCtrl == NULL) {
        mSubCtrl = SubtitleCreate(SubCallbackProcess, this);
    } else {
        mSubCtrl = subCtrl;
    }
    if (!mSubCtrl) {
        ALOGE("SubtitleCreate failed.3\n");
    } else {
        XPlayerSetSubCtrl(mPlayer.mAwPlayer, mSubCtrl);
    }

    Deinterlace* di = DeinterlaceCreate();
    XPlayerSetDeinterlace(mPlayer.mAwPlayer, di);

    mcd = HwDisplay::getInstance();

}

AUTPlayer::~AUTPlayer()
{
    ALOGV("AUTPlayer destroy AwPlayer.\n");

    if (mPlayer.mAwPlayer != NULL) {
        XPlayerDestroy(mPlayer.mAwPlayer);
        mPlayer.mAwPlayer = NULL;
    }

    ALOGV("AUTPlayer destroy AwPlayer 1.\n");
    pthread_mutex_destroy(&mPlayer.mMutex);

}
int AUTPlayer::setUrl(char *url)
{
    int ret = -1;
    char* pUrl;
    pUrl = (char*)(uintptr_t)url;
    ALOGV("AUTPlayer setUrl.\n");
    if (mPlayer.mStatus != STATUS_STOPPED) {
        printf("AUTPlayer invalid command:\n");
        printf("AUTPlayer    play is not in stopped status.\n");
        return ret;
    }
    ALOGV("AUTPlayer setUrl 1.\n");
#if 0
    if (XPlayerReset(mPlayer.mAwPlayer) != 0) {
        XPlayerReset(mPlayer.mAwPlayer);
        printf("AUTPlayer error:\n");
        printf("AUTPlayer    AwPlayer::reset() return fail.\n");
        return -1;
    }
#endif
    //* clear at the NOTIFY_NOT_SEEKABLE callback.

    //* set url to the AwPlayer.
    if (XPlayerSetDataSourceUrl(mPlayer.mAwPlayer,
                                (const char*)pUrl, NULL, NULL) != 0) {
        ALOGE("AUTPlayer error:\n");
        ALOGE("AUTPlayer AwPlayer::setDataSource() return fail.\n");
        return ret;
    }

    //* start preparing.
    pthread_mutex_lock(&mPlayer.mMutex);    //* lock to sync with the prepare finish notify.
    //if(mPlayer.mAwPlayer->prepare() != 0)
    mPlayer.mPreStatus = STATUS_STOPPED;
    //mPlayer.mStatus    = STATUS_PREPARED;
    mPlayer.mStatus    = STATUS_PREPARING;

    if (XPlayerPrepareAsync(mPlayer.mAwPlayer) != 0) {
        ALOGE("AUTPlayer error:\n");
        ALOGE("AUTPlayer    AwPlayer::prepare() return fail.\n");
        pthread_mutex_unlock(&mPlayer.mMutex);
        return -1;
    }
    ALOGV("AUTPlayer preparing...\n");
    pthread_mutex_unlock(&mPlayer.mMutex);


    MediaInfo* mi = XPlayerGetMediaInfo(mPlayer.mAwPlayer);
    if (mi == NULL) {
        ALOGE("GetMediaInfo error\n");
    } else {
        ALOGD("setUrl bSeekable = %d\n", mi->bSeekable);
        mPlayer.mSeekable = mi->bSeekable;

        if (NULL == mi->pVideoStreamInfo) {
            ALOGV("setUrl: it should be a music file\n");
        } else {
            ALOGE("setUrl __LayerSetSrcCrop w=%d,h=%d\n ", mi->pVideoStreamInfo->nWidth, mi->pVideoStreamInfo->nHeight);
            __LayerSetSrcCrop(0, 0, mi->pVideoStreamInfo->nWidth, mi->pVideoStreamInfo->nHeight);
        }
    }
    ret = 0;
    return ret;
}

int AUTPlayer::play()
{
    int ret = -1;
    ALOGV("AUTPlayer play.\n");

    if (mPlayer.mStatus != STATUS_PREPARED &&
        mPlayer.mStatus != STATUS_SEEKING &&
        mPlayer.mStatus != STATUS_PAUSED) {
        ALOGE("AUTPlayer invalid command:\n");
        ALOGE("AUTPlayer play eris neither in prepared status nor in seeking.\n");
        return -1;
    }
    //* start the playback
    if (mPlayer.mStatus != STATUS_SEEKING) {
        if (XPlayerStart(mPlayer.mAwPlayer) != 0) {
            ALOGE("AUTPlayer error:\n");
            ALOGE("AUTPlayer    AwPlayer::start() return fail.\n");
            return ret;
        }
        mPlayer.mPreStatus = mPlayer.mStatus;
        mPlayer.mStatus    = STATUS_PLAYING;
        ALOGV("AUTPlayerplaying.\n");
    } else {
        //* the player will keep the started status and start to play after seek finish.
        if (XPlayerStart(mPlayer.mAwPlayer) != 0) {}
        mPlayer.mPreStatus = STATUS_PLAYING; //* current status is seeking, will set
        //* to mPreStatus when seek finish callback.
    }
    ALOGV("AUTPlayer play -----.\n");
    ret = 0;
//err_end:
    return ret;
}
int AUTPlayer::stop()
{
    int ret = 0;
    ALOGV("AUTPlayer stop.\n");

    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer stop: mPlayer.mAwPlayer is null,return now");
        return -1;
    }

    if (XPlayerReset(mPlayer.mAwPlayer) != 0) {
        ALOGE("AUTPlayer error:\n");
        ALOGE("AUTPlayer    AwPlayer::stop() return fail.\n");
        //return ret;
    }

    printf("AUTPlayerstopped 2.\n");
    mPlayer.mPreStatus = mPlayer.mStatus;
    mPlayer.mStatus    = STATUS_STOPPED;

    ALOGV("AUTPlayerstopped.\n");
    return ret;
}
int AUTPlayer::pause()
{
    int ret = -1;
    ALOGV("AUTPlayer pause.\n");

    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer pause: mPlayer.mAwPlayer is null,return now");
        return -1;
    }

    if (mPlayer.mStatus != STATUS_PLAYING &&
        mPlayer.mStatus != STATUS_SEEKING &&
        mPlayer.mStatus != STATUS_PAUSED
       ) {
        ALOGE("AUTPlayer pause invalid command.status is %d\n", mPlayer.mStatus);
        ALOGE("AUTPlayer player is neither in playing status nor in seeking status.\n");
        return ret;
    }

    //* pause the playback

    if (mPlayer.mStatus != STATUS_SEEKING) {

        if (mPlayer.mStatus == STATUS_PAUSED) {
            if (XPlayerStart(mPlayer.mAwPlayer) != 0) {
                ALOGE("AUTPlayer error:\n");
                ALOGE("AUTPlayer    AwPlayer::start() return fail.\n");
                return ret;
            }
            mPlayer.mPreStatus = mPlayer.mStatus;
            mPlayer.mStatus    = STATUS_PLAYING;
            ALOGV("AUTPlayer pause to playing.\n");
        } else {
            if (XPlayerPause(mPlayer.mAwPlayer) != 0) {
                ALOGE("AUTPlayer error:\n");
                ALOGE("AUTPlayer    AwPlayer::pause() return fail.\n");
                return ret;
            }
            mPlayer.mPreStatus = mPlayer.mStatus;
            mPlayer.mStatus    = STATUS_PAUSED;
            ALOGV("AUTPlayerpaused.\n");
        }
    } else {
        //* the player will keep the pauded status and pause the playback after seek finish.
        if (XPlayerPause(mPlayer.mAwPlayer) != 0) {
            ALOGE("AUTPlayer    AwPlayer::pause() return fail when seeking.\n");
        }
        mPlayer.mPreStatus = STATUS_PAUSED;  //* current status is seeking, will set
        ALOGV("AUTPlayerpause when seeking.\n");
        //* to mPreStatus when seek finish callback.
    }
    ALOGV("AUTPlayerpaused done .\n");
    ret = 0;
    return ret;
}

int AUTPlayer::setSpeed(int nSpeed)
{
    if (mPlayer.mAwPlayer == NULL) {
        printf("mPlayer.mAwPlayer=NULL,setSpeed error\n");
        return -1;
    }
    XPlayerSetSpeed(mPlayer.mAwPlayer, nSpeed);

    //* the player will keep the pauded status and pause the playback after seek finish.
    pthread_mutex_lock(&mPlayer.mMutex);    //* sync with the seek finish callback.
    mPlayer.mPreStatus = STATUS_PLAYING;
    pthread_mutex_unlock(&mPlayer.mMutex);

    return 0;
}

int AUTPlayer::seekto(int sec)
{
    int ret = -1;
    ALOGV("AUTPlayer seekto.\n");
    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer seekto: mPlayer.mAwPlayer is null,return now");
        return -1;
    }
    int nSeekTimeMs;
    int nDuration;
    nSeekTimeMs = sec * 1000;

    if (mPlayer.mStatus != STATUS_PLAYING &&
        mPlayer.mStatus != STATUS_SEEKING &&
        mPlayer.mStatus != STATUS_PAUSED  &&
        mPlayer.mStatus != STATUS_PREPARED) {
        ALOGE("AUTPlayer invalid command:\n");
        ALOGE("  seekto:  player is not in playing/seeking/paused/prepared status.\n");
        return ret;
    }

    if (XPlayerGetDuration(mPlayer.mAwPlayer, &nDuration) != 0) {
        ALOGE("AUTPlayer seek get duration fail.\n");
        return ret;

    }
    if (nSeekTimeMs > nDuration) {
        ALOGE("AUTPlayer seek time out of range, media duration = %u seconds.\n", nDuration / 1000);
        return ret;
    }

    if (mPlayer.mSeekable == 0) {
        ALOGE("AUTPlayer media source is unseekable.\n");
        return ret;
    }

    //* the player will keep the pauded status and pause the playback after seek finish.
    pthread_mutex_lock(&mPlayer.mMutex);    //* sync with the seek finish callback.
    XPlayerSeekTo(mPlayer.mAwPlayer, nSeekTimeMs, AW_SEEK_PREVIOUS_SYNC);
    if (mPlayer.mStatus != STATUS_SEEKING)
        mPlayer.mPreStatus = mPlayer.mStatus;
    mPlayer.mStatus = STATUS_SEEKING;
    pthread_mutex_unlock(&mPlayer.mMutex);
    ret = 0;
    return ret;
}




MediaInfo * AUTPlayer::getMediaInfo()
{
    ALOGV("AUTPlayershow media information.xxxxx\n");
    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer getMediaInfo: mPlayer.mAwPlayer is null,return now");
        return NULL;
    }

    MediaInfo *pmedia = XPlayerGetMediaInfo(mPlayer.mAwPlayer); //mPlayer.mAwPlayer->getMediaInfo();

    return pmedia;
}

int AUTPlayer::getDuration()

{
    int nDuration = 0;
#if 0
    MediaInfo *pmedia = XPlayerGetMediaInfo(mPlayer.mAwPlayer); //mPlayer.mAwPlayer->getMediaInfo();
    if (pmedia != NULL) {
        nDuration = pmedia->nDurationMs;
        return nDuration;
    } else {
        ALOGE("AUTPlayer fail to get media duration.\n");
        return -1;

    }
#endif
    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer getDuration: mPlayer.mAwPlayer is null,return now");
        return -1;
    }

    if (XPlayerGetDuration(mPlayer.mAwPlayer, &nDuration) == 0) {
        ALOGV("AUTPlayermedia duration = %u seconds.\n", nDuration / 1000);
        return nDuration;
    } else {
        ALOGE("AUTPlayer fail to get media duration.\n");
        return -1;
    }
    return 0;
}
int AUTPlayer::getCurrentPosition()
{
    int nPosition = 0;
    if (!mPlayer.mAwPlayer) {
        ALOGE("AUTPlayer fail to get pisition.mAwPlayer=null\n");
        return -1;
    }
    if (XPlayerGetCurrentPosition(mPlayer.mAwPlayer, &nPosition) == 0) {
        ALOGV("AUTPlayer current position = %u seconds.\n", nPosition / 1000);
        return nPosition;
    } else {
        ALOGE("AUTPlayer fail to get pisition.\n");
        return -1;
    }
}



int  AUTPlayer::setViewWindow(int x, int y, int w, int h)
{
    vv = {x, y, w, h};
    __LayerSetDisplayView(x, y, w, h);
    return 0;
}

int  AUTPlayer::setVideoSrcCrop(int x, int y, int w, int h)
{
    src_crop = {x, y, w, h};
    set_corp_ctrl_by_user(1);
    __LayerSetSrcCrop(x, y, w, h);
    return 0;
}

int  AUTPlayer::getCurBuffInfo(int* with, int* height, uint8_t** vir_y)
{
    return __LayerGetCurBuffInfo(with, height, vir_y);
}

int  AUTPlayer::getDispwidth()
{
    return mcd->lcdxres;
}

int  AUTPlayer::getDispheigth()
{
    return mcd->lcdyres;
}
#endif
