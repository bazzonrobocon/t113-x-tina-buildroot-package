#ifndef __DVR_360_MODEL_H
#define __DVR_360_MODEL_H

#include "type_camera.h"
#include "Recorder.h"
#include "CameraHardware2.h"
#include "DvrNormalMode.h"
#include "CameraManager.h"

using namespace android;

class Dvr360Mode :public DvrNormalMode
{

public:
    Dvr360Mode(int cameraId);
    ~Dvr360Mode();
public:
    int openDevice();
    int startPriview(view_info vv);
    int stopPriview();

    int recordInit();
    int startRecord();
    int stopRecord();
    // int takePicture();
    // void setCallbacks(notify_callback notify_cb,data_callback data_cb,data_callback_timestamp data_cb_timestamp,void* user);
    // bool enableWaterMark();
    // bool disableWaterMark();
    // int setWaterMarkMultiple(const char *str);
protected:
    bool uninitializeDev();

protected:
    CameraHardware** mHardwareCameras;
    CameraManager* mCameraManager;
};

#endif
