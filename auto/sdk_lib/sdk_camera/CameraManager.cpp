#define LOG_TAG "CameraManager"
#include "CameraLog.h"

#include "CameraDebug.h"
#include "CameraManager.h"
#include "sunxiMemInterface.h"
#include "AWMemory.h"

#if 1
#include "G2dApi.h"
using namespace g2dapi;
#define SAVE_FRAME 0
#if SAVE_FRAME
static int count = 0;
#endif

#ifdef USE360_Deinterlace_HW
#include "deinterlace2.h"
#endif

#define ABANDONFRAMECT  15

#define NTSC_WIDTH 720
#define NTSC_HEIGHT 480

#define PAL_WIDTH 720
#define PAL_HEIGHT 576

#define CSI720_WIDTH 1280
#define CSI720_HEIGHT 720

#define MAX_WIDTH CSI720_WIDTH
#define MAX_HEIGHT CSI720_HEIGHT

#define CSI720_COMPOSE_WIDTH 2560
#define CSI720_COMPOSE_HEIGHT 1440

#define NTSC_COMPOSE_WIDTH 1440
#define NTSC_COMPOSE_HEIGHT 960

#define PAL_COMPOSE_WIDTH 1440
#define PAL_COMPOSE_HEIGHT 1152

#define MAX_COMPOSE_WIDTH  CSI720_COMPOSE_WIDTH
#define MAX_COMPOSE_HEIGHT CSI720_COMPOSE_HEIGHT

