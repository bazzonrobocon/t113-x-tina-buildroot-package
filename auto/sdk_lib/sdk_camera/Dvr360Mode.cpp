#define LOG_TAG "Dvr360Mode"
#include "Dvr360Mode.h"
#include "CameraLog.h"
#include "CameraFileCfg.h"
#include "Utils.h"
#include "CameraDebug.h"
// #include "aw_ini_parser.h"


Dvr360Mode::Dvr360Mode(int cameraId)
:DvrNormalMode(cameraId),
mHardwareCameras(NULL),
mCameraManager(NULL)
{
    mCameraManager = new CameraManager();
    mCameraManager->setOviewEnable(true);
    mCameraManager->setComposeOrder(0, 1, 2, 3);  // set position
    mCameraManager->composeBufInit();
}

Dvr360Mode::~Dvr360Mode()
{
    CAMERA_LOG;
    uninitializeDev();
    for (int i = 0; i < NUM_OF_360CAMERAS; i++)
    {
        if (mHardwareCameras[i] != NULL)
        {
            delete mHardwareCameras[i];
            mHardwareCameras[i] = NULL;
        }
    }

    if (mCameraManager != NULL)
    {
        delete mCameraManager;
        mCameraManager = NULL;
    }

    if (mRecorder != NULL) {
        delete mRecorder;
        mRecorder = NULL;
    }
}

int Dvr360Mode::openDevice()
{
    Mutex::Autolock locker(&mObjectLock);
    if (mHardwareCameras == NULL)
    {
        mHardwareCameras = new CameraHardware *[NUM_OF_360CAMERAS];
        if (mHardwareCameras == NULL)
        {
            CameraLogE("%s: Unable to allocate V4L2Camera array for %d entries", __FUNCTION__,
                       MAX_NUM_OF_CAMERAS);
            return -1;
        }
        memset(mHardwareCameras, 0, NUM_OF_360CAMERAS * sizeof(CameraHardware *));
    }

    int width = config_get_width(mCameraId);
    int height = config_get_heigth(mCameraId);
    int ret;
    for (int i = 0; i < NUM_OF_360CAMERAS; i++) {
        if(width <= 0 || height <= 0)
        {
            width = config_get_width(0);
            height = config_get_heigth(0);
        }
        mHardwareCameras[i] = new CameraHardware(width,height);
        if (mHardwareCameras[i] == NULL) {
            CameraLogE("error %s: Unable to instantiate fake camera class", __FUNCTION__);
            return -1;
        }

        sprintf(mHardwareCameras[i]->mHardwareInfo.device_name, "/dev/video%d", i * CAMERA_ID_STEP);
        mHardwareCameras[i]->mHardwareInfo.device_id = i * CAMERA_ID_STEP;

        ret = updateHardwareInfo(mHardwareCameras[i], i * CAMERA_ID_STEP);
        if (ret < 0) {
            return ret;
        }
        ret = initializeDev(mHardwareCameras[i]);
        if (!ret) {
            CameraLogE("dev init fail repeat");
            return ret;
        }

        mHardwareCameras[i]->setCameraManager(mCameraManager);
        mCameraManager->setCameraHardware(i, mHardwareCameras[i]);
    }
    mHardwareCamera = mHardwareCameras[0];
    return 0;
}

int  Dvr360Mode::startPriview(view_info vv)
{
    Mutex::Autolock locker(&mObjectLock);
    unsigned int h = config_get_heigth(mCameraId);
    unsigned int w = config_get_width(mCameraId);
    int ret = -1;
    struct src_info srcinfo = {w, h, 0};

    if ((!mCameraManager) || (!mHardwareCamera)) {
        CameraLogE("startPriview error,mHardwareCameras is null");
        return -1;
    }
    int tvoutmode = 0;  // config_get_tvout(mCameraId);
    mHardwareCamera->setPreviewParam(vv, srcinfo, tvoutmode, 3);
    mCameraManager->setOviewEnable(true);
    mCameraManager->startPreview();
    return NO_ERROR;
}

