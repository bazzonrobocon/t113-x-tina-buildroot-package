#ifdef DVR_USE_CEDARE

#define LOG_TAG "AWRecorder"
#include <recorder/recorder.h>
#include "AWRecorder.h"
#include "CameraLog.h"
#include <cstring>
#include "type_camera.h"
#include "Utils.h"
//* create --> setconfig --> start --> stop --> destroy

#define DEBUG_SAVE_INPUT_FRAME 0

AWRecorder::AWRecorder()
:mRecorderCtrl(NULL)
{
    mRecorderCtrl = recorder_create();
    if (mRecorderCtrl == NULL)
    {
        CameraLogE("recorder create error!");
    }
}

AWRecorder::AWRecorder(char* name)
:mRecorderCtrl(NULL)
{
    memset(mName,0,sizeof(mName));
    sprintf(mName,"%s",name);
    mRecorderCtrl = recorder_create();
    if (mRecorderCtrl == NULL)
    {
        CameraLogE("recorder create error!");
    }
}

AWRecorder::~AWRecorder()
{

}

static void dumpVenConfig(video_encoder_config_s& config){
    char msg[512];
    sprintf(msg,
                "\n"
                "config.width:%d\n"
                "config.height:%d\n"
                "config.frame_rate:%d\n"
                "config.bitrate:%d\n"
                "config.I_frame_interval:%d\n"
                "config.qp_min:%d\n"
                "config.qp_max:%d\n"
                "config.vbr:%d\n"
                "config.encoder_type:%d\n"
                "config.stream_output_mode:%d\n",
                 config.width,
                 config.height,
                 config.frame_rate,
                 config.bitrate,
                 config.I_frame_interval,
                 config.qp_min,
                 config.qp_max,
                 config.vbr,
                 config.encoder_type,
                 config.stream_output_mode);
        CameraLogD("%s",msg);
}

static void enableVenc(video_encoder_config_s& config,Config inputConfig)
{
    config.width            = inputConfig.output_width;
    config.height           = inputConfig.output_height;
    config.frame_rate       = inputConfig.framerate;
    config.bitrate          = inputConfig.bitrate;
    config.I_frame_interval = 30;
    config.qp_min           = 1;
    config.qp_max           = 51;
    config.vbr              = 0;
    config.vbr              = 0;
    config.stream_output_mode = STREAM_OUTPUT_MODE_CALLBACK;
    config.encoder_type     = (RECORDER_VENCODER_TYPE)inputConfig.output_encoder_type;
    config.enable           = 1;
    // config.muxer_config.muxer_type         = (RECORDER_MUXER_TYPE)inputConfig.output_muxer_type;
    if(Utils::isEmpty(inputConfig.output_dir))
    {
        config.stream_output_mode = STREAM_OUTPUT_MODE_CALLBACK;
    }else{
        config.stream_output_mode = STREAM_OUTPUT_MODE_WRITE_TO_FILE;
    }

    if(inputConfig.output_encoder_type != RECORDER_VENCODER_TYPE_H264
        && inputConfig.output_encoder_type != RECORDER_VENCODER_TYPE_H265
        && inputConfig.output_encoder_type != RECORDER_VENCODER_TYPE_JPEG)
    {
        CameraLogW("not support encode type:%d! use default h264~",inputConfig.output_encoder_type);
        config.encoder_type = RECORDER_VENCODER_TYPE_H264;
    }
}

static void enableAudioEnc(audio_encoder_config_s& config,Config inputConfig)
{
    //set audio_encoder_config_s data
    if (inputConfig.audio_channel_enable) {
        CameraLogD("enable Audio Record F:%s,L:%d", __FUNCTION__, __LINE__);
        config.sample_rate = inputConfig.audio_sample_rate;//16000
        config.enable = 1;
        config.sample_bits = 16;
        config.channels = inputConfig.audio_channel>2? 1:inputConfig.audio_channel;
        config.encoder_type = RECORDER_AENCODER_TYPE_AAC;
    }
}

