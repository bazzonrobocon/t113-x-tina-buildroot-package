/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : soundControl_tinyalsa.c
 * Description : soundControl for tinyalsa output
 * Au xfang
 * date: 2020/3/18
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <string.h>
#include <cdx_log.h>

#include "soundControl_tinyalsa.h"

//#define USE_AHUB_PLAY 1
typedef struct SoundCtrlContext {
    SoundCtrl                   base;
    pthread_mutex_t             mutex;
    void*                       pUserData;
#ifdef USE_AHUB_PLAY
    unsigned int                ahub_card;
    unsigned int                ahub_device;
#endif
    unsigned int                card;
    unsigned int                device;

    struct pcm *                pcm;
#ifdef USE_AHUB_PLAY
    struct pcm *                ahub_pcm;
#endif
    struct pcm_config           config;

    int                         readsize;
    autCallback_func*           msndCallback;
    void **                     msndCallbackDat;
} SoundCtrlContext;

int tinyalsaSetCallback(SoundCtrl* s, void *callback, void **pUserData)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }

    pthread_mutex_lock(&sc->mutex);
    sc->msndCallback = (autCallback_func*)callback;
    sc->msndCallbackDat = pUserData;

    pthread_mutex_unlock(&sc->mutex);
    return 0;
}

static void tinyalsaRelease(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return ;
    }

    pthread_mutex_lock(&sc->mutex);
#ifdef USE_AHUB_PLAY
    if (sc->ahub_pcm) {
        pcm_close(sc->ahub_pcm);
        sc->ahub_pcm = NULL;
    }
#endif
    if (sc->pcm) {
        pcm_stop(sc->pcm);
        pcm_close(sc->pcm);
        sc->pcm = NULL;
    }

    pthread_mutex_unlock(&sc->mutex);


    pthread_mutex_destroy(&sc->mutex);

    free(sc);
    sc = NULL;

    return;
}

static void tinyalsaSetFormat(SoundCtrl* s, CdxPlaybkCfg *cfg)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return ;
    }
    pthread_mutex_lock(&sc->mutex);

    sc->config.channels = cfg->nChannels;
    sc->config.rate = cfg->nSamplerate;

    if (cfg->nBitpersample == 32) {
        sc->config.format = PCM_FORMAT_S32_LE;
    } else if (cfg->nBitpersample == 16) {
        sc->config.format = PCM_FORMAT_S16_LE;
    }

    //printf("%s cfg->nSamplerate=%d\ncfg->nChannels=%d\ncfg->nBitpersample=%d\nnDataType=%d\n",__FUNCTION__
    //              ,cfg->nSamplerate,cfg->nChannels,cfg->nBitpersample,cfg->nDataType);


    pthread_mutex_unlock(&sc->mutex);
    (void)cfg;
    return;
}

static int tinyalsaStart(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&sc->mutex);
    if (sc->pcm) {
        pthread_mutex_unlock(&sc->mutex);
        return 0;
    }

    sc->pcm = pcm_open(sc->card, sc->device, PCM_OUT, &sc->config);
    if (!sc->pcm || !pcm_is_ready(sc->pcm)) {
        fprintf(stderr, "\033[1m\033[33m pcm_open failed.Unable to open PCM device %u (%s) \033[0m\n",
                sc->device, pcm_get_error(sc->pcm));
        pthread_mutex_unlock(&sc->mutex);
        return -1;
    }
    pcm_start(sc->pcm);

#ifdef USE_AHUB_PLAY
    sc->ahub_pcm = pcm_open(sc->ahub_card, sc->ahub_device, PCM_OUT, &sc->config);
    if (!sc->ahub_pcm || !pcm_is_ready(sc->ahub_pcm)) {
        fprintf(stderr, "\033[1m\033[33m Unable to open AHub PCM device %u (%s) \033[0m\n",
                sc->ahub_device, pcm_get_error(sc->ahub_pcm));
        pthread_mutex_unlock(&sc->mutex);
        return -1;
    }

    sc->readsize = pcm_frames_to_bytes(sc->ahub_pcm, pcm_get_buffer_size(sc->ahub_pcm));
#else
    sc->readsize = pcm_frames_to_bytes(sc->pcm, pcm_get_buffer_size(sc->pcm));
#endif
    pthread_mutex_unlock(&sc->mutex);

    return 0;
}

static int tinyalsaStop(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    int               ret = 0;
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&sc->mutex);
#ifdef USE_AHUB_PLAY
    if (sc->ahub_pcm) {
        pcm_close(sc->ahub_pcm);
        sc->ahub_pcm = NULL;
    }
#endif

    if (sc->pcm) {
        pcm_stop(sc->pcm);
        pcm_close(sc->pcm);
        sc->pcm = NULL;
    }

    pthread_mutex_unlock(&sc->mutex);

    return ret;
}

