#define LOG_TAG "TinaSoundControl"
#include <sdklog.h>

#include "tinasoundcontrol.h"
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
//#include <allwinner/auGaincom.h>
#include "cdx_log.h"
//===========volume control declaration start================
#include<sys/types.h>
#include<unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MIXER_CARD 0
#define VOL_CURVE_STEP 5

const char * const pcm_stream_table[STREAM_MAX] = {
    "plug:softvol-btcall",
    "plug:softvol-navi",
    "plug:softvol-system",
    "plug:softvol-music",
    "plug:softvol-btmusic",
    "plug:softvol-music",
};

const char * const pcm_control_table[STREAM_MAX] = {
    "PCM bt call volume",
    "PCM navi volume",
    "PCM system volume",
    "PCM music volume",
    "PCM bt music volume",
    "PCM music volume",
};

#define STREAM_BTCALL_VAL_MAX 32
#define STREAM_NAVI_VAL_MAX 32
#define STREAM_SYSTEM_VAL_MAX 32
#define STREAM_MUSIC_VAL_MAX 32
#define STREAM_BTMUSIC_VAL_MAX 32
#define STREAM_DEFAULT_VAL_MAX  32


static int stram_volume_max[STREAM_MAX] = {
    STREAM_BTCALL_VAL_MAX,      // STREAM_BTCALL
    STREAM_NAVI_VAL_MAX,      //STREAM_NAVI
    STREAM_SYSTEM_VAL_MAX,      //STREAM_SYSTEM
    STREAM_MUSIC_VAL_MAX,      //STREAM_MUSIC
    STREAM_BTMUSIC_VAL_MAX,      //STREAM_BTMUSIC
    STREAM_DEFAULT_VAL_MAX,      //STREAM_DEFAULT
};

static int stram_volume_default[STREAM_MAX] = {
    STREAM_BTCALL_VAL_MAX,      // STREAM_BTCALL
    STREAM_NAVI_VAL_MAX,      //STREAM_NAVI
    STREAM_SYSTEM_VAL_MAX,      //STREAM_SYSTEM
    STREAM_MUSIC_VAL_MAX,      //STREAM_MUSIC
    STREAM_BTMUSIC_VAL_MAX,      //STREAM_BTMUSIC
    STREAM_DEFAULT_VAL_MAX,      //STREAM_DEFAULT
};

static int volumeCurves[STREAM_MAX][VOL_CURVE_STEP][2] = {
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_BTCALL_VAL_MAX, 8}},      // STREAM_BTCALL
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_NAVI_VAL_MAX, 8}},      //STREAM_NAVI
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_SYSTEM_VAL_MAX, 8}},      //STREAM_SYSTEM
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_MUSIC_VAL_MAX, 8}},      //STREAM_MUSIC
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_BTMUSIC_VAL_MAX, 8}},      //STREAM_BTMUSIC
    {{0, 0}, {5, 20}, {10, 12}, {20, 10}, {STREAM_DEFAULT_VAL_MAX, 8}},      //STREAM_DEFAULT
};

static void open_shm(SoundCtrlContext* sc);
static void set_shm(SoundCtrlContext* sc, int play);
static int get_shm(SoundCtrlContext* sc, audio_stream_type stream_type);
static void close_shm(SoundCtrlContext* sc);
static int tina_mixer_get_control(SoundCtrlContext* sc, audio_stream_type type);
static void tina_mixer_close(SoundCtrlContext* sc);
static void tina_mixer_set_control_value(SoundCtrlContext* sc, audio_stream_type type, int left, int right);
static void TinaSoundDeviceVolCtl(SoundCtrlContext* sc, int isplaying);
//===========volume control declaration end================


int BLOCK_MODE = 0;
int NON_BLOCK_MODE = 1;
//static int openSoundDevice(SoundCtrlContext* sc, int isOpenForPlay ,int mode);
static int closeSoundDevice(SoundCtrlContext* sc);
static int setSoundDeviceParams(SoundCtrlContext* sc);