static void configMuxer(muxer_config_s& config,Config inputConfig)
{
    config.muxer_type = (RECORDER_MUXER_TYPE)inputConfig.output_muxer_type;
    if(inputConfig.output_muxer_type != RECORDER_MUXER_TYPE_MP4
        && inputConfig.output_muxer_type != RECORDER_MUXER_TYPE_TS)
    {
        CameraLogW("not support muxer type:%d! use default TS",inputConfig.output_muxer_type);
        config.muxer_type = RECORDER_MUXER_TYPE_TS;
    }
}

/**
 * Example:
 *   memset(&mEncodeConfig,0,sizeof(Config));
 *   mEncodeConfig.input_width = recordwith;
 *   mEncodeConfig.input_height = recordheith;
 *   mEncodeConfig.output_width = recordwith;
 *   mEncodeConfig.output_height = recordheith;
 *   mEncodeConfig.bitrate = 8;
 *   mEncodeConfig.framerate = 25;
 *   mEncodeConfig.duration = cycltime * 60;
 *   mEncodeConfig.output_encoder_type = encode_config == 0? VENC_CODEC_H264:VENC_CODEC_JPEG;
 *   mEncodeConfig.output_muxer_type = RECORDER_MUXER_TYPE_TS;
 *   Utils::generateVideoName(mEncodeConfig.output_file_name,"/mnt/sdcard/mmcblk1p1",
 *   mEncodeConfig.output_muxer_type == RECORDER_MUXER_TYPE_TS?"ts":"mp4",mCameraId);
 *   mEncodeConfig.video_channel_enable[0] = true;
 *   mRecorder->encodeInit(mEncodeConfig);
*/
int AWRecorder::encodeInit(void* param)
{
    Mutex::Autolock locker(&mObjectLock);
    Config inputConfig;
    memcpy((void*)&inputConfig,param,sizeof(Config));

    memcpy(&mConfig,&inputConfig,sizeof(Config));
    recorder_config config;
    memset(&config,0,sizeof(recorder_config));

    config.yuv_config.width  = inputConfig.input_width;
    config.yuv_config.height = inputConfig.input_height;
    config.yuv_config.format = (RECORDER_YUV_FORMAT)inputConfig.input_format;

    if(inputConfig.video_channel_enable[0])
    {
        enableVenc(config.venc_config[0],inputConfig);
    }

    if(inputConfig.video_channel_enable[1])
    {
        Config subStreamConfig = inputConfig;
        subStreamConfig.output_width = inputConfig.sub_output_width;
        subStreamConfig.output_height = inputConfig.sub_output_height;
        subStreamConfig.framerate = inputConfig.sub_framerate;
        subStreamConfig.bitrate = inputConfig.sub_bitrate;
        enableVenc(config.venc_config[1],subStreamConfig);
        config.venc_config[1].stream_output_mode = STREAM_OUTPUT_MODE_CALLBACK;
    }
    //set audio_encoder_config_s data
    enableAudioEnc(config.aenc_config,inputConfig);
    configMuxer(config.muxer_config,inputConfig);
    dumpVenConfig(config.venc_config[0]);
    dumpVenConfig(config.venc_config[1]);

    recorder_setconfig(mRecorderCtrl,&config);
    if(inputConfig.yuvstream_cb_enable && inputConfig.yuvstream_cb != NULL)
    {
        int ret = recorder_set_yuv_buffer_callback(mRecorderCtrl, (yuv_buffer_callback)*inputConfig.yuvstream_cb, this);
        if (ret != 0) {
            CameraLogE("recorder_set_yuv_buffer_callback failed F:%s,L:%d", __FUNCTION__,__LINE__);
            return -1;
        }
    }

    if(inputConfig.video_channel_enable[0] && inputConfig.mainstream_cb_enable && inputConfig.mainstream_cb != NULL)
    {
        int ret = recorder_set_video_stream_callback(mRecorderCtrl, (video_stream_callback)*inputConfig.mainstream_cb, 0, this);
        if (ret != 0) {
            CameraLogE("recorder_set_video_stream_callback failed F:%s,L:%d", __FUNCTION__,__LINE__);
            return -1;
        }
    }

    if(inputConfig.video_channel_enable[1] && inputConfig.substream_cb_enable && inputConfig.substream_cb != NULL)
    {
        int ret = recorder_set_video_stream_callback(mRecorderCtrl, (video_stream_callback)*inputConfig.substream_cb, 1, this);
        if (ret != 0) {
            CameraLogE("recorder_set_video_sub_stream_callback failed F:%s,L:%d", __FUNCTION__,__LINE__);
            return -1;
        }
    }

    if(inputConfig.substream_cb_enable && inputConfig.audiostream_cb != NULL)
    {
        int ret = recorder_set_audio_stream_callback(mRecorderCtrl, (audio_stream_callback)*inputConfig.audiostream_cb, inputConfig.audio_channel, this);
        if (ret != 0) {
            CameraLogE("recorder_set_audio_stream_callback failed F:%s,L:%d", __FUNCTION__,__LINE__);
            return -1;
        }
    }
    return nextFile();
}


