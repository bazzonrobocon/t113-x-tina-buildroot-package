#include "vencoder.h"
#include "Rtc.h"
#include "V4L2CameraDevice2.h"
#include "CallbackNotifier.h"
#include "PreviewWindow.h"
#include "CameraHardware2.h"

#include "CdxMuxer.h"
#include "CdxParser.h"
#include "MuxerWriter.h"
#include "LegacyRecorderImpl.h"

LegacyRecorderImpl::LegacyRecorderImpl(/* args */)
{

}

LegacyRecorderImpl::~LegacyRecorderImpl()
{

}

int LegacyRecorderImpl::videoEncDeinit()
{
    return 0;
}

int LegacyRecorderImpl::videoEncParmInit(int sw, int sh, int dw, int dh, unsigned int bitrate, int framerate,int type, int pix)
{
    return 0;
}

int LegacyRecorderImpl::setDuration(int sec)
{
    return 0;
}

int LegacyRecorderImpl::encode(V4L2BUF_t *pdata, char*outbuf, int*outsize, void* user)
{
    return 0;
}

int LegacyRecorderImpl::start()
{
    return 0;
}

int LegacyRecorderImpl::stop()
{
    return 0;
}

int LegacyRecorderImpl::startRecord()
{
    return 0;
}

int LegacyRecorderImpl::stopRecord()
{
    return 0;
}

void LegacyRecorderImpl::setCallbacks(notify_callback notify_cb, void *user)
{
}

void LegacyRecorderImpl::dataCallback(int32_t msgType, char *dataPtr, camera_frame_metadata_t * metadata, void *user)
{
}

status_t LegacyRecorderImpl::dataCallbackTimestamp(nsecs_t timestamp, int32_t msgType, char *dataPtr, void *user)
{
    return 0;
}

int LegacyRecorderImpl::SetDataCB(usr_data_cb cb, void* user)
{
    return 0;
}

int LegacyRecorderImpl::getRecordState()
{
    return 0;
}

