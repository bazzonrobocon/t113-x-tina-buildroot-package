#define LOG_TAG "DvrNormalMode"
#include "DvrNormalMode.h"
#include "CameraLog.h"
#include "CameraFileCfg.h"
#include "Utils.h"

#ifdef DVR_USE_CEDARE
#include "AWRecorder.h"
// #include "aw_ini_parser.h"

void DvrNormalMode::dvr_yuv_buffer_cb(recorder_yuv_buf_s* yuv_buf, void* privateData)
{
    // CameraLogV("dvr_yuv_buffer_cb");
}

void DvrNormalMode::dvr_main_stream_callback(recorder_video_stream_s* video_stream, void* privateData)
{
    CameraLogV("dvr_main_stream_callback");
}

void DvrNormalMode::dvr_sub_stream_callback(recorder_video_stream_s* video_stream, void* privateData)
{
    CameraLogV("update sub stream!");
}

void DvrNormalMode::dvr_audio_stream_callback(recorder_audio_stream_s* audio_stream, void* privateData)
{
    CameraLogV("dvr_audio_callback");
}
#endif

void DvrNormalMode::dvr_audiodata_callback(int32_t msgType, nsecs_t timestamp, int card, int device,char *dataPtr, int dsize,void* user)
{
    CameraLogV("dvr_audiodata_callback");
}


DvrNormalMode::DvrNormalMode(int cameraId)
:mCameraId(cameraId),
 mRecorder(NULL),
 mHardwareCamera(NULL),
 mAudioDevice(NULL),
 mNotifyCb(NULL),
 mDataCb(NULL),
 mDataCbTimestamp(NULL),
 mCbUser(NULL),
 usrDatCb(NULL),
 mCbUserDat(NULL)
{

}

DvrNormalMode::~DvrNormalMode()
{
    CAMERA_LOG;
    uninitializeDev();
    if (mHardwareCamera != NULL) {
        delete mHardwareCamera;
        mHardwareCamera = NULL;
        CameraLogD("del mHardwareCamera=%p", mHardwareCamera);
    }

    if (mRecorder != NULL) {
        delete mRecorder;
        mRecorder = NULL;
    }
}

int DvrNormalMode::openDevice()
{
    mHardwareCamera = new CameraHardware();
    CameraLogE("openDevice mHardwareCamera:%p",mHardwareCamera);
    if (mHardwareCamera == NULL) {
        CameraLogE("%s: Unable to instantiate fake camera class", __FUNCTION__);
        return -1;
    }
    sprintf(mHardwareCamera->mHardwareInfo.device_name, "/dev/video%d", mCameraId);
    mHardwareCamera->mHardwareInfo.device_id = mCameraId;
    updateHardwareInfo(mHardwareCamera, mCameraId);
    int rt = initializeDev(mHardwareCamera);
    mHardwareCamera->enableMsgType(CAMERA_MSG_VIDEO_FRAME);
    if (!rt) {
        CameraLogV("initializeDev failed");
    }
    return rt;
}


int DvrNormalMode::updateHardwareInfo(CameraHardware *p, int id)
{
    if (!p) {
        CameraLogE("updateHardwareInfo is NULL");
        return -1;
    }

    switch (config_get_heigth(id)) {
        case 480:
            p->mHardwareInfo.cvbs_type = 0;
            break;
        case 576:
            p->mHardwareInfo.cvbs_type = 1;
            break;
        case 960:
            p->mHardwareInfo.cvbs_type = 2;
            break;
        case 1152:
            p->mHardwareInfo.cvbs_type = 3;
            break;
        case 1080:
        case 720:
            p->mHardwareInfo.cvbs_type = -1;
            break;
        default:
            p->mHardwareInfo.cvbs_type = 1;
            break;
    }
    return 0;
}



int  DvrNormalMode::startPriview(view_info vv)
{
    Mutex::Autolock locker(&mObjectLock);

    unsigned int h = config_get_heigth(mCameraId);
    unsigned int w = config_get_width(mCameraId);
    int ret = -1;
    struct src_info srcinfo = {w, h, 0};

    CameraLogD("Camera[%d] screeen:%d,chnanel:%d,layer:%d,view:x:%d y:%d width:%d height:%d",vv.screen_id,vv.channel,vv.layer,vv.x,vv.y,vv.w,vv.h);

    if (!mHardwareCamera) {
        CameraLogE("startPriview error,mHardwareCamera is null");
        return -1;
    }
    // mode = config_get_tvout(mCameraId);
    int tvoutmode = 0;  // config_get_tvout(mCameraId);
    mHardwareCamera->setPreviewParam(vv, srcinfo, tvoutmode, 3);
    ret = mHardwareCamera->startPreview();
    if (ret != 0) {
        return -1;
    }
    mHardwareCamera->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);
    return NO_ERROR;
}