static SoundControlOpsT mSoundControlOps = {
    .destroy          =   TinaSoundDeviceDestroy,
    .setFormat        =   TinaSoundDeviceSetFormat,
    .start            =   TinaSoundDeviceStart,
    .stop             =   TinaSoundDeviceStop,
    .pause            =   TinaSoundDevicePause,
    .write            =   TinaSoundDeviceWrite,
    .reset            =   TinaSoundDeviceReset,
    .getCachedTime    =   TinaSoundDeviceGetCachedTime,
    .getFrameCount    =   TinaSoundDeviceGetFrameCount,
    .setPlaybackRate  =   TinaSoundDeviceSetPlaybackRate,
    .control          =   TinaSoundDeviceControl
};

static int openSoundDevice(SoundCtrlContext* sc, int mode)
{
    int ret = 0;
    const char *name = NULL;
    audio_stream_type type = sc->stream_type;

    if (type < 0 || type >= STREAM_MAX) {
        printf("audio stream type out of range, reset to default");
        type = sc->stream_type = STREAM_DEFAULT;
    }

    name = pcm_stream_table[type];

    printf("openSoundDevice() in %s style\n", name);
    assert(sc);
    if (!sc->alsa_handler) {
        //if((ret = snd_pcm_open(&sc->alsa_handler, "default",SND_PCM_STREAM_PLAYBACK ,mode))<0){
        //if use dimix func,use the below parms open the sound card
        if ((ret = snd_pcm_open(&sc->alsa_handler, name, SND_PCM_STREAM_PLAYBACK, mode)) < 0) {
            loge("open audio device failed:%s, errno = %d", strerror(errno), errno);
            if (errno == 16) { //the device is busy,sleep 2 second and try again
                sleep(2);
                //if((ret = snd_pcm_open(&sc->alsa_handler, "default",SND_PCM_STREAM_PLAYBACK ,mode))<0){
                //if use dimix func,use the below parms open the sound card
                if ((ret = snd_pcm_open(&sc->alsa_handler, name, SND_PCM_STREAM_PLAYBACK, mode)) < 0) {
                    loge("second open audio device failed:%s, errno = %d", strerror(errno), errno);
                }
            }
        }
    } else {
        logd("the audio device has been opened");
    }
    return ret;
}

static int closeSoundDevice(SoundCtrlContext* sc)
{
    int ret = 0;
    logd("closeSoundDevice()");
    assert(sc);
    if (sc->alsa_handler) {
        if ((ret = snd_pcm_close(sc->alsa_handler)) < 0) {
            loge("snd_pcm_close failed:%s", strerror(errno));
        } else {
            sc->alsa_handler = NULL;
            logd("alsa-uninit: pcm closed");
        }
    }
    return ret;
}

static int setSoundDeviceParams(SoundCtrlContext* sc)
{
    int ret = 0;
    logd("setSoundDeviceParams()");
    assert(sc);
    sc->bytes_per_sample = snd_pcm_format_physical_width(sc->alsa_format) / 8;
    sc->bytes_per_sample *= sc->nChannelNum;
    sc->alsa_fragcount = 8;
    sc->chunk_size = 1024;
    if ((ret = snd_pcm_hw_params_malloc(&sc->alsa_hwparams)) < 0) {
        loge("snd_pcm_hw_params_malloc failed:%s", strerror(errno));
        return ret;
    }

    if ((ret = snd_pcm_hw_params_any(sc->alsa_handler, sc->alsa_hwparams)) < 0) {
        loge("snd_pcm_hw_params_any failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_access(sc->alsa_handler, sc->alsa_hwparams,
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        loge("snd_pcm_hw_params_set_access failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_test_format(sc->alsa_handler, sc->alsa_hwparams,
                                             sc->alsa_format)) < 0) {
        loge("snd_pcm_hw_params_test_format fail , MSGTR_AO_ALSA_FormatNotSupportedByHardware");
        sc->alsa_format = SND_PCM_FORMAT_S16_LE;
    }

    if ((ret = snd_pcm_hw_params_set_format(sc->alsa_handler, sc->alsa_hwparams,
                                            sc->alsa_format)) < 0) {
        loge("snd_pcm_hw_params_set_format failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_channels_near(sc->alsa_handler,
                                                   sc->alsa_hwparams, &sc->nChannelNum)) < 0) {
        loge("snd_pcm_hw_params_set_channels_near failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_rate_near(sc->alsa_handler, sc->alsa_hwparams,
                                               &sc->nSampleRate, NULL)) < 0) {
        loge("snd_pcm_hw_params_set_rate_near failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_period_size_near(sc->alsa_handler,
                                                      sc->alsa_hwparams, &sc->chunk_size, NULL)) < 0) {
        loge("snd_pcm_hw_params_set_period_size_near fail , MSGTR_AO_ALSA_UnableToSetPeriodSize");
        goto SET_PAR_ERR;
    } else {
        logd("alsa-init: chunksize set to %ld", sc->chunk_size);
    }
    if ((ret = snd_pcm_hw_params_set_periods_near(sc->alsa_handler,
                                                  sc->alsa_hwparams, (unsigned int*)&sc->alsa_fragcount, NULL)) < 0) {
        loge("snd_pcm_hw_params_set_periods_near fail , MSGTR_AO_ALSA_UnableToSetPeriods");
        goto SET_PAR_ERR;
    } else {
        logd("alsa-init: fragcount=%d", sc->alsa_fragcount);
    }

    if ((ret = snd_pcm_hw_params(sc->alsa_handler, sc->alsa_hwparams)) < 0) {
        loge("snd_pcm_hw_params failed:%s", strerror(errno));
        goto SET_PAR_ERR;
    }

    sc->alsa_can_pause = snd_pcm_hw_params_can_pause(sc->alsa_hwparams);

    logd("setSoundDeviceParams():sc->alsa_can_pause = %d", sc->alsa_can_pause);
