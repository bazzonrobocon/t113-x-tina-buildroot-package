#ifndef __DVR_SUBSTREAM_MODEL_H
#define __DVR_SUBSTREAM_MODEL_H

#include "type_camera.h"
#include "Recorder.h"
#include "CameraHardware2.h"
#include "CameraManager.h"
#include "DvrNormalMode.h"

using namespace android;

class DvrSubStreamMode :public DvrNormalMode
{
public:
    DvrSubStreamMode(int cameraId)
    :DvrNormalMode(cameraId)
    {

    }

    int recordInit();
};

#endif