int  DvrNormalMode::stopPriview()
{
    mHardwareCamera->stopPreview();
    return 0;
}

int  DvrNormalMode::recordInit()
{
    CAMERA_LOG;
    int cycltime;
    int ret = -1;

    char dir[128];
    memset(dir,0,sizeof(dir));
    sprintf(dir,"/mnt/sdcard/mmcblk1p1/DVR/cam_%d",mCameraId);
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
    mEncodeConfig.input_width = recordwith;
    mEncodeConfig.input_height = recordheith;
    mEncodeConfig.input_format = YUV_FORMAT_YVU420SP;

    mEncodeConfig.output_width = recordwith;
    mEncodeConfig.output_height = recordheith;
    mEncodeConfig.bitrate = 4 * 1024 * 1024;
    mEncodeConfig.framerate = 25;
    mEncodeConfig.duration = cycltime * 60;//sencond
    mEncodeConfig.output_encoder_type = VENC_CODEC_H264;//:VENC_CODEC_JPEG;
    mEncodeConfig.output_muxer_type = RECORDER_MUXER_TYPE_TS;
    mEncodeConfig.video_channel_enable[0] = true;
    mEncodeConfig.preallocate = true;


    mEncodeConfig.yuvstream_cb_enable = true;
    mEncodeConfig.yuvstream_cb = dvr_yuv_buffer_cb;
    mEncodeConfig.cameraId = mCameraId;

    sprintf((char*)mEncodeConfig.output_dir,"%s",dir);

    if(mRecorder == NULL)
    {
        mRecorder = new AWRecorder;
    }
    ret = mRecorder->encodeInit(&mEncodeConfig);

#endif

    if(ret == 0 && mAudioDevice == NULL)
    {
        mAudioDevice = new AudioCapThread();
        mAudioDevice->reqAudioSource(dvr_audiodata_callback,this);
    }

    return ret;
}

int  DvrNormalMode::startRecord()
{
    Mutex::Autolock locker(&mObjectLock);
    int ret = -1;
    CAMERA_LOG;
    if(mAudioDevice != NULL)
    {
        mAudioDevice->startCapture(0);
    }

    if (mRecorder)
    {
        ret = mRecorder->start();
        if (ret != 0) {
            CameraLogE("mRecorder->start error");
            return -1;
        }
        CameraLogE("startRecord mHardwareCamera:%p",mHardwareCamera);
        mHardwareCamera->enableMsgType(CAMERA_MSG_VIDEO_FRAME);  // enable recorder
        ret = mHardwareCamera->startRecording();
        if (ret != 0) {
            CameraLogE("mHardwareCamera startRecording error");
            return -1;
        }
    }

    return NO_ERROR;
}