int AWRecorder::nextFile()
{
    if(Utils::isEmpty(mConfig.output_dir))
    {
        CameraLogE("output_dir!:%s is Empty! F:%s,L:%d", mConfig.output_dir,__FUNCTION__, __LINE__);
        return -1;
    }

    Utils::generateVideoName((char*)mConfig.output_file_name,mConfig.output_dir,
    mConfig.output_muxer_type == RECORDER_MUXER_TYPE_TS?"ts":"mp4",mConfig.cameraId);

    if(mConfig.preallocate && Utils::isVfat32(mConfig.output_dir))
    {
       if(Utils::checkSpaceEnough(mConfig.output_dir))
       {
          int size = Utils::calcPreFileSize(mConfig.bitrate,mConfig.duration * 2);
          int ret = Utils::preAllocatefallocate(mConfig.output_file_name,size);
          if(ret < 0){
             CameraLogE("preAllocatefallocate:%s failed F:%s,L:%d", mConfig.output_file_name,__FUNCTION__, __LINE__);
             return ret;
          }
       }else{
            std::list<char*> videoFiles;
            Utils::getVideoFileList(mConfig.output_dir,&videoFiles);
            if (videoFiles.size() == 0)
            {
                CameraLogE("space not enought!:%s failed F:%s,L:%d", mConfig.output_dir,__FUNCTION__, __LINE__);
                return -1;
            }
            char* renameFile = videoFiles.back();
            char name[256];
            memset(&name,0,sizeof(name));
            sprintf(name,"%s/%s",mConfig.output_dir,renameFile);
            if (rename(name, mConfig.output_file_name) == 0) {
                 CameraLogD("file %s rename success to %s\n", name,mConfig.output_file_name);
            } else {
                CameraLogE("rename faild (%s),space not enought!:%s failed F:%s,L:%d",strerror(errno), mConfig.output_dir,__FUNCTION__, __LINE__);
                return -1;
            }
       }
    }else{
       if(!Utils::checkSpaceEnough(mConfig.output_dir))
       {
           CameraLogE("space not enought!:%s failed F:%s,L:%d", mConfig.output_dir,__FUNCTION__, __LINE__);
           return -1;
       }
       int ret = Utils::createFile(mConfig.output_file_name);
    }

    if(Utils::isExist(mConfig.output_file_name))
    {
        CameraLogI("recorder_start_next_file:%s!",mConfig.output_file_name);
        return recorder_start_next_file(mRecorderCtrl,mConfig.output_file_name,0);
    }

    if (mConfig.output_file_fd >= 0)
    {
        return recorder_start_next_file_by_fd(mRecorderCtrl, mConfig.output_file_fd, 0);
    }
    CameraLogE("output file not config!");
    return -1;
}