static int tinyalsaPause(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

static int tinyalsaWrite(SoundCtrl* s, void* pData, int nDataSize)
{
    ////printf("tinyalsa %s\n",__FUNCTION__);
    int ret;
    SoundCtrlContext* sc;
    void * data = pData;

    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
#ifdef USE_AHUB_PLAY
    if (!sc->ahub_pcm) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
#else
    if (!sc->pcm) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
#endif
    pthread_mutex_lock(&sc->mutex);

    if (((sc->msndCallback) != NULL) && (*(sc->msndCallback) != NULL)) {
        (*sc->msndCallback)(128, *sc->msndCallbackDat, pData, nDataSize);
    }

    int num_frames = 0;
    if (sc->readsize < nDataSize) {
        num_frames = nDataSize / sc->readsize ;
    }

    if (num_frames > 0) {
        int len = sc->readsize;
        while (num_frames >= 0) {
#ifdef USE_AHUB_PLAY
            ret = pcm_write(sc->ahub_pcm, data, len);
#else
            ret = pcm_write(sc->pcm, data, len);
#endif
            if (ret) {
                printf("Error playing sample\n");
            }
            data += len;

            if (sc->readsize < nDataSize - sc->readsize) {
                len = sc->readsize;
                nDataSize -= sc->readsize;
            } else {
                len = nDataSize;
            }
            num_frames--;
        }
    } else {
#ifdef USE_AHUB_PLAY
        ret = pcm_write(sc->ahub_pcm, data, nDataSize);
#else
        ret = pcm_write(sc->pcm, data, nDataSize);
#endif
        if (ret) {
            printf("Error playing sample\n");
        }
    }

    pthread_mutex_unlock(&sc->mutex);
    return nDataSize;
}

//* called at player seek operation.
static int tinyalsaReset(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    return SoundDeviceStop(s);
}

static int tinyalsaGetCachedTime(SoundCtrl* s)
{
    ////printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&sc->mutex);


    pthread_mutex_unlock(&sc->mutex);
    return 0;
}

static int tinyalsaGetFrameCount(SoundCtrl* s)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if (!sc) {
        printf("%s error,sc is NULL\n", __FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&sc->mutex);


    pthread_mutex_unlock(&sc->mutex);
    return 0;
}

int tinyalsaSetPlaybackRate(SoundCtrl* s, const XAudioPlaybackRate *rate)
{
    //printf("tinyalsa %s\n",__FUNCTION__);
    CDX_UNUSE(s);
    CDX_UNUSE(rate);
    return 0;
}

static int tinyalsaControl(SoundCtrl* s, int cmd, void* para)
{
    SoundCtrlContext* sc;
    int ret = 0;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);

    switch (cmd) {
        default:
            //printf("tinyalsaControl : unknown command (%d)...", cmd);
            break;
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

static SoundControlOpsT mSoundControlOps = {
    .destroy       =  tinyalsaRelease,
    .setFormat     =  tinyalsaSetFormat,
    .start         =  tinyalsaStart,
    .stop          =  tinyalsaStop,
    .pause         =  tinyalsaPause,
    .write         =  tinyalsaWrite,
    .reset         =  tinyalsaReset,
    .getCachedTime =  tinyalsaGetCachedTime,
    .getFrameCount =  tinyalsaGetFrameCount,
    .setPlaybackRate = tinyalsaSetPlaybackRate,
    .control          = tinyalsaControl
};

static int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                       char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        fprintf(stderr, "%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        fprintf(stderr, "%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

static int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                              unsigned int rate, unsigned int bits, unsigned int period_size)
{
    struct pcm_params *params;
    int can_play;

    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        fprintf(stderr, "Unable to open PCM device %u.\n", device);
        return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", " frames");

    pcm_params_free(params);

    return can_play;
}

SoundCtrl* SoundDeviceCreate()
{
    SoundCtrlContext* s;
    printf("xfang tinyalsa %s\n", __FUNCTION__);
    s = (SoundCtrlContext*)malloc(sizeof(SoundCtrlContext));
    if (s == NULL) {
        loge("tinyalsa malloc memory fail.");
        return NULL;
    }
    memset(s, 0, sizeof(SoundCtrlContext));

    s->base.ops = &mSoundControlOps;
#ifdef USE_AHUB_PLAY
    s->ahub_card = 2;
    s->ahub_device = 0;
#endif
    s->card = 0;
    s->device = 0;
    unsigned int channels = 2;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;

    memset(&s->config, 0, sizeof(s->config));
    s->config.channels = channels;
    s->config.rate = rate;
    s->config.period_size = period_size;
    s->config.period_count = period_count;
    if (bits == 32) {
        s->config.format = PCM_FORMAT_S32_LE;
    } else if (bits == 16) {
        s->config.format = PCM_FORMAT_S16_LE;
    }
    s->config.start_threshold = 0;
    s->config.stop_threshold = 0;
    s->config.silence_threshold = 0;
#ifdef USE_AHUB_PLAY
    if (!sample_is_playable(s->ahub_card, s->ahub_device, channels, rate, bits,
                            period_size)) {
        fprintf(stderr, "\033[1m\033[33m  sample_is_playable failed.\033[0m\n");
        free(s);
        return NULL;
    }
#else
    if (!sample_is_playable(s->card, s->device, channels, rate, bits,
                            period_size)) {
        fprintf(stderr, "\033[1m\033[33m  sample_is_playable failed.\033[0m\n");
        free(s);
        return NULL;
    }
#endif

    pthread_mutex_init(&s->mutex, NULL);

    return (SoundCtrl*)&s->base;
}