SET_PAR_ERR:
    snd_pcm_hw_params_free(sc->alsa_hwparams);

    return ret;

}

SoundCtrl* TinaSoundDeviceInit(audio_stream_type type)
{
    int i = 0;
    SoundCtrlContext* s;
    s = (SoundCtrlContext*)malloc(sizeof(SoundCtrlContext));
    printf("TinaSoundDeviceInit(), type = %d\n", type);
    if (s == NULL) {
        loge("malloc SoundCtrlContext fail.");
        return NULL;
    }
    memset(s, 0, sizeof(SoundCtrlContext));
    s->base.ops = &mSoundControlOps;
    s->alsa_access_type = SND_PCM_ACCESS_RW_INTERLEAVED;
    s->nSampleRate = 48000;
    s->nChannelNum = 2;
    s->alsa_format = SND_PCM_FORMAT_S16_LE;
    s->alsa_can_pause = 0;
    s->sound_status = STATUS_STOP;
    s->mVolume = 0;
    if (type < 0 || type >= STREAM_MAX) {
        printf("invalid stream type, use STREAM_DEFAULT as default\n");
        s->stream_type = STREAM_DEFAULT;
    } else {
        s->stream_type = type;
    }
    s->control_type = ST_NULL;

    //TODO:use stored user's volume to init this value
    s->vol_left = stram_volume_default[s->stream_type];
    s->vol_right = stram_volume_default[s->stream_type];
    s->mixer = NULL;
    s->p_map = NULL;
    for (i = 0; i < STREAM_MAX; i++) {
        s->ctl[i] = NULL;
    }
    if (tina_mixer_get_control(s, s->stream_type) <= 0) {
        openSoundDevice(s, BLOCK_MODE);
        usleep(10000);
        closeSoundDevice(s);
        tina_mixer_close(s);
    }
    tina_mixer_set_control_value(s, s->stream_type, s->vol_left, s->vol_right);
    open_shm(s);
    pthread_mutex_init(&s->mutex, NULL);
    return (SoundCtrl*)&s->base;
}
#if 0
SoundCtrl* TinaSoundDeviceInit(int srates, int chnum, int alsa_format, char *devname)
{
    SoundCtrlContext* s;
    s = (SoundCtrlContext*)malloc(sizeof(SoundCtrlContext));
    logd("TinaSoundDeviceInit(),s->nSampleRate=%d", srates);
    if (s == NULL) {
        loge("malloc SoundCtrlContext fail.");
        return NULL;
    }
    memset(s, 0, sizeof(SoundCtrlContext));
    s->base.ops = &mSoundControlOps;
    s->alsa_access_type = SND_PCM_ACCESS_RW_INTERLEAVED;
    s->nSampleRate = srates;
    s->nChannelNum = chnum;
    s->alsa_format = alsa_format;
    s->alsa_can_pause = 0;
    s->sound_status = STATUS_STOP;
    s->mVolume = 0;
    strcpy(s->mSoundDevName, devname);
    pthread_mutex_init(&s->mutex, NULL);
    return (SoundCtrl*)&s->base;
}
#endif