int  Dvr360Mode::stopPriview()
{
    Mutex::Autolock locker(&mObjectLock);
    // for (int i = 0; i < NUM_OF_360CAMERAS; i++)
    // {
    //     mHardwareCameras[i]->stopPreview();
    // }
    mCameraManager->stopPreview();
    return 0;
}


int  Dvr360Mode::startRecord()
{
    Mutex::Autolock locker(&mObjectLock);
    int ret = -1;
    if (mRecorder)
    {
        ret = mRecorder->start();
        if (ret != 0) {
            CameraLogE("mRecorder->start error");
            return -1;
        }

        mHardwareCamera->enableMsgType(CAMERA_MSG_VIDEO_FRAME);  // enable recorder
        ret = mHardwareCamera->startRecording();
        if (ret != 0) {
            CameraLogE("mHardwareCamera startRecording error");
            return -1;
        }
    }
    return 0;
}


int  Dvr360Mode::stopRecord()
{
    Mutex::Autolock locker(&mObjectLock);
    CAMERA_LOG;
    mHardwareCamera->disableMsgType(CAMERA_MSG_VIDEO_FRAME);  // enable recorder
    mHardwareCamera->stopRecording();

    if(mRecorder != NULL)
    {
        mRecorder->stop();
    }

#ifdef USE_RECORD_AUDIO_API
    if (mAudioCap != NULL) {
        mAudioCap->stopCapture(mAudioHdl);
    }
    mDvrAudioEnc->AudioRecStop();
#endif
    usleep(2000000);
    return NO_ERROR;
}

int  Dvr360Mode::recordInit()
{
    Mutex::Autolock locker(&mObjectLock);
    CAMERA_LOG;
    int cycltime;
    int ret = 0;

    char dir[128];
    memset(dir,0,sizeof(dir));
    sprintf(dir,"mnt/sdcard/mmcblk1p1/DVR/cam_%d",mCameraId);
    Utils::createDir(dir);

    int recordwith = config_get_width(mCameraId);
    int recordheith = config_get_heigth(mCameraId);

    cycltime = config_get_cyctime(mCameraId);
    CameraLogD("recordwith = %d,recordheith = %d, cycltime = %d\n", recordwith, recordheith,
               cycltime);
    int encode_config = config_get_encode_fmt(mCameraId);
    CameraLogD("encode_config %d", encode_config);
#ifdef DVR_USE_CEDARE
    memset(&mEncodeConfig,0,sizeof(Config));
    mEncodeConfig.input_width = recordwith * 2;
    mEncodeConfig.input_height = recordheith * 2;
    mEncodeConfig.input_format = YUV_FORMAT_YVU420SP;
    mEncodeConfig.output_width = recordwith * 2;
    mEncodeConfig.output_height = recordheith * 2;
    mEncodeConfig.bitrate = 4 * 1024 * 1024;
    mEncodeConfig.framerate = 25;
    mEncodeConfig.duration = cycltime * 60;
    mEncodeConfig.output_encoder_type = VENC_CODEC_H264;//:VENC_CODEC_JPEG;
    mEncodeConfig.output_muxer_type = RECORDER_MUXER_TYPE_TS;
    mEncodeConfig.video_channel_enable[0] = true;
    mEncodeConfig.yuvstream_cb_enable = true;
    mEncodeConfig.yuvstream_cb = dvr_yuv_buffer_cb;
    mEncodeConfig.preallocate = true;
    mEncodeConfig.cameraId = mCameraId;
    sprintf((char*)mEncodeConfig.output_dir,"%s",dir);

    if(mRecorder == NULL)
    {
        mRecorder = new AWRecorder;
    }

    ret = mRecorder->encodeInit((void*)&mEncodeConfig);
#endif

    return ret;
}



bool Dvr360Mode::uninitializeDev() {
    // Release the hardware resources.
    if (mCameraManager != NULL) {
        mCameraManager->releaseCamera();
    }
    // mHardwareCameras.clear();
    return true;
}