int  DvrNormalMode::stopRecord()
{
    CAMERA_LOG;
    mHardwareCamera->disableMsgType(CAMERA_MSG_VIDEO_FRAME);  // enable recorder
    mHardwareCamera->stopRecording();

    if(mRecorder != NULL)
    {
        mRecorder->stop();
    }

    if(mAudioDevice != NULL)
    {
        mAudioDevice->stopAudio();
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

int  DvrNormalMode::takePicture()
{
    if(mHardwareCamera != NULL)
    {
        mHardwareCamera->enableMsgType(CAMERA_MSG_COMPRESSED_IMAGE);
        mHardwareCamera->takePicture();
    }
    return NO_ERROR;
}

void DvrNormalMode::setCallbacks(notify_callback notify_cb,data_callback data_cb,data_callback_timestamp data_cb_timestamp,void* user)
{
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mCbUser = user;

}

bool DvrNormalMode::enableWaterMark()
{
    CameraLogV("enableWaterMark");
    if (mHardwareCamera == NULL) {
        return false;
    }
    if (mHardwareCamera->sendCommand(CAMERA_CMD_START_WATER_MARK, 0, 0) != NO_ERROR) {
        CameraLogE("START_WATER_MARK failed");
        return false;
    }
    usleep(50000);
    return true;
}

bool DvrNormalMode::disableWaterMark()
{
    CameraLogV("disableWaterMark");
    if (mHardwareCamera == NULL) {
        return false;
    }

    if (mHardwareCamera->sendCommand(CAMERA_CMD_STOP_WATER_MARK, 0, 0) != NO_ERROR) {
        CameraLogE("STOP_WATER_MARK failed");
        return false;
    }
    return true;
}

int  DvrNormalMode::setWaterMarkMultiple(const char *str)
{
    CameraLogV("setWaterMarkMultiple");
    if (mHardwareCamera == NULL || str == NULL) return BAD_VALUE;

    CameraLogV("setWaterMarkMultiple(%s)", str);
    // String8 str8(str);            // = new String8(str)
    return mHardwareCamera->setWaterMarkMultiple((char *)str,
                                                  WATER_MARK_DISP_MODE_VIDEO_AND_PREVIEW);
}


bool DvrNormalMode::initializeDev(CameraHardware *pHardware) {
    if (pHardware == NULL) {
        CameraLogE("initializeDev:pHardware is null");
        return -1;
    }

    if (!pHardware->isCameraIdle()) {
        CameraLogE("camera[%d] is busy, wait for a moment", pHardware->mHardwareInfo.device_id);
        return -1;
    }

    if (pHardware->connectCamera() != NO_ERROR) {
        CameraLogE("%s: Unable to connect camera", __FUNCTION__);
        return -EINVAL;
    }

    if (pHardware->Initialize() != NO_ERROR) {
        CameraLogE("%s: Unable to Initialize camera", __FUNCTION__);
        return -EINVAL;
    }

    // Enable error, and metadata messages by default
    pHardware->enableMsgType(
    /* MOTION_DETECTION_ENABLE */
#ifdef ADAS_ENABLE
        CAMERA_MSG_ADAS_METADATA | /* ADAS_ENABLE */
#endif
        CAMERA_MSG_PREVIEW_METADATA);

    pHardware->setCallbacks(__notify_cb, __data_cb, __data_cb_timestamp, (void *)this);

    // for hardware codec
    pHardware->sendCommand(CAMERA_CMD_SET_CEDARX_RECORDER, 0, 0);

    return true;
}

bool DvrNormalMode::uninitializeDev() {
    // Release the hardware resources.
    if (mHardwareCamera != NULL) {
        mHardwareCamera->releaseCamera();
    }
    // mHardwareCamera.clear();
    return true;
}

int DvrNormalMode::release()
{
    Mutex::Autolock locker(&mObjectLock);
    CameraLogW("stop camera id %d", mCameraId);
    if (NULL != mHardwareCamera) {
        mHardwareCamera->disableMsgType(CAMERA_MSG_ALL_MSGS);
        mHardwareCamera->stopRecording();
        mHardwareCamera->stopPreview();
        mHardwareCamera->cancelPicture();
    } else {
        CameraLogW("mHardwareCameras=%p", mHardwareCamera);
    }

    if(mAudioDevice != NULL)
    {
        mAudioDevice->releaseAudioSource(0);
    }    

    if(NULL != mRecorder)
    {
        mRecorder->stop();
        mRecorder->release();
        delete mRecorder;
        mRecorder = NULL;
    }

    return NO_ERROR;    
}

int DvrNormalMode::getCameraId()
{
    return mCameraId;
}

int DvrNormalMode::SetDataCB(usr_data_cb cb, void* user)
{
    usrDatCb = cb;
    mCbUserDat = user;
}

void DvrNormalMode::notifyCallback(int32_t msgType, int32_t ext1, int32_t ext2, void *user) {
    // UNUSED(msgType);
    // UNUSED(ext1);
    // UNUSED(ext2);
    // UNUSED(user);
    CAMERA_LOG;
}

void DvrNormalMode::__notify_cb(int32_t msg_type, int32_t ext1, int32_t ext2, void *user) {
    CAMERA_LOG;
    DvrNormalMode *__this = static_cast<DvrNormalMode *>(user);
    __this->notifyCallback(msg_type, ext1, ext2, user);
    if (__this->mNotifyCb != NULL) {
        CameraLogV("call mNotifyCb, cbdat:%p", __FUNCTION__, __LINE__, __this->mCbUser);
        __this->mNotifyCb(msg_type, ext1, ext2, __this->mCbUser);
    }
}

// takepicture data cb
void DvrNormalMode::__data_cb(int32_t msg_type, char *data, camera_frame_metadata_t *metadata,
                            void *user) {
    CAMERA_LOG;
    DvrNormalMode *__this = static_cast<DvrNormalMode *>(user);
    if (__this->mDataCb != NULL) {
        __this->mDataCb(msg_type, data, metadata, __this->mCbUser);  // up to the app
    }
}

// record data cb
void DvrNormalMode::__data_cb_timestamp(nsecs_t timestamp, int32_t msg_type, char *data, void *user) {
    // CAMERA_LOG;
    DvrNormalMode *__this = static_cast<DvrNormalMode *>(user);
    if (__this->mDataCbTimestamp != NULL) {
        __this->mDataCbTimestamp(timestamp, msg_type, data, __this->mCbUser);  // up to the app
    }

    if(__this->mRecorder)
    {
        // CameraLogV("camera[%d] encode.",__this->getCameraId());
        __this->mRecorder->encode(data);
    }
}