void TinaSoundDeviceDestroy(SoundCtrl* s)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    pthread_mutex_lock(&sc->mutex);
    logd("TinaSoundDeviceRelease()");
    if (sc->sound_status != STATUS_STOP) {
        closeSoundDevice(sc);
    }
    tina_mixer_close(sc);
    set_shm(sc, 0);
    close_shm(sc);
    pthread_mutex_unlock(&sc->mutex);
    pthread_mutex_destroy(&sc->mutex);
    free(sc);
    sc = NULL;
}

void TinaSoundDeviceSetFormat(SoundCtrl* s, CdxPlaybkCfg* cfg)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    pthread_mutex_lock(&sc->mutex);
    logd("TinaSoundDeviceSetFormat(),sc->sound_status == %d", sc->sound_status);
    if (sc->sound_status == STATUS_STOP) {
        logd("TinaSoundDeviceSetFormat()");
        sc->nSampleRate = cfg->nSamplerate;
        sc->nChannelNum = cfg->nChannels;
        sc->alsa_format = SND_PCM_FORMAT_S16_LE; //cfg->nBitpersample;//
        sc->bytes_per_sample = snd_pcm_format_physical_width(sc->alsa_format) / 8;
        sc->bytes_per_sample *= sc->nChannelNum;
        ALOGW("TinaSoundDeviceSetFormat()>>>sample_rate:%d,channel_num:%d,sc->bytes_per_sample:%d",
              cfg->nSamplerate, cfg->nChannels, sc->bytes_per_sample);
    }
    pthread_mutex_unlock(&sc->mutex);
}