int AWRecorder::encode(void* frame)
{
    Mutex::Autolock locker(&mObjectLock);
    V4L2BUF_t* buf = (V4L2BUF_t*)frame;
    recorder_yuv_buf_s yuvBuf;
    memset(&yuvBuf,0,sizeof(yuvBuf));
    int y_size = buf->width * buf->height;
    yuvBuf.id = 0;
    yuvBuf.phy_addr = (unsigned char*)buf->addrPhyY;
    // yuvBuf.phy_addr_c = (unsigned char*)buf->addrPhyC;
    yuvBuf.phy_addr_c = (unsigned char*)buf->addrPhyY + y_size;

    yuvBuf.vir_addr = (unsigned char*)buf->addrVirY;
    // yuvBuf.vir_addr_c = (unsigned char*)buf->addrVirC;
    yuvBuf.vir_addr_c = (unsigned char*)buf->addrVirY + y_size;
    yuvBuf.pts = Utils::getSystemTimeNow() / 1000;

    if(yuvBuf.phy_addr == NULL || yuvBuf.phy_addr_c == NULL ||yuvBuf.vir_addr == NULL ||yuvBuf.vir_addr_c == NULL)
    {
        CameraLogE("AWRecorder::encode faild! cause wrong frame data!");
        return -1;
    }

#if DEBUG_SAVE_INPUT_FRAME
    char name[128];
    sprintf(name,"/encode_input_%dx%d.bin",buf->width,buf->height);
    Utils::saveframe(name,buf->addrVirY,buf->width*buf->height*3/2,1);
#endif

    int ret = recorder_queue_yuv_buffer(mRecorderCtrl, &yuvBuf);
    if (ret != 0) {
        CameraLogE("recorder_queue_yuv_buffer failed F:%s,L:%d", __FUNCTION__, __LINE__);
    }
    return 0;
}

#define CHECK_ERROR(x) if(int ret = x){CameraLogE("%s:%d ret:%d",__FUNCTION__,__LINE__,ret);return ret;}

int AWRecorder::start()
{
    Mutex::Autolock locker(&mObjectLock);
    if(mConfig.video_channel_enable[0])
    {
        CHECK_ERROR(recorder_start(mRecorderCtrl, 0))
    }
    if(mConfig.video_channel_enable[1])
    {
        CHECK_ERROR(recorder_start(mRecorderCtrl, 1))
    }

    mNextRecordThread = new AWThread("cycle recorde thread",onNextRecord,this);
    mNextRecordThread->start();
    return 0;
}


int AWRecorder::pause()
{
    Mutex::Autolock locker(&mObjectLock);
    if(mConfig.video_channel_enable[0])
    {
        CHECK_ERROR(recorder_pause(mRecorderCtrl, 0))
    }
    if(mConfig.video_channel_enable[1])
    {
        CHECK_ERROR(recorder_pause(mRecorderCtrl, 1))
    }

    return 0;
}


int AWRecorder::stop()
{
    Mutex::Autolock locker(&mObjectLock);
    if(mConfig.video_channel_enable[0])
    {
        CHECK_ERROR(recorder_stop(mRecorderCtrl, 0))
    }
    if(mConfig.video_channel_enable[1])
    {
        CHECK_ERROR(recorder_stop(mRecorderCtrl, 1))
    }

    return 0;
}

bool  AWRecorder::onNextRecord(void* privData)
{
    AWRecorder*  __this__ = (AWRecorder*)privData;
    Config config = __this__->getConfig();

    if(config.duration > 0)
    {
        sleep(config.duration);
    }
    __this__->pause();
    int ret = __this__->nextFile();
    __this__->start();
    if(ret != 0)
    {
        CameraLogE("AWRecorder::onNextRecord faild!");
        return false;
    }
    return true;
}

int AWRecorder::release()
{
    Mutex::Autolock locker(&mObjectLock);
    mNextRecordThread->stop();
    recorder_destroy(mRecorderCtrl);
    return 0;
}

Config AWRecorder::getConfig()
{
    return mConfig;
}
#endif