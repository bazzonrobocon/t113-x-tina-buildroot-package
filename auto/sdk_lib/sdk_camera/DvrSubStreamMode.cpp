#define LOG_TAG "DvrSubStreamMode"
#include "DvrSubStreamMode.h"
#include "CameraLog.h"
#include "CameraFileCfg.h"
#include "Utils.h"


int  DvrSubStreamMode::recordInit()
{
    CAMERA_LOG;
    int cycltime;
    int ret = -1;

    int recordwith = config_get_width(mCameraId);
    int recordheith = config_get_heigth(mCameraId);

    cycltime = config_get_cyctime(mCameraId);
    CameraLogD("recordwith = %d,recordheith = %d, cycltime = %d\n", recordwith, recordheith,
               cycltime);
    int encode_config = config_get_encode_fmt(mCameraId);
    CameraLogD("encode_config %d", encode_config);

#ifdef DVR_USE_CEDARE
    memset(&mEncodeConfig,0,sizeof(Config));
    mEncodeConfig.input_width = recordwith;
    mEncodeConfig.input_height = recordheith;
    // mEncodeConfig.input_format = YUV_FORMAT_YUV420SP;
    mEncodeConfig.input_format = YUV_FORMAT_YVU420SP;

    mEncodeConfig.output_width = recordwith;
    mEncodeConfig.output_height = recordheith;
    mEncodeConfig.bitrate = 4 * 1024 * 1024;
    mEncodeConfig.framerate = 25;
    mEncodeConfig.duration = cycltime * 60;
    mEncodeConfig.output_encoder_type = VENC_CODEC_H264;//:VENC_CODEC_JPEG;
    mEncodeConfig.output_muxer_type = RECORDER_MUXER_TYPE_TS;
    mEncodeConfig.video_channel_enable[0] = true;
    mEncodeConfig.video_channel_enable[1] = true;

    mEncodeConfig.sub_output_width = 640;
    mEncodeConfig.sub_output_height = 480;
    mEncodeConfig.sub_framerate = 25;
    mEncodeConfig.sub_bitrate = 4 * 1024 * 1024;

    mEncodeConfig.yuvstream_cb_enable = true;
    mEncodeConfig.yuvstream_cb = dvr_yuv_buffer_cb;
    mEncodeConfig.mainstream_cb_enable = false;
    mEncodeConfig.mainstream_cb = dvr_main_stream_callback;
    mEncodeConfig.substream_cb_enable = true;
    mEncodeConfig.substream_cb = dvr_sub_stream_callback;
    // mEncodeConfig.audiostream_cb_enable = true;
    // mEncodeConfig.audiostream_cb = dvr_audio_callback;

    char dir[128];
    memset(dir,0,sizeof(dir));
    sprintf(dir,"mnt/sdcard/mmcblk1p1/DVR/cam_%d",mCameraId);
    Utils::createDir(dir);

    mEncodeConfig.preallocate = true;
    mEncodeConfig.cameraId = mCameraId;
    sprintf((char*)mEncodeConfig.output_dir,"%s",dir);

    if(mRecorder == NULL)
    {
        char name[128];
        sprintf(name,"camera[%d]_recorder",mCameraId);
        mRecorder = new AWRecorder(name);
    }

    ret = mRecorder->encodeInit(&mEncodeConfig);
#endif

    return ret;
}