int TinaSoundDeviceStart(SoundCtrl* s)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    pthread_mutex_lock(&sc->mutex);
    int ret = 0;
    printf("TinaSoundDeviceStart(): sc->sound_status = %d\n", sc->sound_status);
    if (sc->sound_status == STATUS_START) {
        logd("Sound device already start.");
        pthread_mutex_unlock(&sc->mutex);
        return ret;
    } else if (sc->sound_status == STATUS_PAUSE) {
        if (snd_pcm_state(sc->alsa_handler) == SND_PCM_STATE_SUSPENDED) {
            logd("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume");
            while ((ret = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN) {
                sleep(1);
            }
        }
        if (sc->alsa_can_pause) {
            if ((ret = snd_pcm_pause(sc->alsa_handler, 0)) < 0) {
                loge("snd_pcm_pause failed:%s", strerror(errno));
                pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
        } else {
            if ((ret = snd_pcm_prepare(sc->alsa_handler)) < 0) {
                loge("snd_pcm_prepare failed:%s", strerror(errno));
                pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
        }
        sc->sound_status = STATUS_START;
    } else if (sc->sound_status == STATUS_STOP) {
        sc->alsa_fragcount = 8;
        sc->chunk_size = 1024;//1024;
        ret = openSoundDevice(sc, BLOCK_MODE);
        logd("after openSoundDevice() ret = %d", ret);
        if (ret >= 0) {
            ret = setSoundDeviceParams(sc);
            if (ret < 0) {
                loge("setSoundDeviceParams fail , ret = %d", ret);
                pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
            sc->sound_status = STATUS_START;
        }
    }
    open_shm(sc);
    set_shm(sc, 1);
    TinaSoundDeviceVolCtl(sc, 1);
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}
int TinaSoundDeviceSetCallback(SoundCtrl* s, void *callback, void **pUserData)
{
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;

    //autCallback_func *msndCallback;
    //void ** msndCallbackDat;
    sc->msndCallback = (autCallback_func*)callback;
    sc->msndCallbackDat = pUserData;
    return 0;
}


int TinaSoundDeviceStop(SoundCtrl* s)
{
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    pthread_mutex_lock(&sc->mutex);
    printf("TinaSoundDeviceStop():sc->sound_status = %d\n", sc->sound_status);
    set_shm(sc, 0);
    TinaSoundDeviceVolCtl(sc, 0);
    tina_mixer_close(sc);
    if (sc->sound_status == STATUS_STOP) {
        logd("Sound device already stopped.");
        pthread_mutex_unlock(&sc->mutex);
        return ret;
    } else {
        if ((ret = snd_pcm_drop(sc->alsa_handler)) < 0) {
            loge("snd_pcm_drop():MSGTR_AO_ALSA_PcmPrepareError");
            pthread_mutex_unlock(&sc->mutex);
            return ret;
        }
        if ((ret = snd_pcm_prepare(sc->alsa_handler)) < 0) {
            loge("snd_pcm_prepare():MSGTR_AO_ALSA_PcmPrepareError");
            pthread_mutex_unlock(&sc->mutex);
            return ret;
        }
        ret = closeSoundDevice(sc);
        sc->sound_status = STATUS_STOP;
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

int TinaSoundDevicePause(SoundCtrl* s)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    pthread_mutex_lock(&sc->mutex);
    int ret = 0;
    logd("TinaSoundDevicePause(): sc->sound_status = %d", sc->sound_status);
    if (sc->sound_status == STATUS_START) {
        if (sc->alsa_can_pause) {
            logd("alsa can pause,use snd_pcm_pause");
            ret = snd_pcm_pause(sc->alsa_handler, 1);
            if (ret < 0) {
                loge("snd_pcm_pause failed:%s", strerror(errno));
                pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
        } else {
            logd("alsa can not pause,use snd_pcm_drop");
            if ((ret = snd_pcm_drop(sc->alsa_handler)) < 0) {
                loge("snd_pcm_drop failed:%s", strerror(errno));
                pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
        }
        sc->sound_status = STATUS_PAUSE;
    } else {
        logd("TinaSoundDevicePause(): pause in an invalid status,status = %d", sc->sound_status);
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

int TinaSoundDeviceWrite(SoundCtrl* s, void* pData, int nDataSize)
{
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    //TLOGD("TinaSoundDeviceWrite:sc->bytes_per_sample = %d\n",sc->bytes_per_sample);
    if (sc->bytes_per_sample == 0) {
        sc->bytes_per_sample = 4;
    }
    if (sc->sound_status == STATUS_STOP || sc->sound_status == STATUS_PAUSE) {
        return ret;
    }
    int num_frames = nDataSize / sc->bytes_per_sample;
    snd_pcm_sframes_t res = 0;
    if (((sc->msndCallback) != NULL) && (*(sc->msndCallback) != NULL)) {
        (*sc->msndCallback)(128, *sc->msndCallbackDat, pData, nDataSize);
    } else {
        //logv("sc->msndCallback=NULL\n");
    }

    if (!sc->alsa_handler) {
        loge("MSGTR_AO_ALSA_DeviceConfigurationError");
        return ret;
    }

    if (num_frames == 0) {
        loge("num_frames == 0");
        return ret;
    }
    /*
    AudioGain audioGain;
    audioGain.preamp = sc->mVolume;
    audioGain.InputChan = (int)sc->nChannelNum;
    audioGain.OutputChan = (int)sc->nChannelNum;
    audioGain.InputLen = nDataSize;
    audioGain.InputPtr = (short*)pData;
    audioGain.OutputPtr = (short*)pData;
    int gainRet = tina_do_AudioGain(&audioGain);
    if(gainRet == 0){
        loge("tina_do_AudioGain fail");
    }
    */

    do {
        //logd("snd_pcm_writei begin,nDataSize = %d",nDataSize);
        res = snd_pcm_writei(sc->alsa_handler, pData, num_frames);
        //logd("snd_pcm_writei finish,res = %ld",res);
        if (res == -EINTR) {
            /* nothing to do */
            res = 0;
        } else if (res == -ESTRPIPE) {
            /* suspend */
            logd("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume");
            while ((res = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN)
                sleep(1);
        }
        if (res < 0) {
            loge("MSGTR_AO_ALSA_WriteError,res = %ld", res);
            if ((res = snd_pcm_prepare(sc->alsa_handler)) < 0) {
                loge("MSGTR_AO_ALSA_PcmPrepareError");
                return res;
            }
        }
    } while (res == 0);
    return res < 0 ? res : res * sc->bytes_per_sample;
}

int TinaSoundDeviceReset(SoundCtrl* s)
{
    logd("TinaSoundDeviceReset()");
    return TinaSoundDeviceStop(s);
}

int TinaSoundDeviceGetCachedTime(SoundCtrl* s)
{
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    //logd("TinaSoundDeviceGetCachedTime()");
    if (sc->alsa_handler) {
        snd_pcm_sframes_t delay = 0;
        //notify:snd_pcm_delay means the cache has how much data(the cache has been filled with pcm data),
        //snd_pcm_avail_update means the free cache,
        if ((ret = snd_pcm_delay(sc->alsa_handler, &delay)) < 0) {
            loge("TinaSoundDeviceGetCachedTime(),ret = %d , delay = %ld", ret, delay);
            return 0;
        }
        //logd("TinaSoundDeviceGetCachedTime(),snd_pcm_delay>>> delay = %d",delay);
        //delay = snd_pcm_avail_update(sc->alsa_handler);
        //logd("TinaSoundDeviceGetCachedTime(), snd_pcm_avail_update >>> delay = %d\n",delay);
        if (delay < 0) {
            /* underrun - move the application pointer forward to catch up */
#if SND_LIB_VERSION >= 0x000901 /* snd_pcm_forward() exists since 0.9.0rc8 */
            snd_pcm_forward(sc->alsa_handler, -delay);
#endif
            delay = 0;
        }
        //logd("TinaSoundDeviceGetCachedTime(),ret = %d , delay = %ld",ret,delay);
        ret = ((int)((float) delay * 1000000 / (float) sc->nSampleRate));
    }
    return ret;
}
int TinaSoundDeviceGetFrameCount(SoundCtrl* s)
{
    //to do
    return 0;
}
int TinaSoundDeviceSetPlaybackRate(SoundCtrl* s, const XAudioPlaybackRate *rate)
{
    //to do
    return 0;
}

int TinaSoundDeviceControl(SoundCtrl* s, int cmd, void* para)
{
    SoundCtrlContext* sc;
    int ret = 0;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);

    switch (cmd) {

        default:
            logd("__SdControll : unknown command (%d)...", cmd);
            break;
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}


/*
int TinaSoundDeviceSetVolume(SoundCtrl* s, float volume){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    assert(sc);
    sc->mVolume = (int)volume;
    logd("sc->gain = %d",sc->mVolume);
    return 0;
}

int TinaSoundDeviceGetVolume(SoundCtrl* s, float *volume){
    return 0;
}

int TinaSoundDeviceSetCallback(SoundCtrl* s, SndCallback callback, void* pUserData){
    return 0;
}
*/

//===========volume control implement start================
static void open_shm(SoundCtrlContext* sc)
{
    if (sc->p_map) return;

    sc->shmid = shmget(0x2234, sizeof(vol_ctl_shm), IPC_CREAT | 0666);
    if (sc->shmid == -1) {
        printf("shmget err");
        return;
    }
    printf("shmid:%d \n", sc->shmid);

    sc->p_map = shmat(sc->shmid, NULL, 0);

    if (sc->p_map == (void *) -1) {
        printf("shmget err");
        return;
    }
    //printf("open_shm.....\n");
}

static void set_shm(SoundCtrlContext* sc, int play)
{
    //printf("set_shm..........\n");
    if (!sc->p_map) return;
    sc->p_map->type[sc->stream_type] = play;
    return;
}

static int get_shm(SoundCtrlContext* sc, audio_stream_type stream_type)
{
    int i = 0;
    int playing = -1;
    if (!sc->p_map) return -1;
    playing = sc->p_map->type[stream_type];
    //printf("stream %d play status : %d\n", stream_type, playing);

    return playing;
}

static void close_shm(SoundCtrlContext* sc)
{
    if (!sc->p_map)    return;
    shmdt(sc->p_map);
    int ret = shmctl(sc->shmid, IPC_RMID, NULL);
    if (ret < 0) {
        printf("rmerrr\n");
    }
    sc->p_map = NULL;
    printf("close_shm.....\n");
}

static int tina_mixer_get_control(SoundCtrlContext* sc, audio_stream_type type)
{
    if (!sc->mixer) {
        sc->mixer = mixer_open(MIXER_CARD);
        if (!sc->mixer) {
            printf("tina mixer is not init, eixt set volume\n");
            return 0;
        }
    }
    if (!sc->ctl[type]) {
        sc->ctl[type] = mixer_get_ctl_by_name(sc->mixer, pcm_control_table[type]);
        if (!sc->ctl[type]) {
            printf("Invalid mixer control.\n");
            return 0;
        }
    }

    return 1;
}

static void tina_mixer_close(SoundCtrlContext* sc)
{
    int i = 0;
    if (sc->mixer) {
        printf("close tina mixer...\n");
        mixer_close(sc->mixer);
        sc->mixer = NULL;
        for (i = 0; i < STREAM_MAX; i++) {
            sc->ctl[i] = NULL;
        }
    }
}

static void tina_mixer_set_control_value(SoundCtrlContext* sc, audio_stream_type type, int left, int right)
{
    int step = 0;
    int point = 0;

    if (tina_mixer_get_control(sc, type) <= 0) {
        //printf("mixer control %s is not exist, will not set it.\n", pcm_control_table[type]);
        return;
    }
    for (step = 0; step < VOL_CURVE_STEP; step++) {
        if (volumeCurves[type][step][0] >= left) {
            point = volumeCurves[type][step][1];
            break;
        }
    }
    left *= point;
    //printf("left point = %d, left = %d\n", point, left);
    if (left > 255)left = 255;

    for (step = 0; step < VOL_CURVE_STEP; step++) {
        if (volumeCurves[type][step][0] >= right) {
            point = volumeCurves[type][step][1];
            break;
        }
    }
    right *= point;
    //printf("right point = %d, right = %d\n", point, right);
    if (right > 255)right = 255;

    printf("set %s : left:%d, right:%d\n", pcm_control_table[type], left, right);
    if (mixer_ctl_set_value(sc->ctl[type], 0, left)) {
        printf("set left vol fail.\n");
    }
    if (mixer_ctl_set_value(sc->ctl[type], 1, right)) {
        printf("set right vol fail.\n");
    }
}

//user interface set stream volume
void TinaSoundDeviceSetVol(SoundCtrl* s, int left, int right)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;

    if (left > stram_volume_max[sc->stream_type] || left < 0) {
        printf("left volume out of range, use default vol.\n");
        left = stram_volume_default[sc->stream_type];
    }
    sc->vol_left = left;

    if (right > stram_volume_max[sc->stream_type] || right < 0) {
        printf("right volume out of range, use default vol.\n");
        right = stram_volume_default[sc->stream_type];
    }
    sc->vol_right = right;

    tina_mixer_set_control_value(sc, sc->stream_type, left, right);

    //TODO: store user's volume

}

//user interface get stream volume
void TinaSoundDeviceGetVol(SoundCtrl* s, int *left, int *right)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;

    *left = sc->vol_left;
    *right = sc->vol_right;
}

static void stream_volume_control(SoundCtrlContext* sc, int stream_type, int isplaying)
{
    stream_control type = ST_NULL;
    int left = 0, right = 0;

    if (sc->control_type == ST_NULL) {
        type = ST_MUTE;
    } else {
        type = sc->control_type;
    }

    if (type == ST_MUTE) {
        if (isplaying) {
            left = 0;
            right = 0;
        } else {
            left = sc->vol_left;
            right = sc->vol_right;
        }

        tina_mixer_set_control_value(sc, stream_type, left, right);
    } else if (type == ST_DECREASE) {
        if (isplaying) {
            left = sc->vol_left / 2;
            right = sc->vol_right / 2;
        } else {
            left = sc->vol_left;
            right = sc->vol_right;
        }
        tina_mixer_set_control_value(sc, stream_type, left, right);
    } else if (type == ST_PAUSE) {
        //TODO: not implement now...
    }

}

static void TinaSoundDeviceVolCtl(SoundCtrlContext* sc, int isplaying)
{
    int i = 0;

    for (i = 0; i < sc->stream_type; i++) {
        if (get_shm(sc, i) == 1) {
            //printf("higher priorty stream is playing, control myself...\n");
            stream_volume_control(sc, sc->stream_type, isplaying);
            return;
        }
    }

    i = sc->stream_type;
    tina_mixer_set_control_value(sc, i, sc->vol_left, sc->vol_right);

    for (++i; i < STREAM_MAX; i++) {
        if (get_shm(sc, i) == 1) {
            if (isplaying) {
                //printf("control all lower priorty stream\n");
                stream_volume_control(sc, i, isplaying);
            } else {
                //printf("control the first lower priorty stream\n");
                stream_volume_control(sc, i, isplaying);
                break;
            }
        }
    }
}


//===========volume control implement end================



