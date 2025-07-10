
#ifndef __LEGACY_REC_CAMERA_H__
#define __LEGACY_REC_CAMERA_H__

#include "LegacyRecorder.h"




class LegacyRecorderImpl:public LegacyRecorder
{

public:
    LegacyRecorderImpl();
    ~LegacyRecorderImpl();
public:
    int videoEncDeinit();
    int videoEncParmInit(int sw, int sh, int dw, int dh, unsigned int bitrate, int framerate,int type, int pix = 0);
    int setDuration(int sec);
    int encode(V4L2BUF_t *pdata, char*outbuf, int*outsize, void* user);
    int start();
    int stop();
    int startRecord();
    int stopRecord();
    void setCallbacks(notify_callback notify_cb, void *user);
    void dataCallback(int32_t msgType, char *dataPtr, camera_frame_metadata_t * metadata, void *user);
    status_t dataCallbackTimestamp(nsecs_t timestamp, int32_t msgType, char *dataPtr, void *user);
    int SetDataCB(usr_data_cb cb, void* user);
    int getRecordState();

    // static status_t audioRecMuxerCb(int32_t msgType,
    //                                 char *dataPtr, int dalen,
    //                                 void *user);    
};

#endif