namespace android
{

CameraManager::CameraManager():
    mFrameWidth(0),
    mFrameHeight(0),
    mIonFd(-1),
    mManagerIsInited(0),
    mCameraStep(CAMERA_ID_STEP)
#ifdef USE360_Deinterlace_HW
    , mDiProcess(NULL)
#endif
{
    CameraLogD("new CameraManager() version:%s", MODULE_VERSION);
    mAbandonFrameCnt = ABANDONFRAMECT;
    mCaptureState = CAPTURE_STATE_PAUSE;
    mIonFd = -1;
    open_cedar_dev();

    memset(&mComposeBuf, 0x0, sizeof(BufManager));
    memset((void *)mCameraBuf, 0x0, sizeof(BufManager)*NB_CAMERA);
    memset((void *)mCameraState, 0, sizeof(bool)*NB_CAMERA);

    for (int i = 0; i < MAX_NUM_OF_CAMERAS; i++) {
        mCameraHardware[i] = NULL;
    }

    mComposeOrderIdx0 = 0;
    mComposeOrderIdx1 = 1;
    mComposeOrderIdx2 = 2;
    mComposeOrderIdx3 = 3;

    //init mutex and condition
    pthread_mutex_init(&mComposeMutex, NULL);
    pthread_cond_init(&mComposeCond, NULL);

    pthread_mutex_init(&mCameraMutex, NULL);
    pthread_cond_init(&mCameraCond, NULL);

    // init command queue
    OSAL_QueueCreate(&mQueueCMCommand, CMD_CM_QUEUE_MAX);
    memset((void*)mQueueCMElement, 0, sizeof(mQueueCMElement));

    // init command thread
    pthread_mutex_init(&mCommandMutex, NULL);
    pthread_cond_init(&mCommandCond, NULL);
    mCommandThread = new DoCommandThread(this);

    // init Compose thread
    mComposeThread = new ComposeThread(this);

    mPreviewThread = new PreviewThread(this);

}

int CameraManager::composeBufInit()
{
    paramStruct_t param_in;
    memset(&param_in, 0, sizeof(paramStruct_t));
    int ret = allocOpen(MEM_TYPE_DMA, &param_in, NULL);
    if (ret < 0) {
        CameraLogE("open dma_ion failed");
    }
    if (0 == mFrameWidth) {
        if (NULL != mCameraHardware[0]) {
            mFrameWidth = mCameraHardware[0]->getFrameWidth();
            mFrameHeight = mCameraHardware[0]->getFrameHeight();
        } else {
            mFrameWidth = 1280;
            mFrameHeight = 720;
        }
    }
    int composeW = mFrameWidth * 2;
    int composeH = mFrameHeight * 2;
    ion_buffer_node ibuf;
    memset(&param_in, 0, sizeof(paramStruct_t));
    param_in.size = composeW * composeH * 3 / 2;
    for (int i = 0; i < NB_COMPOSE_BUFFER; i++) {
        ret = allocAlloc(MEM_TYPE_DMA, &param_in, NULL);
        if (ret < 0) {
            CameraLogD("composeBufInit allocAlloc failed");
        }

        ibuf = param_in.ion_buffer;
        ibuf.phy = ion_get_phyaddr_from_fd(ibuf.fd_data.aw_fd);
        mComposeBuf.buf[i].overlay_info = (void *)malloc(sizeof(ion_buffer_node));
        memcpy(mComposeBuf.buf[i].overlay_info, &ibuf, sizeof(ion_buffer_node));
        mComposeBuf.buf[i].addrVirY = ibuf.vir;
        mComposeBuf.buf[i].addrPhyY = ibuf.phy;
        mComposeBuf.buf[i].dmafd = ibuf.fd_data.aw_fd;

        mComposeBuf.buf[i].addrVirC = 0;
        mComposeBuf.buf[i].addrPhyC = 0;

        memset((void*)mComposeBuf.buf[i].addrVirY, 0x00, composeW * composeH); //make it black
        memset((void*)(mComposeBuf.buf[i].addrVirY + composeW * composeH), 0x80, composeW * composeH / 2);
        CameraLogV("------COMP_USE_DMABUF_API------dmafd----=%d", ((ion_buffer_node*)mComposeBuf.buf[i].overlay_info)->fd_data.aw_fd);

    }

    mG2DHandle = g2dInit();//open("/dev/g2d", O_RDWR, 0);
    if (mG2DHandle < 0) {
        CameraLogE("open /dev/g2d failed");
        return -1;
    }

#ifdef USE360_Deinterlace_HW
    memset(&param_in, 0, sizeof(paramStruct_t));
    param_in.size = composeW * composeH * 3 >> 1;
    for (int i = 0; i < NB_COMPOSE_BUFFER; i++) {
        //ibuf[i]=dma_ion_alloc(mIonFd,size);
        ret = allocAlloc(MEM_TYPE_DMA, &param_in, NULL);
        if (ret < 0) {
            CameraLogE("allocAlloc failed");
        }
        ibuf = param_in.ion_buffer;
        mdiBuffer[i].overlay_info = (void *)malloc(sizeof(ion_buffer_node));
        memcpy(mdiBuffer[i].overlay_info, &ibuf, sizeof(ion_buffer_node));
        mdiBuffer[i].addrVirY = ibuf.vir;
        mdiBuffer[i].addrPhyY = ibuf.phy;
        mdiBuffer.buf[i].dmafd = ibuf.fd_data.aw_fd;

        mdiBuffer[i].addrVirC = NULL;
        mdiBuffer[i].addrPhyC = NULL;

        memset((void*)mdiBuffer[i].addrVirY, 0x00, composeW * composeH);
        memset((void*)mdiBuffer[i].addrVirY + composeW * composeH, 0x80, composeW * composeH >> 1);
        CameraLogV("mdiBuffer[%d]------dmafd----=%d", i, ((ion_buffer_node*)mdiBuffer[i].overlay_info)->fd_data.aw_fd);
    }

    mDiProcess = new DiProcess();
    if (mDiProcess == NULL) {
        CameraLogE("new Diprocess failed");
        goto END_ERROR;
    }
    CameraLogD("new Diprocess OK");
    ret = mDiProcess->init();
    if (ret != NO_ERROR) {
        CameraLogE("Diprocess init failed");
        goto END_ERROR;
    }
#endif

    if (mCommandThread != NULL)
        mCommandThread->startThread();


    if (mComposeThread != NULL)
        mComposeThread->startThread();

    if (mComposeThread != NULL)
        mPreviewThread->startThread();

    mManagerIsInited = 1;
    return 0;

END_ERROR:
#ifdef USE360_Deinterlace_HW
    if (mDiProcess != NULL) {
        delete mDiProcess;
        mDiProcess = NULL;
    }
#endif
    return -1;
}



int CameraManager::composeBufDeinit()
{
    paramStruct_t param_in;
    memset(&param_in, 0, sizeof(paramStruct_t));
    for (int i = 0; i < NB_COMPOSE_BUFFER; i++) {
        if (mComposeBuf.buf[i].addrVirY != 0) {
            param_in.vir = mComposeBuf.buf[i].addrVirY;
            allocFree(MEM_TYPE_DMA, &param_in, NULL);
            mComposeBuf.buf[i].addrVirY = 0;
        }

        if (NULL != mComposeBuf.buf[i].overlay_info) {
            free(mComposeBuf.buf[i].overlay_info);
            mComposeBuf.buf[i].overlay_info = NULL;
        }


#ifdef USE360_Deinterlace_HW
        if (mdiBuffer.buf[i].addrVirY != 0) {
            param_in.vir = mdiBuffer.buf[i].addrVirY;
            allocFree(MEM_TYPE_DMA, &param_in, NULL);
            mdiBuffer.buf[i].addrVirY = 0;
        }

        if (NULL != mdiBuffer.buf[i].overlay_info) {
            free(mdiBuffer.buf[i].overlay_info);
            mdiBuffer.buf[i].overlay_info = NULL;
        }

#endif
    }

#ifdef USE360_Deinterlace_HW
    if (mDiProcess != NULL)
        delete mDiProcess;
#endif

    allocClose(MEM_TYPE_DMA, &param_in, NULL);

    if (mG2DHandle != 0) {
        g2dUnit(mG2DHandle);
        mG2DHandle = 0;
    }
    mManagerIsInited = 0;
    return 0;
}

V4L2BUF_t * CameraManager::getAvailableWriteBuf()
{
    V4L2BUF_t *ret = NULL;
    pthread_mutex_lock(&mComposeMutex);
    if (mComposeBuf.buf_unused < NB_COMPOSE_BUFFER - 1) {
        ret = &mComposeBuf.buf[mComposeBuf.write_id];
    }
    pthread_mutex_unlock(&mComposeMutex);
    return ret;
}

bool CameraManager::canCompose()
{
    int i;
    bool canCompose = true;
    pthread_mutex_lock(&mCameraMutex);
    for (i = 0; i < NB_CAMERA; i++) {
        if (mCameraBuf[i].buf_unused <= 0) {
            //CameraLogW("mCameraBuf[camera %d].buf_unused==0",i + CAMERA_ID_STEP);
            canCompose = false;
            break;
        }
    }
    for (i = 0; i < NB_CAMERA; i++) {
        if (mCameraBuf[i].buf_unused >= NB_COMPOSE_BUFFER - 1) {
            V4L2BUF_t buffer = (mCameraBuf[i].buf[mCameraBuf[i].read_id]);
            mCameraBuf[i].read_id++;
            mCameraBuf[i].buf_unused--;
            if (mCameraBuf[i].read_id >= NB_COMPOSE_BUFFER) {
                mCameraBuf[i].read_id = 0;
            }
            pthread_mutex_unlock(&mCameraMutex);
            CameraLogD("too many buffers so release camrea[%d].index=%d", i, buffer.index);
            mCameraHardware[i]->releasePreviewFrame(buffer.index,true);
            pthread_mutex_lock(&mCameraMutex);
        }
    }
    pthread_mutex_unlock(&mCameraMutex);
    return canCompose;
}

int CameraManager::setFrameSize(int index, int width, int height)
{
    pthread_mutex_lock(&mCameraMutex);
    if (0 == mFrameWidth) { //set w/h only once
        mFrameWidth = width;
        mFrameHeight = height;
    }
    pthread_mutex_unlock(&mCameraMutex);
    return 0;
}

bool CameraManager::isSameSize()
{
    int width;
    int height;
    int i;

    width = mCameraHardware[0]->getFrameWidth();
    height = mCameraHardware[0]->getFrameHeight();
    for (i = 1; i < NB_CAMERA; i++) {
        if ((width != mCameraHardware[i]->getFrameWidth())
            || (height != mCameraHardware[i]->getFrameHeight()))
            return false;
    }

    return true;
}

int CameraManager::queueCameraBuf(int deviceId, V4L2BUF_t *buffer)
{
    int i;
    pthread_mutex_lock(&mCameraMutex);
    if (0 == mManagerIsInited) {
        CameraLogE("queueCameraBuf mManagerIsInited=%d",mManagerIsInited);
        pthread_mutex_unlock(&mCameraMutex);
        return -5;
    }
    if ((mCaptureState == CAPTURE_STATE_EXIT) || (mCaptureState == CAPTURE_STATE_PAUSE)) {
        pthread_mutex_unlock(&mCameraMutex);
        CameraLogE("queueCameraBuf mCaptureState=%d",mCaptureState);
        return -1;
    }
    // if ((index < CAMERA_ID_STEP) || (index >= CAMERA_ID_STEP + NB_CAMERA)) {
    //     CameraLogE("queueCameraBuf index %d out of ranger %d-%d", index,
    //           CAMERA_ID_STEP, CAMERA_ID_STEP + NB_CAMERA);
    //     pthread_mutex_unlock(&mCameraMutex);
    //     return -2;
    // }
    int index = deviceId / mCameraStep;
    if (mCaptureState == CAPTURE_STATE_READY) {
        mCameraState[index] = true;
        for (i = 0; i < NB_CAMERA; i++) {
            if (mCameraState[i] == false) {
                CameraLogD("other camera not ready!,Camera[%d]  f:%s line:%d",index,__FUNCTION__,__LINE__);
                pthread_mutex_unlock(&mCameraMutex);
                return -3;
            }
        }

        if (!isSameSize()) {
            mCaptureState = CAPTURE_STATE_PAUSE;
            pthread_mutex_unlock(&mCameraMutex);
            //mCameraHardware[CAMERA_ID_STEP]->onCameraDeviceError(CAMERA_ERROR_SIZE);
            CameraLogE("Camera size is not the same,So it's can not be composed");
            return -4;
        }
        CameraLogD("OK,It's will do camera buffer compose");
        mCaptureState = CAPTURE_STATE_STARTED;
    }
    if (mCameraBuf[index].buf_unused < NB_COMPOSE_BUFFER) {
        // buffer->refCnt++;
        mCameraBuf[index].buf[mCameraBuf[index].write_id] = *buffer;
        mCameraBuf[index].write_id++;
        if (mCameraBuf[index].write_id >= NB_COMPOSE_BUFFER)
            mCameraBuf[index].write_id = 0;
        mCameraBuf[index].buf_unused++;
    }

    pthread_cond_signal(&mCameraCond);
    pthread_mutex_unlock(&mCameraMutex);
    return 0;
}

void CameraManager::composeBuffer(unsigned char *outBuffer, unsigned char *inBuffer0, unsigned char *inBuffer1,
                                  unsigned char *inBuffer2, unsigned char *inBuffer3)
{
    //origin V4L2CameraDevice composebuffer need 60 ms
    //int buf1 = buf[0].
    //copy y
    int i = 0;
    int n = 0;
    unsigned char *pbuf;
    unsigned char *pbuf0;
    unsigned char *pbuf1;
    unsigned char *pbuf2;
    unsigned char *pbuf3;
    if ((inBuffer0 == 0) || (inBuffer1 == 0) || (inBuffer2 == 0) || (inBuffer3 == 0)) {
        memset((void*)outBuffer, 0x10, mFrameWidth * mFrameHeight);  //set it black
        memset((void*)(outBuffer + mFrameWidth * mFrameHeight),
               0x80, mFrameWidth * mFrameHeight / 2);
        return;
    }
    for (i = 0; i < mFrameHeight; i++) {
        memcpy(outBuffer + n * mFrameWidth, inBuffer0 + i * mFrameWidth, mFrameWidth);
        memcpy(outBuffer + (n + 1)*mFrameWidth, inBuffer1 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    pbuf = outBuffer + mFrameHeight * mFrameWidth * 2;
    n = 0;
    for (i = 0; i < mFrameHeight; i++) {
        memcpy(pbuf + n * mFrameWidth, inBuffer2 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, inBuffer3 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    //copy uv
    pbuf = outBuffer + mFrameWidth * mFrameHeight * 4;
    pbuf0 = inBuffer0 + mFrameWidth * mFrameHeight;
    pbuf1 = inBuffer1 + mFrameWidth * mFrameHeight;
    pbuf2 = inBuffer2 + mFrameWidth * mFrameHeight;
    pbuf3 = inBuffer3 + mFrameWidth * mFrameHeight;
    n = 0;
    for (i = 0; i < mFrameHeight / 2; i++) {
        memcpy(pbuf + n * mFrameWidth, pbuf0 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, pbuf1 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    pbuf = outBuffer + mFrameWidth * mFrameHeight * 5;
    n = 0;
    for (i = 0; i < mFrameHeight / 2; i++) {
        memcpy(pbuf + n * mFrameWidth, pbuf2 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, pbuf3 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }

}

//queue compose buffer and send condition signal to another wait thread
int CameraManager::queueComposeBuf()
{
    pthread_mutex_lock(&mComposeMutex);
    mComposeBuf.write_id++;
    if (mComposeBuf.write_id >= NB_COMPOSE_BUFFER)
        mComposeBuf.write_id = 0;
    mComposeBuf.buf_unused++;
    pthread_cond_signal(&mComposeCond);
    pthread_mutex_unlock(&mComposeMutex);
    return 0;
}


//output interface for get composebuf
V4L2BUF_t * CameraManager::dequeueComposeBuf()
{
    V4L2BUF_t *buf;
    V4L2BUF_t *prebuf;
    V4L2BUF_t *preprebuf ;
    pthread_mutex_lock(&mComposeMutex);
    if (mComposeBuf.buf_unused <= 0) {
        pthread_cond_wait(&mComposeCond, &mComposeMutex);
        if (mPreviewThread->getThreadStatus() == true) {
            pthread_mutex_unlock(&mComposeMutex);
            return NULL;
        }
    }
#if 0
    struct timeval lRunTimeEnd;
    gettimeofday(&lRunTimeEnd, NULL);
    long long time1 = lRunTimeEnd.tv_sec * 1000000 + lRunTimeEnd.tv_usec;
    long long time2 = 0;
    long long time3 = 0 ;
#endif

    buf = &mComposeBuf.buf[mComposeBuf.read_id];
    prebuf = (mComposeBuf.read_id == 0) ?
             (&mComposeBuf.buf[NB_COMPOSE_BUFFER - 1]) : (&mComposeBuf.buf[mComposeBuf.read_id - 1]);

    if (mComposeBuf.read_id == 0)
        preprebuf = &mComposeBuf.buf[NB_COMPOSE_BUFFER - 2] ;
    else if (mComposeBuf.read_id == 1)
        preprebuf = &mComposeBuf.buf[NB_COMPOSE_BUFFER - 1] ;
    else
        preprebuf = &mComposeBuf.buf[mComposeBuf.read_id - 2];

    mComposeBuf.read_id++;
    if (mComposeBuf.read_id >= NB_COMPOSE_BUFFER)
        mComposeBuf.read_id = 0;
    mComposeBuf.buf_unused--;
    pthread_mutex_unlock(&mComposeMutex);
    int composeW = mFrameWidth * 2;
    int composeH = mFrameHeight * 2;

    buf->width = composeW;
    buf->height = composeH;
    buf->crop_rect.left     = 0;
    buf->crop_rect.top      = 0;
    buf->crop_rect.width    = composeW - 1;
    buf->crop_rect.height   = composeH - 1;
    buf->timeStamp = (int64_t)systemTime();
    buf->format = V4L2_PIX_FMT_NV21;
    buf->isThumbAvailable = 0;
    buf->bytesused = composeW * composeH; //W*H*4 *3/2 ==> W*H*2*3

    buf->addrVirC = buf->addrVirY + buf->width * buf->height;
    buf->addrPhyC = buf->addrPhyY + buf->width * buf->height;

    // if (mAbandonFrameCnt > 0) {
    //     mAbandonFrameCnt--;
    //     memset((void*)buf->addrVirY, 0x10, composeW * composeH);
    //     memset((void*)(buf->addrVirY + composeW * composeH), 0x80, composeW * composeH / 2);
    //     return buf;
    // }

    paramStruct_t param_in;
#ifdef USE360_Deinterlace_HW
    if (mCaptureState == CAPTURE_STATE_STARTED) {
        int frame_size = composeW * composeH * 3 >> 1; //1440*960*3/2
        if (di_select == 2) {
            memset(&param_in, 0, sizeof(paramStruct_t));
            param_in.ion_buffer.fd_data.aw_fd = ((ion_buffer_node*)mdiBuffer[mComposeBuf.read_id].overlay_info)->fd_data.aw_fd;
            flushCache(MEM_TYPE_DMA, &param_in, NULL);

            mDiProcess->diProcess((unsigned char*)preprebuf->addrPhyY,
                                  (unsigned char*)prebuf->addrPhyY,
                                  (unsigned char *)buf->addrPhyY,
                                  composeW, composeH, V4L2_PIX_FMT_NV21,
                                  (unsigned char *)mdiBuffer[mComposeBuf.read_id].addrPhyY,
                                  composeW, composeH, V4L2_PIX_FMT_NV21, 0);

            mDiProcess->diProcess((unsigned char*)preprebuf->addrPhyY,
                                  (unsigned char*)prebuf->addrPhyY,
                                  (unsigned char *)buf->addrPhyY,
                                  composeW, composeH, V4L2_PIX_FMT_NV21,
                                  (unsigned char *)mdiBuffer[mComposeBuf.read_id].addrPhyY,
                                  composeW, composeH, V4L2_PIX_FMT_NV21, 1);




            mdiBuffer[mComposeBuf.read_id].width = buf->width;
            mdiBuffer[mComposeBuf.read_id].height = buf->height;
            mdiBuffer[mComposeBuf.read_id].index = buf->index;
            mdiBuffer[mComposeBuf.read_id].timeStamp = buf->timeStamp;
            mdiBuffer[mComposeBuf.read_id].crop_rect = buf->crop_rect;
            mdiBuffer[mComposeBuf.read_id].format = buf->format;


            mdiBuffer[mComposeBuf.read_id].addrVirC = mdiBuffer[mComposeBuf.read_id].addrVirY +
                                                      composeW * composeH;
            mdiBuffer[mComposeBuf.read_id].addrPhyC = buf->addrPhyY +
                                                      composeW * composeH;


            memset(&param_in, 0, sizeof(paramStruct_t));
            param_in.ion_buffer.fd_data.aw_fd = ((ion_buffer_node*)buf->overlay_info)->fd_data.aw_fd;
            flushCache(MEM_TYPE_DMA, &param_in, NULL);

            if (mCaptureState == CAPTURE_STATE_STARTED) {
                //CameraLogD("compose onNextFrameAvailable %dx%d",mdiBuffer[mComposeBuf.read_id].width,
                //      mdiBuffer[mComposeBuf.read_id].height);
                mCameraHardware[0]->mPreviewWindow.onNextFrameAvailable(&mdiBuffer[mComposeBuf.read_id], false, 0);
                mCameraHardware[0]->mCallbackNotifier.onNextFrameAvailable(&mdiBuffer[mComposeBuf.read_id], true, 0);
            }
        } else {
            di_select++;
        }
    }
#else   //have not define USE360_Deinterlace_HW
    if (mCaptureState == CAPTURE_STATE_STARTED) {
        memset(&param_in, 0, sizeof(paramStruct_t));
        param_in.ion_buffer.fd_data.aw_fd = ((ion_buffer_node*)buf->overlay_info)->fd_data.aw_fd;
        flushCache(MEM_TYPE_DMA, &param_in, NULL);
        mCameraHardware[0]->mPreviewWindow.onNextFrameAvailable(buf, false, 0);
        mCameraHardware[0]->mCallbackNotifier.onNextFrameAvailable(buf, true, 0);
    }
#endif

#if 0
    gettimeofday(&lRunTimeEnd, NULL);
    time2 = lRunTimeEnd.tv_sec * 1000000 + lRunTimeEnd.tv_usec;
    time3 = time2 - time1 ;
    static unsigned char s_printcnt = 0;
    if (s_printcnt++ > 50) {
        CameraLogD("compose use %lldus(%lldms)", time3, time3 / 1000);
        s_printcnt = 0;
    }
    time1 = time2;
#endif

    return buf;
}

//index0~3 should be 0~3    return 0 means success ; return -1 means faild
bool CameraManager::setComposeOrder(unsigned int index0, unsigned int index1, unsigned int index2, unsigned int index3)
{
    if ((index0 + index1 + index2 + index3) > 6) {
        CameraLogE("input param err!setComposeOrder faild");
        return -1;
    }
    mComposeOrderIdx0 = index0;
    mComposeOrderIdx1 = index1;
    mComposeOrderIdx2 = index2;
    mComposeOrderIdx3 = index3;
    return 0;
}

bool CameraManager::composeThread()
{
    int i;
    V4L2BUF_t inbuffer[NB_CAMERA];
    memset(&inbuffer, 0x0, sizeof(V4L2BUF_t)*NB_CAMERA);
    V4L2BUF_t *writeBuffer = NULL;
    bool bufReady  = canCompose();
    if (!bufReady) {
        //CameraLogV("composeThread  no camera buffer, sleep...");
        pthread_mutex_lock(&mCameraMutex);
        CameraLogV("composeThread wait...");
        pthread_cond_wait(&mCameraCond, &mCameraMutex);
        pthread_mutex_unlock(&mCameraMutex);
        return true;
    }

    writeBuffer = getAvailableWriteBuf();
    if (writeBuffer != NULL) {
        pthread_mutex_lock(&mCameraMutex);
        for (i = 0; i < NB_CAMERA; i++) {
            if (mCameraBuf[i].buf_unused <= 0) {
                inbuffer[i].addrVirY = 0;
                bufReady = false;
            } else {
                inbuffer[i] = (mCameraBuf[i].buf[mCameraBuf[i].read_id]);
                mCameraBuf[i].read_id++;
                mCameraBuf[i].buf_unused--;
                if (mCameraBuf[i].read_id >= NB_COMPOSE_BUFFER) {
                    mCameraBuf[i].read_id = 0;
                }
            }
        }
        pthread_mutex_unlock(&mCameraMutex);

        if (bufReady) {
            paramStruct_t param_in;
            memset(&param_in, 0, sizeof(paramStruct_t));
            param_in.ion_buffer.fd_data.aw_fd = ((ion_buffer_node*)writeBuffer->overlay_info)->fd_data.aw_fd;
            flushCache(MEM_TYPE_DMA, &param_in, NULL);

//struct timeval lRunTimeEnd;
//gettimeofday(&lRunTimeEnd, NULL);
//long long time1 = lRunTimeEnd.tv_sec * 1000000 + lRunTimeEnd.tv_usec;
//long long time2 = 0;
//long long time3 = 0 ;


            if (mG2DHandle > 0) {
            // if (false) {

#if 0
                g2d_compose((unsigned char *)(inbuffer[mComposeOrderIdx0].addrPhyY),
                            (unsigned char *)(inbuffer[mComposeOrderIdx1].addrPhyY),
                            (unsigned char *)(inbuffer[mComposeOrderIdx2].addrPhyY),
                            (unsigned char *)(inbuffer[mComposeOrderIdx3].addrPhyY),
                            mFrameWidth, mFrameHeight, (unsigned char *)writeBuffer->addrPhyY);
#else
                int infd[4];
                int src_w = mFrameWidth;
                int src_h = mFrameHeight;
                int dst_w = mFrameWidth * 2;
                int dst_h = mFrameHeight * 2;
                infd[0] = inbuffer[mComposeOrderIdx0].dmafd;
                infd[1] = inbuffer[mComposeOrderIdx1].dmafd;
                infd[2] = inbuffer[mComposeOrderIdx2].dmafd;
                infd[3] = inbuffer[mComposeOrderIdx3].dmafd;
                int outfd = ((ion_buffer_node*)writeBuffer->overlay_info)->fd_data.aw_fd;
                //int outfd = writeBuffer->dmafd;
                //CameraLogD("infd0 = %d outfd=%d, mComposeOrderIdx0=%d",infd[0],outfd,mComposeOrderIdx0);
                int flag = (int)G2D_ROT_0;
                int fmt = (int)G2D_FORMAT_YUV420UVC_V1U1V0U0;

                int ret = g2dClipByFd(mG2DHandle, infd[0], flag, fmt, src_w, src_h, 0, 0, src_w, src_h, outfd, fmt, src_w, src_h, 0, 0,          dst_w, dst_h);
                ret    |= g2dClipByFd(mG2DHandle, infd[1], flag, fmt, src_w, src_h, 0, 0, src_w, src_h, outfd, fmt, src_w, src_h, src_w, 0,      dst_w, dst_h);
                ret    |= g2dClipByFd(mG2DHandle, infd[2], flag, fmt, src_w, src_h, 0, 0, src_w, src_h, outfd, fmt, src_w, src_h, 0, src_h,      dst_w, dst_h);
                ret    |= g2dClipByFd(mG2DHandle, infd[3], flag, fmt, src_w, src_h, 0, 0, src_w, src_h, outfd, fmt, src_w, src_h, src_w, src_h, dst_w, dst_h);
                // int ret = g2d_compose(mG2DHandle,src_w,src_h,infd[0],infd[1],infd[2],infd[3],outfd,fmt);
                // int ret = g2d_xCompose(mG2DHandle,src_w,src_h,infd[0],infd[1],infd[2],infd[3],outfd);
                if (ret != 0) {
                    CameraLogE("g2d_compose: failed to call g2dClipByFd!!!");
                    return -1;
                }
#endif
            } else {
                composeBuffer4in1((unsigned char *)writeBuffer->addrVirY,
                                  (unsigned char *)(inbuffer[mComposeOrderIdx0].addrVirY),
                                  (unsigned char *)(inbuffer[mComposeOrderIdx1].addrVirY),
                                  (unsigned char *)(inbuffer[mComposeOrderIdx2].addrVirY),
                                  (unsigned char *)(inbuffer[mComposeOrderIdx3].addrVirY));
            }


            param_in.ion_buffer.fd_data.aw_fd = ((ion_buffer_node*)writeBuffer->overlay_info)->fd_data.aw_fd;
#if SAVE_FRAME
            count++;
            // if (count % 20 == 0) {
            if (count > 0) {
                FILE *pfile = NULL;
                char buffer[128] = {0};
                sprintf(buffer, "/tmp/nv21_%d.yuv", count);
                pfile = fopen(buffer, "w");
                if (pfile) {
                    fwrite((void *)writeBuffer->addrVirY, 1, mFrameWidth * mFrameHeight * 6, pfile);
                    fclose(pfile);
                }
            }
#endif
        }

        queueComposeBuf();

        for (i = 0; i < NB_CAMERA; i++) {
            if (inbuffer[i].addrVirY != 0) {
                // CameraLogD("--Camera[%d] buf:%d dmafd:%d  inbuffer.refCnt:%d",i,inbuffer[i].index,inbuffer[i].dmafd,inbuffer[i].refCnt);
                // CameraLogD("CameraManager release [camera %d].index=%d force= true",i*CAMERA_ID_STEP,inbuffer[i].index);
                mCameraHardware[i]->releasePreviewFrame(inbuffer[i].index,true);
            }

        }
    } else {
        //CameraLogW("preview thread get compose data too slow!!");
        CameraLogW("preview too slow!!");
    }

    //release buffer;

    return true;
}

void CameraManager::composeBuffer2in1(unsigned char *outBuffer, unsigned char *inBuffer0, unsigned char *inBuffer1)
{
    //origin V4L2CameraDevice composebuffer need 60 ms
    //int buf1 = buf[0].
    //copy y
    unsigned char *pbuf;
    unsigned char *pbuf0;
    unsigned char *pbuf1;
    int size_y = mFrameWidth * mFrameHeight;
    int size_uv = mFrameWidth * mFrameHeight / 2;

    if ((inBuffer0 == 0) || (inBuffer1 == 0)) {
        memset((void*)outBuffer, 0x10, size_y);
        memset((void*)(outBuffer + size_y), 0x80, size_uv);
        return;
    }

    memcpy(outBuffer, inBuffer0, size_y);
    memcpy(outBuffer + size_y, inBuffer1, size_y);

    //copy uv
    pbuf = outBuffer + size_y * 2;
    pbuf0 = inBuffer0 + size_y;
    pbuf1 = inBuffer1 + size_y;

    memcpy(pbuf, pbuf0, size_uv);
    memcpy(pbuf + size_uv, pbuf1, size_uv);


}

//#else
void CameraManager::composeBuffer4in1(unsigned char *outBuffer, unsigned char *inBuffer0, unsigned char *inBuffer1,
                                      unsigned char *inBuffer2, unsigned char *inBuffer3)
{
    //origin V4L2CameraDevice composebuffer need 60 ms
    //int buf1 = buf[0].
    //copy y
    int i = 0;
    int n = 0;
    unsigned char *pbuf;
    unsigned char *pbuf0;
    unsigned char *pbuf1;
    unsigned char *pbuf2;
    unsigned char *pbuf3;

    int composeW = mFrameWidth * 2;
    int composeH = mFrameHeight * 2;

    if ((inBuffer0 == 0) || (inBuffer1 == 0) || (inBuffer2 == 0) || (inBuffer3 == 0)) {
        memset((void*)outBuffer, 0x10, composeW * composeH);
        memset((void*)(outBuffer + composeW * composeH), 0x80, composeW * composeH / 2);
        return;
    }
    for (i = 0; i < mFrameHeight; i++) {
        memcpy(outBuffer + n * mFrameWidth, inBuffer0 + i * mFrameWidth, mFrameWidth);
        memcpy(outBuffer + (n + 1)*mFrameWidth, inBuffer1 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    pbuf = outBuffer + mFrameHeight * mFrameWidth * 2;
    n = 0;
    for (i = 0; i < mFrameHeight; i++) {
        memcpy(pbuf + n * mFrameWidth, inBuffer2 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, inBuffer3 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    //copy uv
    pbuf = outBuffer + mFrameWidth * mFrameHeight * 4;
    pbuf0 = inBuffer0 + mFrameWidth * mFrameHeight;
    pbuf1 = inBuffer1 + mFrameWidth * mFrameHeight;
    pbuf2 = inBuffer2 + mFrameWidth * mFrameHeight;
    pbuf3 = inBuffer3 + mFrameWidth * mFrameHeight;
    n = 0;
    for (i = 0; i < mFrameHeight / 2; i++) {
        memcpy(pbuf + n * mFrameWidth, pbuf0 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, pbuf1 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }
    pbuf = outBuffer + mFrameWidth * mFrameHeight * 5;
    n = 0;
    for (i = 0; i < mFrameHeight / 2; i++) {
        memcpy(pbuf + n * mFrameWidth, pbuf2 + i * mFrameWidth, mFrameWidth);
        memcpy(pbuf + (n + 1)*mFrameWidth, pbuf3 + i * mFrameWidth, mFrameWidth);
        n += 2;
    }

}

bool CameraManager::previewThread()
{
    dequeueComposeBuf();
    return true;
}


bool CameraManager::commandThread()
{
    int i;
    pthread_mutex_lock(&mCommandMutex);
    Queue_CM_Element * queue = (Queue_CM_Element *)OSAL_Dequeue(&mQueueCMCommand);
    if (queue == NULL) {
        CameraLogV("wait commond queue ......");
        pthread_cond_wait(&mCommandCond, &mCommandMutex);
        pthread_mutex_unlock(&mCommandMutex);
        return true;
    }
    pthread_mutex_unlock(&mCommandMutex);
    switch (queue->cmd) {
        case CMD_CM_START_PREVIEW: {
            for (i = 0; i < NB_CAMERA; i++) {
                if (NULL != mCameraHardware[i]) {
                    mCameraHardware[i]->startPreview();
                    mCameraHardware[i]->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);
                }

            }
            break;
        }
        case CMD_CM_STOP_PREVIEW: {
            int i = 0;
            pthread_mutex_lock(&mCameraMutex);
            for (i = 0; i < NB_CAMERA; i++) {
                mCameraBuf[i].buf_unused = 0;
            }

            pthread_mutex_unlock(&mCameraMutex);

            for (i = 0; i < NB_CAMERA; i++) {
                if (NULL != mCameraHardware[i]) {
                    mCameraHardware[i]->disableMsgType(CAMERA_MSG_PREVIEW_FRAME);
                    //mCameraHardware[i]->stopPreview();  //stopPreview will release v4l2 capthread
                }
            }

            break;
        }
        case CMD_CM_RELEASE_CAMERA: {
            int i = 0;
            pthread_mutex_lock(&mCameraMutex);
            for (i = 0; i < NB_CAMERA; i++) {
                mCameraBuf[i].buf_unused = 0;
            }
            pthread_mutex_unlock(&mCameraMutex);

            mManagerIsInited = 0;
            mPreviewThread->stopThread();
            mComposeThread->stopThread();
            mCommandThread->stopThread();

            for (i = NB_CAMERA - 1; i >= 0 ; i--) {
                mCameraHardware[i] = NULL;
            }
            break;
        }
        default:
            CameraLogW("unknown queue command: %d", queue->cmd);
            break;
    }
    return true;
}

void CameraManager::startPreview()
{
#if SAVE_FRAME
    count = 0;
#endif
    memset(mCameraState, 0, sizeof(bool)*NB_CAMERA);
    mAbandonFrameCnt = ABANDONFRAMECT;
    pthread_mutex_lock(&mCommandMutex);
    mQueueCMElement[CMD_CM_START_PREVIEW].cmd = CMD_CM_START_PREVIEW;
    OSAL_Queue(&mQueueCMCommand, &mQueueCMElement[CMD_CM_START_PREVIEW]);
    pthread_cond_signal(&mCommandCond);
    pthread_mutex_unlock(&mCommandMutex);

    pthread_mutex_lock(&mCameraMutex);
    mCaptureState = CAPTURE_STATE_READY;
    pthread_mutex_unlock(&mCameraMutex);
}

void CameraManager::stopPreview()
{

    pthread_mutex_lock(&mCameraMutex);
    mCaptureState = CAPTURE_STATE_PAUSE;
    pthread_mutex_unlock(&mCameraMutex);

    pthread_mutex_lock(&mCommandMutex);
    mQueueCMElement[CMD_CM_STOP_PREVIEW].cmd = CMD_CM_STOP_PREVIEW;
    OSAL_Queue(&mQueueCMCommand, &mQueueCMElement[CMD_CM_STOP_PREVIEW]);
    pthread_cond_signal(&mCommandCond);
    pthread_mutex_unlock(&mCommandMutex);

}

void CameraManager::releaseCamera()
{
    pthread_mutex_lock(&mCameraMutex);
    mCaptureState = CAPTURE_STATE_EXIT;
    pthread_mutex_unlock(&mCameraMutex);

    pthread_mutex_lock(&mCommandMutex);
    mQueueCMElement[CMD_CM_RELEASE_CAMERA].cmd = CMD_CM_RELEASE_CAMERA;
    OSAL_Queue(&mQueueCMCommand, &mQueueCMElement[CMD_CM_RELEASE_CAMERA]);
    pthread_cond_signal(&mCommandCond);
    pthread_mutex_unlock(&mCommandMutex);
}

void CameraManager::setAnalogInputColor(int brightness, int contrast, int saturation, int hue)
{
    brightness = brightness * 255 / 100;
    contrast   = contrast * 255 / 100;
    saturation = saturation * 255 / 100;
    hue        = hue * 255 / 100;
    for (int i = 0; i < NB_CAMERA; i++) {
        mCameraHardware[i]->setAnalogInputColor(brightness, contrast, saturation);
    }
}

status_t CameraManager::setCameraHardware(int index, CameraHardware *hardware)
{
    if (hardware == NULL) {
        CameraLogE("ERROR hardware info");
        return EINVAL;
    }
    mCameraHardware[index] = hardware;
    return NO_ERROR;
}

CameraManager::~CameraManager()
{
    CameraLogV("~CameraManager");
    if (mPreviewThread != NULL) {
        mPreviewThread->stopThread();
        pthread_mutex_lock(&mComposeMutex);
        pthread_cond_signal(&mComposeCond);
        pthread_mutex_unlock(&mComposeMutex);
        mPreviewThread->join();
        mPreviewThread.clear();
        mPreviewThread = 0;
    }

    if (mComposeThread != NULL) {
        mComposeThread->stopThread();
        pthread_mutex_lock(&mCameraMutex);
        pthread_cond_signal(&mCameraCond);
        pthread_mutex_unlock(&mCameraMutex);
        mComposeThread->join();
        mComposeThread.clear();
        mComposeThread = 0;
    }

    if (mCommandThread != NULL) {
        mCommandThread->stopThread();
        pthread_mutex_lock(&mCommandMutex);
        pthread_cond_signal(&mCommandCond);
        pthread_mutex_unlock(&mCommandMutex);
        mCommandThread->join();
        mCommandThread.clear();
        mCommandThread = 0;
    }

    pthread_mutex_destroy(&mCommandMutex);
    pthread_cond_destroy(&mCommandCond);

    pthread_mutex_destroy(&mComposeMutex);
    pthread_cond_destroy(&mComposeCond);

    pthread_mutex_destroy(&mCameraMutex);
    pthread_cond_destroy(&mCameraCond);
    OSAL_QueueTerminate(&mQueueCMCommand);

    composeBufDeinit();

    /*if(mIonFd>0){
        ::close(mIonFd);
        mIonFd=-1;
    }*/
    close_cedar_dev();
}

int CameraManager::g2d_scale(unsigned char *src, int src_width, int src_height, unsigned char *dst, int dst_width, int dst_height)
{
#if defined(_T507_) || defined(_T113_)
    CameraLogD("%s error. IC dont support g2d_scale", __FUNCTION__);
    return -1;
#else
    g2d_stretchblt scale;

    scale.flag = G2D_BLT_NONE;//G2D_BLT_NONE;//G2D_BLT_PIXEL_ALPHA|G2D_BLT_ROTATE90;
    scale.src_image.addr[0] = (__u64)src;
    scale.src_image.addr[1] = (__u64)src + src_width * src_height;
    scale.src_image.w = src_width;
    scale.src_image.h = src_height;
    scale.src_image.format = G2D_FMT_PYUV420UVC;
    scale.src_image.pixel_seq = G2D_SEQ_NORMAL;
    scale.src_rect.x = 0;
    scale.src_rect.y = 0;
    scale.src_rect.w = src_width;
    scale.src_rect.h = src_height;

    scale.dst_image.addr[0] = (__u64)dst;
    scale.dst_image.addr[1] = (__u64)dst + dst_width * dst_height;
    //scale.dst_image.addr[2] = (int)dst + dst_width * dst_height * 5 / 4;
    scale.dst_image.w = dst_width;
    scale.dst_image.h = dst_height;
    scale.dst_image.format = G2D_FMT_PYUV420UVC; // G2D_FMT_PYUV420UVC;
    scale.dst_image.pixel_seq = G2D_SEQ_NORMAL;
    scale.dst_rect.x = 0;
    scale.dst_rect.y = 0;
    scale.dst_rect.w = dst_width;
    scale.dst_rect.h = dst_height;
    scale.color = 0xff;
    scale.alpha = 0xff;

    if (ioctl(mG2DHandle, G2D_CMD_STRETCHBLT, &scale) < 0) {
        CameraLogE("g2d_scale: failed to call G2D_CMD_STRETCHBLT!!!");
        return -1;
    }
#endif
    return 0;
}

int CameraManager::g2d_clip(void* psrc, int src_w, int src_h, int src_x, int src_y, int width, int height, void* pdst, int dst_w, int dst_h, int dst_x, int dst_y)
{
#if defined(_T507_) || defined(_T113_)
    CameraLogD("%s error.pls use G2dApi.h's function instead", __FUNCTION__);
    return -1;
#else
    g2d_blt     blit_para;
    int         err;

    blit_para.src_image.addr[0]      = (__u64)psrc;
    blit_para.src_image.addr[1]      = (__u64)psrc + src_w * src_h;
    blit_para.src_image.w            = src_w;
    blit_para.src_image.h            = src_h;
    blit_para.src_image.format       = G2D_FMT_PYUV420UVC;
    blit_para.src_image.pixel_seq    = G2D_SEQ_NORMAL;
    blit_para.src_rect.x             = src_x;
    blit_para.src_rect.y             = src_y;
    blit_para.src_rect.w             = width;
    blit_para.src_rect.h             = height;

    blit_para.dst_image.addr[0]      = (__u64)pdst;
    blit_para.dst_image.addr[1]      = (__u64)pdst + dst_w * dst_h;
    blit_para.dst_image.w            = dst_w;
    blit_para.dst_image.h            = dst_h;
    blit_para.dst_image.format       = G2D_FMT_PYUV420UVC;
    blit_para.dst_image.pixel_seq    = G2D_SEQ_NORMAL;
    blit_para.dst_x                  = dst_x;
    blit_para.dst_y                  = dst_y;
    blit_para.color                  = 0xff;
    blit_para.alpha                  = 0xff;

    blit_para.flag = G2D_BLT_NONE;

    err = ioctl(mG2DHandle, G2D_CMD_BITBLT, (unsigned long)&blit_para);
    if (err < 0) {
        CameraLogE("g2d_clip: failed to call G2D_CMD_BITBLT!!!");
        return -1;
    }
#endif
    return 0;
}

int CameraManager::g2d_compose(void* psrc1, void* psrc2, void* psrc3, void* psrc4, int src_w, int src_h, void* pdst)
{
    int ret = -1;

    int dst_w = src_w * 2;
    int dst_h = src_h * 2;

#if defined(_T507_) || defined(_T113_)
    CameraLogD("%s is not available in this IC.", __FUNCTION__);
#else
    ret  = g2d_clip(psrc1, src_w, src_h, 0, 0, src_w, src_h, pdst, dst_w, dst_h, 0, 0);
    ret |= g2d_clip(psrc2, src_w, src_h, 0, 0, src_w, src_h, pdst, dst_w, dst_h, src_w, 0);
    ret |= g2d_clip(psrc3, src_w, src_h, 0, 0, src_w, src_h, pdst, dst_w, dst_h, 0, src_h);
    ret |= g2d_clip(psrc4, src_w, src_h, 0, 0, src_w, src_h, pdst, dst_w, dst_h, src_w, src_h);
#endif
    if (ret != 0) {
        CameraLogE("g2d_compose: failed to call g2dYuvspClip!!!");
        return -1;
    }

    return 0;
}

}

#endif
