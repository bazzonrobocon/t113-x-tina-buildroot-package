#define LOG_TAG "V4L2CameraDevice"
#include "CameraDebug.h"
#if DBG_V4L2_CAMERA
#define LOG_NDEBUG 0
#endif
#include "CameraLog.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/fb.h>

#include <sys/time.h>
#include "videodev2_new.h"
#include <gps.h>
// #include <CdxIon.h>

#include "V4L2CameraDevice2.h"
#include "CallbackNotifier.h"
#include "PreviewWindow.h"
#include "CameraHardware2.h"

#include "CameraManager.h"
#include "CameraFileCfg.h"

#ifdef NPU_DETECT_ENABLE
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglplatform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "drm_fourcc.h"
#include "REShader.h"
#include "renderengine.h"

#include "awnn_internal.h"
#include "awnn_lib.h"
#include "awnn_quantize.h"
#include "yolo_layer.h"
#include "yolov3_postprocess.h"

#include "awf_detection.h"

#ifdef NPU_DETECT_FACE_ENBALE
#define NPU_NG_FILE "/etc/face_det_v4.0.0_A523.nb"
#define NPU_INPUT_WIDTH 480
#define NPU_INPUT_HEIGHT 480
#else
#define NPU_INPUT_WIDTH 640
#define NPU_INPUT_HEIGHT 384
#define NPU_NG_FILE "/etc/4.0.0_Beta.nb"
#endif
#define NPU_INPUT_SIZE (NPU_INPUT_WIDTH * NPU_INPUT_HEIGHT * 3 / 2)

#endif

#define MAX_ADAS_SIZE (1920 * 1080 * 3 / 2)

#define V4L2_SAVEFRAME_PATH "/tmp/camera"

#define ENABLE_DEBUG_CAP_TIMEOUT 0

#if ENABLE_DEBUG_CAP_TIMEOUT
long long cap_base_time = 0;
#define CAP_TIMEOUT 500
#endif

#define CHECK_NO_ERROR(a)     \
    if (a != NO_ERROR) {      \
        if (mCameraFd != 0) { \
            close(mCameraFd); \
            mCameraFd = 0;    \
        }                     \
        return EINVAL;        \
    }

//#define WATERMARK_USE_MS 1

#if USE_MP_CONVERT || USE_G2D_COPY
using namespace g2dapi;
#endif

namespace android {

int createDirectory(const char *path) {
    // 使用 stat 函数检查文件夹是否存在
    struct stat st;
    if (stat(path, &st) == 0) {
        // 文件夹已经存在
        CameraLogD("Directory '%s' already exists.\n", path);
        return 0;
    } else {
        // 文件夹不存在，尝试创建它
        if (mkdir(path, 0777) == 0) {
            CameraLogD("Directory '%s' created successfully.\n", path);
            return 1;  // 表示成功创建文件夹
        } else {
            CameraLogE("Error creating directory");
            return -1;  // 表示创建文件夹失败
        }
    }
}

static void calculateCrop(Rect *rect, int new_zoom, int max_zoom, int width, int height) {
    if (max_zoom == 0) {
        rect->left = 0;
        rect->top = 0;
        rect->right = width - 1;
        rect->bottom = height - 1;
    } else {
        int new_ratio = (new_zoom * 2 * 100 / max_zoom + 100);
        rect->left = (width - (width * 100) / new_ratio) / 2;
        rect->top = (height - (height * 100) / new_ratio) / 2;
        rect->right = rect->left + (width * 100) / new_ratio - 1;
        rect->bottom = rect->top + (height * 100) / new_ratio - 1;
    }

    // CameraLogD("crop: [%d, %d, %d, %d]", rect->left, rect->top, rect->right, rect->bottom);
}

static void YUYVToNV21(const void *yuyv, void *nv21, int width, int height) {
    uint8_t *Y = (uint8_t *)nv21;
    uint8_t *VU = (uint8_t *)Y + width * height;
    *Y = 1;

    for (int i = 0; i < height; i += 2) {
        for (int j = 0; j < width; j++) {
            *(uint8_t *)((uint8_t *)Y + i * width + j) =
                *(uint8_t *)((uint8_t *)yuyv + i * width * 2 + j * 2);
            *(uint8_t *)((uint8_t *)Y + (i + 1) * width + j) =
                *(uint8_t *)((uint8_t *)yuyv + (i + 1) * width * 2 + j * 2);

            if (j % 2) {
                if (j < width - 1) {
                    *(uint8_t *)((uint8_t *)VU + ((i * width) >> 1) + j) =
                        *(uint8_t *)((uint8_t *)yuyv + i * width * 2 + (j + 1) * 2 + 1);
                }
            } else {
                if (j > 1) {
                    *(uint8_t *)((uint8_t *)VU + ((i * width) >> 1) + j) =
                        *(uint8_t *)((uint8_t *)yuyv + i * width * 2 + (j - 1) * 2 + 1);
                }
            }
        }
    }
}

#if V4L2_SAVEFRAME
static int saveframe(char *str, void *p, int length, int is_oneframe) {
    FILE *fd;

    if (is_oneframe) {
        fd = fopen(str, "wb");
    } else {
        fd = fopen(str, "a");
    }

    if (!fd) {
        CameraLogE("Open file error");
        return -1;
    }
    int written = 0;
    if ((written = fwrite(p, 1, length, fd)) == length) {
        CameraLogD("Write file successfully");
        fclose(fd);
        return 0;
    } else {
        CameraLogE("Write file fail %s", strerror(errno));
        fclose(fd);
        return -1;
    }
}
#endif

#ifdef ADAS_ENABLE
unsigned char *image_buf;
status_t V4L2CameraDevice::adasInitialize() {
    if (mAdasStarted) {
        CameraLogW("adas has inited!");
        return 0;
    }
    adasIgnorFrameCnt = 0;
    image_buf = (unsigned char *)malloc(1920 * 1080 * 3 / 2);
    mAdasThread = new DoAdasThread(this);
    mAdasThread->startThread();
    mAdasStarted = true;
    return 0;
}

bool V4L2CameraDevice::adasThread() {
    pthread_mutex_lock(&mAdasMutex);
    pthread_cond_wait(&mAdasCond, &mAdasMutex);

    if (!image_buf) {
        CameraLogE("adasThread:image_buf = NULL");
        return -1;
    }

    if (mCurV4l2Buf != NULL) memcpy(image_buf, mCurV4l2Buf, ADAS_WIDTH * ADAS_HEIGHT * 3 / 2);
    if (mAdasStarted) fcw_execute(image_buf);
    pthread_mutex_unlock(&mAdasMutex);

    ADASOUT_IF adasResult;
    memcpy(&adasResult.ldw_result, &g_ldw_result, sizeof(g_ldw_result));
    memcpy(&adasResult.fcw_result, &g_fcw_result, sizeof(g_fcw_result));
    // CameraLogE("adasThread##########vanishLine =%f",adasResult.ldw_result.autoCalVanishLine);
    mCallbackNotifier->adasResultMsg(&adasResult);
    return true;
}

void V4L2CameraDevice::adasDestroy() {
    if (!mAdasStarted) {
        CameraLogW("Adas is not running yet!");
        return;
    }
    CameraLogI("<adasDestroy>start");
    mAdasStarted = false;
    pthread_mutex_lock(&mAdasMutex);
    fcw_close();
    pthread_mutex_unlock(&mAdasMutex);
    if (mAdasThread != NULL) {
        mAdasThread->stopThread();
        pthread_mutex_lock(&mAdasMutex);
        pthread_cond_signal(&mAdasCond);
        pthread_mutex_unlock(&mAdasMutex);
        mAdasThread.clear();
        mAdasThread = 0;
    }

    if (image_buf) {
        free(image_buf);
        image_buf = NULL;
    }
    CameraLogI("<adasDestroy>end");
}

int V4L2CameraDevice::setAdasParameters(int key, int value) {
    return fcw_setAdasParameters(key, value);
}
#endif

int loadFile(V4L2BUF_t mem, const char *path, int size)
{
	FILE *file;
	ssize_t n;

	file = fopen(path, "w+");
	if (file < 0) {
		CameraLogE("fopen %s err.\n", path);
		return -1;
	}

	n = fwrite(mem.addrVirY, 1, size, file);
	if (n < size) {
		CameraLogE("fwrite is unormal, wrote totally:%d  but need:%ld\n",
			(unsigned int)n, size);
	}

	fclose(file);

	return 0;
}

#ifdef NPU_DETECT_ENABLE
void V4L2CameraDevice::npuInitialize(void) {
    // npu init
#ifdef NPU_DETECT_FACE_ENBALE
    char* nbg = NPU_NG_FILE;
    if (!mAwf_Container)
        mAwf_Container = (AWF_Det_Container *)malloc(sizeof(AWF_Det_Container));
    if (!mAwf_Outputs)
        mAwf_Outputs =(AWF_Det_Outputs *)malloc(sizeof(AWF_Det_Container));

    awf_make_det_container((AWF_Det_Container *)mAwf_Container, nbg,
        (AWF_Det_Outputs *)mAwf_Outputs);
    float confid_thrs = 0.3;
    float nms_thrs = 0.45;
    awf_set_det_runtime((AWF_Det_Container *)mAwf_Container,  confid_thrs,  nms_thrs);
#else
    const char* nbg = NPU_NG_FILE;
    awnn_init();
    mAwnn_context = awnn_create(nbg);
#endif

}

void V4L2CameraDevice::npuDestroy(void) {
    renderEngineDestroy((RenderEngine_t)mRender);

#ifdef NPU_DETECT_FACE_ENBALE
    awf_free_det_container((AWF_Det_Container *)mAwf_Container,
        (AWF_Det_Outputs *)mAwf_Outputs);
    free(mAwf_Container);
    free(mAwf_Outputs);
    mAwf_Outputs = NULL;
    mAwf_Container = NULL;
#else
    awnn_destroy((Awnn_Context_t *)mAwnn_context);
    mAwnn_context = NULL;
    // npu uninit
    awnn_uninit();
#endif

}

#endif

void V4L2CameraDevice::showformat(int format, char *str) {
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_YUYV", str);
            break;
        case V4L2_PIX_FMT_MJPEG:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_MJPEG", str);
            break;
        case V4L2_PIX_FMT_YVU420:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_YVU420", str);
            break;
        case V4L2_PIX_FMT_NV12:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_NV12", str);
            break;
        case V4L2_PIX_FMT_NV21:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_NV21", str);
            break;
        case V4L2_PIX_FMT_H264:
            CameraLogD("The %s foramt is V4L2_PIX_FMT_H264", str);
            break;
        default:
            CameraLogD("The %s format can't be showed", str);
    }
}

V4L2CameraDevice::V4L2CameraDevice(CameraHardware *camera_hal, PreviewWindow *preview_window,
                                   CallbackNotifier *cb)
    : mCameraHardware(camera_hal),
      mPreviewWindow(preview_window),
      mCallbackNotifier(cb),
      mCameraDeviceState(STATE_CONSTRUCTED),
      mCaptureThreadState(CAPTURE_STATE_NULL),
      mCameraFd(0),
      mCameraId(0),
      mFrameRate(30),
      mTakePictureState(TAKE_PICTURE_NULL),
      mIsPicCopy(false),
      mFrameWidth(1280),
      mFrameHeight(720),
      mThumbWidth(0),
      mThumbHeight(0),
      mCurFrameTimestamp(0),
      nPlanes(0),
      mBufferCnt(NB_BUFFER),
      mUseHwEncoder(false),
      mNewZoom(0),
      mLastZoom(-1),
      mMaxZoom(0xffffffff),
#if USE_MP_CONVERT || USE_G2D_COPY
      mV4l2G2DHandle(0),
#endif
      mCanBeDisconnected(false),
      mVideoHint(false),
      mCurAvailBufferCnt(0),
      mStatisicsIndex(0),
      mNeedHalfFrameRate(false),
      mShouldPreview(true),
      mIsThumbUsedForVideo(false),
      mVideoWidth(640),
      mVideoHeight(480),
#ifdef CDX_FOR_PURE_H264
      mDecoder(NULL),
#endif
      mPicCnt(0),
      mOrgTime(0),
#ifdef ADAS_ENABLE
      mCurV4l2Buf(NULL),
      mAdasStarted(false),
      adasIgnorFrameCnt(0),
#endif
#ifdef WATERMARK
      mWaterMarkEnable(false),
      mWaterMarkCtrlRec(NULL),
      mWaterMarkCtrlPrev(NULL),
      mWaterMarkMultiple(NULL),
      mWaterMarkDispMode(WATER_MARK_DISP_MODE_VIDEO_AND_PREVIEW),
#endif
      mCameraFlipStatus(NO_FLIP),
#ifdef CAMERA_MANAGER_ENABLE
      mCameraManager(NULL),
#endif
#ifdef NPU_DETECT_ENABLE
    mRender(NULL),
    mAwnn_context(NULL),
#endif
#if defined NPU_DETECT_ENABLE && defined NPU_DETECT_FACE_ENBALE
    mAwf_Container(NULL),
    mAwf_Outputs(NULL),
#endif
    misGLContextInited(false)
{
    CameraLogV("V4L2CameraDevice construct");

    memset(&mV4l2Info, 0, sizeof(mV4l2Info));
    memset(&mMapMem, 0, sizeof(v4l2_mem_map_t));
    memset(&mVideoBuffer, 0, sizeof(bufferManagerQ_t));
#if USE_G2D_COPY
    memset(&mG2DBuffer, 0, sizeof(bufferManagerQ_t));
#endif

#if V4L2_SAVEFRAME
    int ret = createDirectory(V4L2_SAVEFRAME_PATH);
    if (ret < 0) {
        CameraLogE("createDirectory for %s fail");
        return;
    }
#endif

#ifdef CDX_FOR_PURE_H264
    memset(&mVideoConf, 0, sizeof(VConfig));
    memset(&mVideoInfo, 0, sizeof(VideoStreamInfo));
    memset(&mDataInfo, 0, sizeof(VideoStreamDataInfo));
#endif
    // init preview buffer queue
    OSAL_QueueCreate(&mQueueBufferPreview, NB_BUFFER);
    OSAL_QueueCreate(&mQueueBufferPicture, NB_BUFFER);
#ifdef NPU_DETECT_ENABLE
    OSAL_QueueCreate(&mQueueBufferNpuDetect, NB_BUFFER);
#endif

    mGpsData = NULL;
#ifdef WATERMARK
    pthread_mutex_init(&mWaterMarkLock, NULL);
#endif

#if USE_MP_CONVERT || USE_G2D_COPY
    CameraLogD("open G2D device");
    mV4l2G2DHandle = g2dInit();
    if (mV4l2G2DHandle < 0) {
        CameraLogE("open /dev/g2d failed");
    }
#endif

#ifdef ADAS_ENABLE
    pthread_mutex_init(&mAdasMutex, NULL);
    pthread_cond_init(&mAdasCond, NULL);
#endif
    pthread_mutex_init(&mConnectMutex, NULL);
    pthread_cond_init(&mConnectCond, NULL);

    // init capture thread
    mCaptureThread = new DoCaptureThread(this);
    pthread_mutex_init(&mCaptureMutex, NULL);
    pthread_cond_init(&mCaptureCond, NULL);
    pthread_mutex_lock(&mCaptureMutex);
    mCaptureThreadState = CAPTURE_STATE_PAUSED;
    pthread_mutex_unlock(&mCaptureMutex);
    mCaptureThread->startThread();

    // init preview thread
    mPreviewThread = new DoPreviewThread(this);
    pthread_mutex_init(&mPreviewMutex, NULL);
    pthread_cond_init(&mPreviewCond, NULL);
    mPreviewThread->startThread();

    // init picture thread
    mPictureThread = new DoPictureThread(this);
    pthread_mutex_init(&mPictureMutex, NULL);
    pthread_cond_init(&mPictureCond, NULL);
    mPictureThread->startThread();
#ifdef NPU_DETECT_ENABLE
    // init picture thread
    mNpuWorkThread = new DoNpuWorkThread(this);
    pthread_mutex_init(&mNpuWorkMutex, NULL);
    pthread_cond_init(&mNpuWorkCond, NULL);
    mNpuWorkThread->startThread();
#endif
}

V4L2CameraDevice::~V4L2CameraDevice() {
    CameraLogV("V4L2CameraDevice disconstruct");

    if (mCaptureThread != NULL) {
        mCaptureThread->stopThread();
        CameraLogV("V4L2CameraDevice disconstruct:waiting for video signal timeout");
        // usleep(2200000);//wait for video signal timeout
        pthread_mutex_lock(&mCaptureMutex);
        pthread_cond_signal(&mCaptureCond);
        pthread_mutex_unlock(&mCaptureMutex);
        mCaptureThread->join();
        mCaptureThread.clear();
        mCaptureThread = 0;
    }

    if (mPreviewThread != NULL) {
        mPreviewThread->stopThread();
        pthread_mutex_lock(&mPreviewMutex);
        pthread_cond_signal(&mPreviewCond);
        pthread_mutex_unlock(&mPreviewMutex);
        mPreviewThread->join();
        mPreviewThread.clear();
        mPreviewThread = 0;
    }

    if (mPictureThread != NULL) {
        mPictureThread->stopThread();
        pthread_mutex_lock(&mPictureMutex);
        pthread_cond_signal(&mPictureCond);
        pthread_mutex_unlock(&mPictureMutex);
        mPictureThread->join();
        mPictureThread.clear();
        mPictureThread = 0;
    }
#ifdef NPU_DETECT_ENABLE
    if (mNpuWorkThread != NULL) {
        mNpuWorkThread->stopThread();
        pthread_mutex_lock(&mNpuWorkMutex);
        pthread_cond_signal(&mNpuWorkCond);
        pthread_mutex_unlock(&mNpuWorkMutex);
        mNpuWorkThread->join();
        mNpuWorkThread.clear();
        mNpuWorkThread = 0;
    }
#endif
    pthread_mutex_destroy(&mCaptureMutex);
    pthread_cond_destroy(&mCaptureCond);

    pthread_mutex_destroy(&mPreviewMutex);
    pthread_cond_destroy(&mPreviewCond);

    pthread_mutex_destroy(&mPictureMutex);
    pthread_cond_destroy(&mPictureCond);
#ifdef NPU_DETECT_ENABLE
    pthread_mutex_destroy(&mNpuWorkMutex);
    pthread_cond_destroy(&mNpuWorkCond);
#endif
    pthread_mutex_destroy(&mConnectMutex);
    pthread_cond_destroy(&mConnectCond);

#ifdef WATERMARK
    pthread_mutex_destroy(&mWaterMarkLock);
#endif

#ifdef ADAS_ENABLE
    pthread_mutex_destroy(&mAdasMutex);
    pthread_cond_destroy(&mAdasCond);
#endif

    // close v4l2 camera device
    closeCameraDev();

#if USE_MP_CONVERT
    if (mV4l2G2DHandle != -1) {
        g2dUnit(mV4l2G2DHandle);
        mV4l2G2DHandle = -1;
    }
#endif

#ifdef USE_MP_CONVERT
    if (mV4l2_G2dAddrVir != 0) {
        mV4l2CameraMemops.vir = mV4l2_G2dAddrVir;
        allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mV4l2_G2dAddrVir = 0;
    }
#endif

#ifdef ADAS_ENABLE
    if (mAdasBuffer.addrVirY != 0) {
        mV4l2CameraMemops.vir = mAdasBuffer.addrVirY;
        allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mAdasBuffer.addrVirY = 0;
    }
#endif

#ifdef NPU_DETECT_ENABLE
    npuDestroy();
#endif

    for (int i = 0; i < NB_BUFFER; i++) {
        releasePreviewFrame(i);
    }

    if (mV4l2CameraMemops.ops != NULL) {
        allocClose(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mV4l2CameraMemops.ops = NULL;
    }

    int ret = OSAL_QueueTerminate(&mQueueBufferPreview);
    if (ret < 0) {
        CameraLogE("v4l2 mQueueBufferPreview terminate failed,ret=%d", ret);
    }
    ret = OSAL_QueueTerminate(&mQueueBufferPicture);
    if (ret < 0) {
        CameraLogE("v4l2 mQueueBufferPicture terminate failed,ret=%d", ret);
    }
#ifdef NPU_DETECT_ENABLE
    ret = OSAL_QueueTerminate(&mQueueBufferNpuDetect);
    if (ret < 0) {
        CameraLogE("v4l2 mQueueBufferNpuDetect terminate failed,ret=%d", ret);
    }
#endif
}

/****************************************************************************
 * V4L2CameraDevice interface implementation.
 ***************************************************************************/
void V4L2CameraDevice::setCameraFlipStatus(int flipEnable) {
    mCameraFlipStatus = flipEnable;
    CameraLogV("%s : mCameraFlipStatus = %d", __FUNCTION__, mCameraFlipStatus);
}

status_t V4L2CameraDevice::connectDevice(HALCameraInfo *halInfo) {
    Mutex::Autolock locker(&mObjectLock);

    if (isConnected()) {
        CameraLogW("%s: camera device is already connected.", __FUNCTION__);
        return NO_ERROR;
    }
    CameraLogV("begin openCameraDev");

    // open v4l2 camera device
    int ret = openCameraDev(halInfo);
    if (ret != OK) {
        return ret;
    }
    mCameraId = halInfo->device_id;
    CameraLogD("connectDevice device_id:%d", halInfo->device_id);
    memcpy((void *)&mV4l2Info, (void *)halInfo, sizeof(HALCameraInfo));

    memset(&mV4l2CameraMemops, 0, sizeof(dma_mem_des_t));
    ret = allocOpen(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    if (ret < 0) {
        CameraLogE("connectDevice:allocOpen failed");
        goto END_ERROR;
    }

    mV4l2CameraMemops.size = MAX_PICTURE_SIZE;
    ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    if (ret < 0) {
        CameraLogE("connectDevice:allocAlloc failed");
        goto END_ERROR;
    }
    memset((void *)mV4l2CameraMemops.vir, 0, MAX_PICTURE_SIZE);

    if ((mCameraManager != NULL) && (mCameraManager->isOviewEnable())) {
        // mCameraManager->setScMemOpsS(&mV4l2CameraMemops);
    }

#ifdef USE_MP_CONVERT
    mV4l2CameraMemops.size = G2D_BUFFER_SIZE;
    ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    if (ret < 0) {
        CameraLogE("USE_MP_CONVERT allocAlloc failed");
    }
    mV4l2_G2dAddrVir = mV4l2CameraMemops.vir;
    mV4l2_G2dAddrPhy = mV4l2CameraMemops.phy;
    mV4l2_G2dAddrSharefd = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;
#endif

#ifdef ADAS_ENABLE
    mV4l2CameraMemops.size = MAX_ADAS_SIZE;
    ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    if (ret < 0) {
        CameraLogE("ADAS_ENABLE allocAlloc failed");
        goto END_ERROR;
    }
    mAdasBuffer.addrVirY = mV4l2CameraMemops.vir;
    mAdasBuffer.addrPhyY = mV4l2CameraMemops.phy;
    mAdasBuffer.dmafd = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;
#endif

#ifdef NPU_DETECT_ENABLE
#ifdef NPU_DETECT_FACE_ENBALE
    mV4l2CameraMemops.size = NPU_INPUT_WIDTH * NPU_INPUT_HEIGHT * 3;
#else
    mV4l2CameraMemops.size = NPU_INPUT_WIDTH * NPU_INPUT_HEIGHT * 3/ 2;
#endif
    ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    if (ret < 0) {
        CameraLogE("ADAS_ENABLE allocAlloc failed");
        goto END_ERROR;
    }
    mNpuBuffer.addrVirY = mV4l2CameraMemops.vir;
    mNpuBuffer.addrPhyY = mV4l2CameraMemops.phy;
    mNpuBuffer.dmafd = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;
#endif

    /* There is a device to connect to. */
    mCameraDeviceState = STATE_CONNECTED;

    return NO_ERROR;

END_ERROR:
    if (mCameraFd != 0) {
        close(mCameraFd);
        mCameraFd = 0;
    }

#if USE_MP_CONVERT
    if (mV4l2G2DHandle != 0) {
        close(mV4l2G2DHandle);
        mV4l2G2DHandle = 0;
    }
#endif
    return UNKNOWN_ERROR;
}

status_t V4L2CameraDevice::disconnectDevice() {
    CAMERA_LOG;

    Mutex::Autolock locker(&mObjectLock);

    if (!isConnected()) {
        CameraLogW("%s: camera device is already disconnected.", __FUNCTION__);
        return NO_ERROR;
    }

    if (isStarted()) {
        CameraLogE("%s: Cannot disconnect from the started device.", __FUNCTION__);
        return -EINVAL;
    }

    // close v4l2 camera device
    closeCameraDev();

#if USE_MP_CONVERT
    if (mV4l2G2DHandle != 0) {
        close(mV4l2G2DHandle);
        mV4l2G2DHandle = 0;
    }
#endif

#ifdef USE_MP_CONVERT
    if (mV4l2_G2dAddrVir != 0) {
        mV4l2CameraMemops.vir = mV4l2_G2dAddrVir;
        allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mV4l2_G2dAddrVir = 0;
    }
#endif

#ifdef ADAS_ENABLE
    if (mAdasBuffer.addrVirY != 0) {
        mV4l2CameraMemops.vir = mAdasBuffer.addrVirY;
        allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mAdasBuffer.addrVirY = 0;
    }t
#endif

#ifdef NPU_DETECT_ENABLE
    if (mNpuBuffer.addrVirY != 0) {
        mV4l2CameraMemops.vir = mNpuBuffer.addrVirY;
        allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        mNpuBuffer.addrVirY = 0;
    }
#endif

    /* There is no device to disconnect from. */
    mCameraDeviceState = STATE_CONSTRUCTED;

    return NO_ERROR;
}

status_t V4L2CameraDevice::startDevice(int width, int height, uint32_t pix_fmt, bool video_hint) {
    CameraLogD("%s, width&hight:%dx%d, fmt:%d", __FUNCTION__, width, height, pix_fmt);

    Mutex::Autolock locker(&mObjectLock);

    if (!isConnected()) {
        CameraLogE("%s: camera device is not connected.", __FUNCTION__);
        return EINVAL;
    }

    if (isStarted()) {
        CameraLogE("%s: camera device is already started.", __FUNCTION__);
        return EINVAL;
    }

    mVideoFormat = pix_fmt;
    mVideoHint = video_hint;
    mCanBeDisconnected = false;

#ifdef ADAS_ENABLE
    mCurV4l2Buf = NULL;
#endif

    int ret = v4l2SetVideoParams(width, height, pix_fmt);
    if (ret < 0) {
        CameraLogE("v4l2SetVideoParams failed");
        return EINVAL;
    }
    // set capture mode and fps
    v4l2setCaptureParams();

    // v4l2 request buffers
    int buf_cnt = NB_BUFFER;
    CHECK_NO_ERROR(v4l2ReqBufs(&buf_cnt));
    mBufferCnt = buf_cnt;
    mCurAvailBufferCnt = mBufferCnt;

    // v4l2 query buffers
    CHECK_NO_ERROR(v4l2QueryBuf());

#ifdef CAMERA_MANAGER_ENABLE
    if ((mCameraManager != NULL) && (mCameraManager->isOviewEnable())) {
        mCameraManager->setFrameSize(mV4l2Info.device_id, mFrameWidth, mFrameHeight);
    }
#endif

    // stream on the v4l2 device
    CHECK_NO_ERROR(v4l2StartStreaming());

    mCameraDeviceState = STATE_STARTED;

#ifdef CDX_FOR_PURE_H264
    if ((mCaptureFormat == V4L2_PIX_FMT_H264) || (mCaptureFormat == V4L2_PIX_FMT_MJPEG)) {
        CameraLogD("F%s,L:%d init video Decoder!", __FUNCTION__, __LINE__);
        //* all decoder support YV12 format.
        mVideoConf.eOutputPixelFormat =
            PIXEL_FORMAT_NV21;  // PIXEL_FORMAT_YV12;PIXEL_FORMAT_YUV_MB32_420;//
        //* never decode two picture when decoding a thumbnail picture.
        mVideoConf.bDisable3D = 1;
        mVideoConf.nVbvBufferSize = 0;

        // mVideoConf.memops = MemAdapterGetOpsS();
        mVideoConf.memops = (struct ScMemOpsS *)mV4l2CameraMemops.ops;

        mVideoInfo.eCodecFormat = (mCaptureFormat == V4L2_PIX_FMT_MJPEG) ? VIDEO_CODEC_FORMAT_MJPEG
                                                                         : VIDEO_CODEC_FORMAT_H264;
        mVideoInfo.nWidth = mFrameWidth;
        mVideoInfo.nHeight = mFrameHeight;
        mVideoInfo.nFrameRate = mFrameRate;
        mVideoInfo.nFrameDuration = 1000 * 1000 / mFrameRate;
        mVideoInfo.nAspectRatio = 1000;
        mVideoInfo.bIs3DStream = 0;
        mVideoInfo.nCodecSpecificDataLen = 0;
        mVideoInfo.pCodecSpecificData = NULL;

        Libve_init2(&mDecoder, &mVideoInfo, &mVideoConf);
        if (mDecoder == NULL) {
            CameraLogE("FUNC:%s, Line:%d ", __FUNCTION__, __LINE__);
        }
    }
#endif
    return NO_ERROR;
}

status_t V4L2CameraDevice::stopDevice() {
    CameraLogD("V4L2CameraDevice::stopDevice");

    pthread_mutex_lock(&mConnectMutex);
    if (!mCanBeDisconnected) {
        CameraLogW("wait until capture thread pause or exit");
        pthread_cond_wait(&mConnectCond, &mConnectMutex);
    }
    pthread_mutex_unlock(&mConnectMutex);

    Mutex::Autolock locker(&mObjectLock);

    if (!isStarted()) {
        CameraLogW("%s: camera device is not started.", __FUNCTION__);
        return NO_ERROR;
    }

    // v4l2 device stop stream
    v4l2StopStreaming();

    // v4l2 device unmap buffers
    v4l2UnmapBuf();

    for (int i = 0; i < NB_BUFFER; i++) {
        memset(&mV4l2buf[i], 0, sizeof(V4L2BUF_t));
    }

    mCameraDeviceState = STATE_CONNECTED;

    mLastZoom = -1;

#ifdef ADAS_ENABLE
    mCurV4l2Buf = NULL;
#endif

#ifdef CDX_FOR_PURE_H264
    if (mCaptureFormat == V4L2_PIX_FMT_H264) {
        Libve_exit2(&mDecoder);
    }
#endif

    return NO_ERROR;
}

status_t V4L2CameraDevice::startDeliveringFrames() {
    CAMERA_LOG;

    pthread_mutex_lock(&mCaptureMutex);

    if (mCaptureThreadState == CAPTURE_STATE_NULL) {
        CameraLogE("error state of capture thread, %s", __FUNCTION__);
        pthread_mutex_unlock(&mCaptureMutex);
        return EINVAL;
    }
    if (mCaptureThreadState == CAPTURE_STATE_STARTED) {
        CameraLogW("capture thread has already started");
        pthread_mutex_unlock(&mCaptureMutex);
        return NO_ERROR;
    }

    // singal to start capture thread
    mCaptureThreadState = CAPTURE_STATE_STARTED;
    pthread_cond_signal(&mCaptureCond);
    pthread_mutex_unlock(&mCaptureMutex);

    return NO_ERROR;
}

status_t V4L2CameraDevice::stopDeliveringFrames() {
    CAMERA_LOG;

    pthread_mutex_lock(&mCaptureMutex);
    if (mCaptureThreadState == CAPTURE_STATE_NULL) {
        CameraLogE("error state of capture thread, %s", __FUNCTION__);
        pthread_mutex_unlock(&mCaptureMutex);
        return EINVAL;
    }

    if (mCaptureThreadState == CAPTURE_STATE_PAUSED) {
        CameraLogW("capture thread has already paused");
        pthread_mutex_unlock(&mCaptureMutex);
        return NO_ERROR;
    }

    mCaptureThreadState = CAPTURE_STATE_PAUSED;
    pthread_mutex_unlock(&mCaptureMutex);

    return NO_ERROR;
}

/****************************************************************************
 * Worker thread management.
 ***************************************************************************/
int V4L2CameraDevice::v4l2WaitCameraReady() {
    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    FD_SET(mCameraFd, &fds);

    /* Timeout */
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    r = select(mCameraFd + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        CameraLogE("select err, %s", strerror(errno));
        return -1;
    } else if (r == 0) {
        CameraLogE("select timeout %s", mV4l2Info.device_name);
        int i;
        V4L2BUF_t *p = NULL;
        for (i = 0; i < NB_BUFFER; i++) {
            p = &mV4l2buf[i];
            if (p->refCnt > 0) {
                CameraLogE("select timeout force %d free", p->index);
                releasePreviewFrame(p->index);
            }
        }

        return -1;
    }

    return 0;
}

void V4L2CameraDevice::singalDisconnect() {
    pthread_mutex_lock(&mConnectMutex);
    mCanBeDisconnected = true;
    pthread_cond_signal(&mConnectCond);
    pthread_mutex_unlock(&mConnectMutex);
}

int V4L2CameraDevice::testFrameRate() {
    mPicCnt++;
    if (mPicCnt >= 1500) {
        timeval cur_time;
        gettimeofday(&cur_time, NULL);
        const uint64_t now_time = cur_time.tv_sec * 1000000LL + cur_time.tv_usec;
        int framerate = mPicCnt * 1000 / ((now_time - mOrgTime) / 1000);
        if (framerate) {
            if (mCameraType == CAMERA_TYPE_UVC) {
                CameraLogD("Mic UVC framerate = %dfps@%dx%d ", framerate, mFrameWidth,
                           mFrameHeight);
            } else if (mCameraType == CAMERA_TYPE_CSI) {
                CameraLogD("Mic CSI framerate = %dfps@%dx%d ", framerate, mFrameWidth,
                           mFrameHeight);
            } else {
                CameraLogD("Mic TVIN framerate = %dfps@%dx%d ", framerate, mFrameWidth,
                           mFrameHeight);
            }
        }
        mOrgTime = now_time;
        mPicCnt = 0;
    }
    return 0;
}

#ifdef WATERMARK
static void getTimeString(char *str, int len) {
    time_t timep;
    struct tm *p;
    int year, month, day, hour, min, sec;

    time(&timep);
    p = localtime(&timep);
    year = p->tm_year + 1900;
    month = p->tm_mon + 1;
    day = p->tm_mday;
    hour = p->tm_hour;
    min = p->tm_min;
    sec = p->tm_sec;

#ifndef WATERMARK_USE_MS
    snprintf(str, len, "%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
#else
    struct timeval t;
    t.tv_sec = t.tv_usec = 0;
    gettimeofday(&t, NULL);

    snprintf(str, len, "%d-%02d-%02d %02d:%02d:%02d.%02d", year, month, day, hour, min, sec,
             t.tv_usec / 10000);
#endif
}

void V4L2CameraDevice::waterMarkinit() {
    if (mWaterMarkEnable) {
        CameraLogW("Water mark is already running!");
        return;
    }
    CameraLogD("waterMarkinit");
    if (mFrameHeight > 1080) {
        mWaterMarkCtrlRec = initialWaterMark(1080);
    } else {
        mWaterMarkCtrlRec = initialWaterMark(mFrameHeight);
    }

    if (NULL == mWaterMarkCtrlRec) {
        CameraLogE("Fail to initialize record water mark!");
        return;
    }
    mWaterMarkCtrlPrev = initialWaterMark(540);
    if (NULL == mWaterMarkCtrlPrev) {
        CameraLogE("Fail to initialize preview water mark!");
        return;
    }
    mWaterMarkMultiple = initWaterMarkMultiple();

    mWaterMarkEnable = true;
}

void V4L2CameraDevice::waterMarkDestroy() {
    if (!mWaterMarkEnable) {
        CameraLogW("Water mark is not running yet!");
        return;
    }
    CameraLogI("<waterMarkDestroy>");
    pthread_mutex_lock(&mWaterMarkLock);
    mWaterMarkEnable = false;
    if (mWaterMarkCtrlRec != NULL) {
        releaseWaterMark(mWaterMarkCtrlRec);
        mWaterMarkCtrlRec = NULL;
    }
    if (mWaterMarkCtrlPrev != NULL) {
        releaseWaterMark(mWaterMarkCtrlPrev);
        mWaterMarkCtrlPrev = NULL;
    }

    if (mWaterMarkMultiple != NULL) {
        releaseWaterMarkMultiple(mWaterMarkMultiple);
        mWaterMarkMultiple = NULL;
    }

    pthread_mutex_unlock(&mWaterMarkLock);
}

void V4L2CameraDevice::setWaterMarkDispMode(int dispMode) {
    CameraLogI("F:%s,L:%d,dispMode:%d", __FUNCTION__, __LINE__, dispMode);
    mWaterMarkDispMode = dispMode;
}

void V4L2CameraDevice::addWaterMark(unsigned char *addrVirY, int width, int height) {
    CAMERA_LOG;
    pthread_mutex_lock(&mWaterMarkLock);
    if (mWaterMarkEnable && mWaterMarkCtrlRec != NULL && mWaterMarkMultiple != NULL) {
        mWaterMarkIndata.y = addrVirY;
        mWaterMarkIndata.c = addrVirY + width * height;
        mWaterMarkIndata.width = width;
        mWaterMarkIndata.height = height;
        mWaterMarkIndata.resolution_rate = (float)mThumbHeight / (float)mFrameHeight;

        char time_watermark[32];
        getTimeString(time_watermark, 32);

        char buffer_rec[128];
        mCallbackNotifier->getWaterMarkMultiple(buffer_rec);
        CameraLogV("buffer_rec = %s", buffer_rec);
        doWaterMarkMultiple(&mWaterMarkIndata, mWaterMarkCtrlRec, mWaterMarkMultiple, buffer_rec,
                            time_watermark);

        mV4l2CameraMemops.vir = (uint8_t *)addrVirY;
        mV4l2CameraMemops.size = width * height * 3 / 2;
        flushCache(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    }
    pthread_mutex_unlock(&mWaterMarkLock);
}
#endif

#ifdef NPU_DETECT_ENABLE
static char * npu_cls[] = {
    "person",
    "car",
    "bird",
    "cat",
    "dog",
    "pottedplant",
    "diningtable",
    "tvmonitor",
    "refrigerator",
    "clock",
    "chair",
};

int V4L2CameraDevice::osd_disable(void)
{
	int on_off;

	on_off = 0;
	if (-1 == ioctl(mCameraFd, VIDIOC_OVERLAY, &on_off)) {
		printf("VIDIOC_OVERLAY on error!\n");
		return -1;
	}
}

int V4L2CameraDevice::orl_fmt_set(int num, void* res)
{
	struct v4l2_format fmt;
	struct v4l2_clip clips[num*2];
	int i, on_off;
    Awnn_Result_t *rect = (Awnn_Result_t *)res;

	on_off = 0;
	if (-1 == ioctl(mCameraFd, VIDIOC_OVERLAY, &on_off)) {
		CameraLogE("VIDIOC_OVERLAY on error!\n");
		return -1;
	}

	for (i = 0; i < num; i++) {
		clips[i].c.height = rect->boxes[i].ymax - rect->boxes[i].ymin;
		clips[i].c.width = rect->boxes[i].xmax - rect->boxes[i].xmin;
		clips[i].c.left = rect->boxes[i].xmin;
		clips[i].c.top = rect->boxes[i].ymin;

		clips[num + i].c.top = 0xff0000 >> ((i % 3)*8);
	}
	clips[num].c.width = 4;

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.clips = &clips[0];
	fmt.fmt.win.clipcount = num * 2;
	fmt.fmt.win.bitmap = NULL;
	//fmt.fmt.win.chromakey = 0xff5566;
	fmt.fmt.win.field = V4L2_FIELD_NONE;
	fmt.fmt.win.global_alpha = 16;
	//printf("fmt.fmt.win.clipcount:%d\n", fmt.fmt.win.clipcount);
	if (-1 == ioctl(mCameraFd, VIDIOC_S_FMT, &fmt)) {
		CameraLogE("VIDIOC_S_FMT overlay error!\n");
		return -1;
	}

	on_off = 1;
	if (-1 == ioctl(mCameraFd, VIDIOC_OVERLAY, &on_off)) {
		CameraLogE("VIDIOC_OVERLAY on error!\n");
		return -1;
	}
}

int V4L2CameraDevice::npuAwnnDetectWork(void)
{
  if (NULL == mAwnn_context) {
        CameraLogE("mAwnn_context is NULL");
        return -1;
    }

    unsigned char *input_buffers[2] = {mNpuBuffer.addrVirY, mNpuBuffer.addrVirY + NPU_INPUT_WIDTH * NPU_INPUT_HEIGHT};
    awnn_set_input_buffers((Awnn_Context_t *)mAwnn_context, input_buffers);
    // process network
    awnn_run((Awnn_Context_t *)mAwnn_context);
    // get result
    float **results = awnn_get_output_buffers((Awnn_Context_t *)mAwnn_context);
    Awnn_Result_t res;
    // post process
    yolov3_postprocess(results, &res, NPU_INPUT_WIDTH, NPU_INPUT_HEIGHT,
            NPU_INPUT_WIDTH, NPU_INPUT_HEIGHT, 0.35);

    for (int i = 0; i < res.valid_cnt; i++) {
        res.boxes[i].xmin = (res.boxes[i].xmin * mFrameWidth * 100) / NPU_INPUT_WIDTH / 100;
        res.boxes[i].xmax = (res.boxes[i].xmax * mFrameWidth * 100) / NPU_INPUT_WIDTH / 100;
        res.boxes[i].ymin = (res.boxes[i].ymin * mFrameHeight * 100) / NPU_INPUT_HEIGHT /100;
        res.boxes[i].ymax = (res.boxes[i].ymax * mFrameHeight * 100) / NPU_INPUT_HEIGHT /100;
        CameraLogD("%d: cls %s, prob %f, rect [%d, %d, %d, %d]\n", i, npu_cls[res.boxes[i].label], res.boxes[i].score,
            res.boxes[i].xmin, res.boxes[i].ymin, res.boxes[i].xmax, res.boxes[i].ymax);
    }

    if(res.valid_cnt > 0) {
        orl_fmt_set(res.valid_cnt, &res);
    }
    else
        osd_disable();

    return 0;
}

#ifdef NPU_DETECT_FACE_ENBALE
int V4L2CameraDevice::npuDetectFaceWork(V4L2BUF_t *pbuf)
{
    AW_Img input_img;
    input_img.w = NPU_INPUT_WIDTH;
    input_img.h = NPU_INPUT_HEIGHT;
    input_img.c = 3;
    input_img.c_space = RGB;
    input_img.data = pbuf->addrVirY;
    AWF_Det_Container *container = (AWF_Det_Container *)mAwf_Container;
    AWF_Det_Outputs *outputs = (AWF_Det_Outputs *)mAwf_Outputs;
    Awnn_Result_t res;

    awf_run_det(container, &input_img, outputs);

    float scale_w = mFrameWidth * 1.0 / NPU_INPUT_WIDTH;
    float scale_h = mFrameHeight * 1.0 / NPU_INPUT_HEIGHT;
    for (int i = 0; i < outputs->num; i++) {
        AW_Box face_box = outputs->faces[i].bbox;
        AW_LDMK face_ldmk = outputs->faces[i].ldmk;
        face_box.tl_x = (int)(outputs->faces[i].bbox.tl_x * scale_w + 0.5);
        face_box.tl_y = (int)(outputs->faces[i].bbox.tl_y * scale_h + 0.5);
        face_box.br_x = (int)(outputs->faces[i].bbox.br_x * scale_w + 0.5);
        face_box.br_y = (int)(outputs->faces[i].bbox.br_y * scale_h + 0.5);

        face_box.width = face_box.br_x - face_box.tl_x;
        face_box.height = face_box.br_y - face_box.tl_y;
        res.boxes[i].xmin = face_box.tl_x;
        res.boxes[i].xmax = face_box.br_x;
        res.boxes[i].ymin = face_box.tl_y;
        res.boxes[i].ymax = face_box.br_y;
        //printf("xmin/ymin/xmax/ymax/w/h: %d %d %d %d %d %d.\n", face_box.tl_x, face_box.tl_y, face_box.br_x,
            //face_box.br_y, face_box.width, face_box.height);
    }

    if(r outputs->num > 0) {
        orl_fmt_set(outputs->num, &res);
    }
    else
        osd_disable();
}
#endif

int V4L2CameraDevice::npuDetectWork(V4L2BUF_t *pbuf)
{
    unsigned int in_format, out_format;
    int ret;
    const char* nbg = NPU_NG_FILE;
    in_format = DRM_FORMAT_NV21;
    struct RESurface surface;
    float degree = 0.0f;
    void *sync;

    if (!misGLContextInited) {
#ifdef GPU_SCALE_ON_SCREEN
        out_format = DRM_FORMAT_ARGB8888;
        mRender = renderEngineCreate(true, 1,
            in_format, out_format);
#else
        //getDisplayScreenInfo(&out_width, &out_height);
#if NPU_DETECT_FACE_ENBALE
        out_format = DRM_FORMAT_RGB888;
#else
        out_format = DRM_FORMAT_NV21;
#endif
        //create out memory
        mRender = renderEngineCreate(false, 1,
            in_format, out_format);
        renderEngineSetOffScreenTarget((RenderEngine_t)mRender,
            NPU_INPUT_WIDTH,
            NPU_INPUT_HEIGHT,
            mNpuBuffer.dmafd,
            0);
#endif
        npuInitialize();
        misGLContextInited = true;
    }

    struct RE_rect crop = {
        .left = 0,
        .top = 0,
        .right = mFrameWidth,
        .bottom = mFrameHeight,
    };

    struct RE_rect dstWinRect = {
        .left = 0,
        .top = 0,
        .right = NPU_INPUT_WIDTH,
        .bottom = NPU_INPUT_HEIGHT,
    };

    surface.srcFd = pbuf->dmafd;
    surface.srcDataOffset = 0;
    surface.srcWidth = mFrameWidth;
    surface.srcHeight = mFrameHeight;
    memcpy(&surface.srcCrop, &crop, sizeof(crop));
    memcpy(&surface.dstWinRect, &dstWinRect, sizeof(dstWinRect));
    surface.rotateAngle = degree;

    renderEngineSetSurface(mRender, &surface);

    ret = renderEngineDrawOffScreen(mRender, &sync);
    if (ret < 0) {
        CameraLogE("drawLayer failed\n");
        return -1;
    }

    ret = renderEngineWaitSync(mRender, sync);
    if (ret < 0) {
        CameraLogE("waitSync failed\n");
        return -1;
    }

#if 0
        loadFile(mNpuBuffer, "640*384.nv21",
            NPU_INPUT_WIDTH * NPU_INPUT_HEIGHT * 3);
#endif

#ifdef NPU_DETECT_FACE_ENBALE
    ret = npuDetectFaceWork(&mNpuBuffer);
    if (ret < 0) {
        CameraLogE("npuAwnnDetectFaceWork failed\n");
        return -1;
    }
#else
    ret = npuAwnnDetectWork();
    if (ret < 0) {
        CameraLogE("npuAwnnDetectWork failed\n");
        return -1;
    }
#endif

#ifdef NPU_DETECT_FPS_DEBUG
    npu_ts = (int64_t)systemTime() / 1000;
    CameraLogD("npu detect fps:%d \n", 1000000/(npu_ts - npu_last_ts));
    npu_last_ts = npu_ts;
#endif
}
#endif

bool V4L2CameraDevice::captureThread() {
    CameraLogV("");
    pthread_mutex_lock(&mCaptureMutex);
    // stop capture
    if (mCaptureThreadState == CAPTURE_STATE_PAUSED) {
        singalDisconnect();
        // wait for signal of starting to capture a frame
        CameraLogV("capture thread paused");
        pthread_cond_wait(&mCaptureCond, &mCaptureMutex);
    }

    // thread exit
    if (mCaptureThreadState == CAPTURE_STATE_EXIT) {
        singalDisconnect();
        CameraLogV("capture thread exit");
        pthread_mutex_unlock(&mCaptureMutex);
        return false;
    }
    pthread_mutex_unlock(&mCaptureMutex);

    int ret = -1;
    V4L2BUF_t v4l2_buf;

    ret = v4l2WaitCameraReady();

    pthread_mutex_lock(&mCaptureMutex);
    // stop capture or thread exit
    if (mCaptureThreadState == CAPTURE_STATE_PAUSED || mCaptureThreadState == CAPTURE_STATE_EXIT) {
        singalDisconnect();
        CameraLogW("should stop capture now");
        pthread_mutex_unlock(&mCaptureMutex);
        return 0;
    }

    // if (ret != 0) {
    //     CameraLogW("wait v4l2 buffer time out %s", mV4l2Info.device_name);
    //     pthread_mutex_unlock(&mCaptureMutex);
    //     mCallbackNotifier->onCameraDeviceError(1);

    //     CameraLogW("preview queue has %d items.", OSAL_GetElemNum(&mQueueBufferPreview));
    //     return false;
    // }

    // get one video frame
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(v4l2_buffer));
    ret = getPreviewFrame(&buf);
    if (ret != OK) {
        pthread_mutex_unlock(&mCaptureMutex);
        usleep(10000);
        return ret;
    }

    mCurAvailBufferCnt--;


    if (mCurAvailBufferCnt <= NB_BUFFER / 2) {
        CameraLogV("Camera[%d] mCurAvailBufferCnt:%d", mV4l2Info.device_id, mCurAvailBufferCnt);
        int i;
        for (i = 0; i < NB_BUFFER; i++)
            CameraLogV("now buffer stat bufidx=%d,bufrefcnt=%d", i, mV4l2buf[i].refCnt);
        mStatisicsIndex = 0;
    } else if (mNeedHalfFrameRate) {
        mStatisicsIndex++;
        if (mStatisicsIndex >= STATISICS_CNT) {
            mNeedHalfFrameRate = false;
        }
    }

    mCurFrameTimestamp = (int64_t)systemTime();

    if (mLastZoom != mNewZoom) {
        float widthRate = (float)mFrameWidth / (float)mVideoWidth;
        float heightRate = (float)mFrameHeight / (float)mVideoHeight;
        if (mIsThumbUsedForVideo && (widthRate > 4.0) && (heightRate > 4.0)) {
            calculateCrop(&mRectCrop, mNewZoom, mMaxZoom, mFrameWidth / 2, mFrameHeight / 2);
        } else {
            // the main frame crop
            calculateCrop(&mRectCrop, mNewZoom, mMaxZoom, mFrameWidth, mFrameHeight);
        }
        mCameraHardware->setNewCrop(&mRectCrop);

        // the sub-frame crop
        if (mV4l2Info.fast_picture_mode) {
            calculateCrop(&mThumbRectCrop, mNewZoom, mMaxZoom, mThumbWidth, mThumbHeight);
        }

        mLastZoom = mNewZoom;
        if (mCameraType == CAMERA_TYPE_TVIN_NTSC) {
            mThumbRectCrop.left = 0;
            mThumbRectCrop.top = 0;
            mThumbRectCrop.right = 719;
            mThumbRectCrop.bottom = 479;
        }
        if (mCameraType == CAMERA_TYPE_TVIN_PAL) {
            mThumbRectCrop.left = 0;
            mThumbRectCrop.top = 0;
            mThumbRectCrop.right = 719;
            mThumbRectCrop.bottom = 575;
        }
        CameraLogV("CROP: [%d, %d, %d, %d]", mRectCrop.left, mRectCrop.top, mRectCrop.right,
                   mRectCrop.bottom);
        CameraLogV("thumb CROP: [%d, %d, %d, %d]", mThumbRectCrop.left, mThumbRectCrop.top,
                   mThumbRectCrop.right, mThumbRectCrop.bottom);
    }
    // CameraLogD("mVideoFormat=0x%x ,mCaptureFormat=0x%x\n",mVideoFormat, mCaptureFormat);

#ifdef CDX_FOR_PURE_H264
    if ((mCaptureFormat == V4L2_PIX_FMT_MJPEG) || (mCaptureFormat == V4L2_PIX_FMT_H264)) {
        if (mDecoder == NULL) {
            CameraLogE("mDecoder is NULL, init again");
            Libve_init2(&mDecoder, &mVideoInfo, &mVideoConf);
        }
        mDataInfo.nLength = buf.bytesused;
        mDataInfo.nPts = (int64_t)systemTime() / 1000;

        mV4l2CameraMemops.vir = mVideoBuffer.buf_vir_addr[buf.index];
        mV4l2CameraMemops.size = mFrameWidth * mFrameHeight * 3 / 2;
        flushCache(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);

        Libve_dec2(&mDecoder, (void *)mMapMem.mem[buf.index],
                   (void *)mVideoBuffer.buf_vir_addr[buf.index], &mVideoInfo, &mDataInfo,
                   &mVideoConf, &mV4l2CameraMemops);

        mV4l2CameraMemops.vir = mVideoBuffer.buf_vir_addr[buf.index];
        mV4l2CameraMemops.size = mFrameWidth * mFrameHeight * 3 / 2;
        flushCache(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
    }
#endif

    // if (mVideoFormat != V4L2_PIX_FMT_YUYV && mCaptureFormat == V4L2_PIX_FMT_YUYV) {
    if (mVideoFormat == V4L2_PIX_FMT_YUYV && mCaptureFormat == V4L2_PIX_FMT_YUYV) {
#ifdef USE_MP_CONVERT
        YUYVToYUV420C((void *)buf.m.offset, (void *)(mVideoBuffer.buf_phy_addr[buf.index]),
                      mFrameWidth, mFrameHeight);
#else
        YUYVToNV21(static_cast<void *>(mMapMem.mem[buf.index]),
                   mVideoBuffer.buf_vir_addr[buf.index], mFrameWidth, mFrameHeight);
#endif
    }

    if (mCameraType == CAMERA_TYPE_CSI) {
        v4l2_buf.addrVirY = (uint8_t *)mMapMem.mem[buf.index];
        v4l2_buf.addrVirC = (uint8_t *)mMapMem.mem[buf.index] + ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight);
        v4l2_buf.dmafd = (unsigned int)mMapMem.dmafd[buf.index];
        v4l2_buf.width = mFrameWidth;
        v4l2_buf.height = mFrameHeight;
        v4l2_buf.addrPhyY = mMapMem.phy[buf.index];
        v4l2_buf.addrPhyC = mMapMem.phy[buf.index] + ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight);
    } else {
        v4l2_buf.addrVirY = mVideoBuffer.buf_vir_addr[buf.index];
        v4l2_buf.addrVirC =
            mVideoBuffer.buf_vir_addr[buf.index] + ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight);
        v4l2_buf.addrPhyY = mVideoBuffer.buf_phy_addr[buf.index];
        v4l2_buf.width = mFrameWidth;
        v4l2_buf.height = mFrameHeight;
    }

    v4l2_buf.index = buf.index;
    v4l2_buf.timeStamp = mCurFrameTimestamp;
    v4l2_buf.crop_rect.left = mRectCrop.left;
    v4l2_buf.crop_rect.top = mRectCrop.top;
    v4l2_buf.crop_rect.width = mRectCrop.right - mRectCrop.left + 1;
    v4l2_buf.crop_rect.height = mRectCrop.bottom - mRectCrop.top + 1;
    v4l2_buf.format = mCaptureFormat;
    v4l2_buf.bytesused = buf.bytesused;
    v4l2_buf.isThumbAvailable = 0;
    v4l2_buf.refCnt = 0;

#if USE_G2D_COPY
    g2dClipByFd(mV4l2G2DHandle, v4l2_buf.dmafd, G2D_ROT_0, G2D_FORMAT_YUV420UVC_V1U1V0U0,
                v4l2_buf.width, v4l2_buf.height, 0, 0, v4l2_buf.width, v4l2_buf.height,
                mG2DBuffer.dma_fd[buf.index], G2D_FORMAT_YUV420UVC_V1U1V0U0, v4l2_buf.width,
                v4l2_buf.height, 0, 0, v4l2_buf.width, v4l2_buf.height);
    v4l2_buf.dmafd = (unsigned int)mG2DBuffer.dma_fd[buf.index];
    v4l2_buf.addrPhyY = mG2DBuffer.buf_phy_addr[buf.index];
    v4l2_buf.addrVirY = (unsigned long)mG2DBuffer.buf_vir_addr[buf.index];
#endif

#if V4L2_SAVEFRAME
    {
        static int cntt = 0;
        cntt++;
        char buf[256];
        // int buf_len = v4l2_buf.width * v4l2_buf.height * 3 / 2;
        int buf_len =
            v4l2_buf.bytesused != 0 ? v4l2_buf.bytesused : v4l2_buf.width * v4l2_buf.height * 3 / 2;
        if (cntt < V4L2_SAVEFRAME_COUNT) {
            CameraLogD("save yuv date, p:0x%x, len:%d", v4l2_buf.addrVirY, buf_len);
            sprintf(buf, "%s/v4l2_hal_aft_%dx%d_%d.bin", V4L2_SAVEFRAME_PATH, v4l2_buf.width,
                    v4l2_buf.height, cntt);
            saveframe(buf, (void *)v4l2_buf.addrVirY, buf_len, 0);
        }
    }
#endif

#ifdef WATERMARK
    if (mWaterMarkDispMode == WATER_MARK_DISP_MODE_VIDEO_AND_PREVIEW) {
        addWaterMark((unsigned char *)v4l2_buf.addrVirY, v4l2_buf.width, v4l2_buf.height);
    }
#endif

    memcpy(&mV4l2buf[v4l2_buf.index], &v4l2_buf, sizeof(V4L2BUF_t));
    memcpy(&mV4l2buf[v4l2_buf.index].buf, &buf, sizeof(v4l2_buffer));

#ifdef NPU_DETECT_ENABLE
    if (0 == mCameraId) {
        ret = OSAL_Queue(&mQueueBufferNpuDetect, &mV4l2buf[v4l2_buf.index]);
        if (ret != 0) {
            CameraLogW("npu queue full");
            // usleep(50000);
            goto DEC_REF;
        }
    }

    pthread_cond_signal(&mNpuWorkCond);
#endif

#ifdef CAMERA_MANAGER_ENABLE
    if (mCameraManager != NULL) {
        // add reference count
        if (mCameraManager->isOviewEnable()) {
            ret = mCameraManager->queueCameraBuf(mV4l2Info.device_id, &v4l2_buf);
            if(ret < 0)
            {
                // faild to queue compose release it!
                mV4l2buf[v4l2_buf.index].refCnt = 1;
            }
        }
        goto DEC_REF;
    }
#endif

    {
#if !USE_G2D_COPY
        // add reference count
        mV4l2buf[v4l2_buf.index].refCnt++;
#endif
        CameraLogV("before OSAL_Queue addrVirY = 0x%x v4l2_buf addrVirY=0x%x",
                   mV4l2buf[v4l2_buf.index].addrVirY, v4l2_buf.addrVirY);
        ret = OSAL_Queue(&mQueueBufferPreview, &mV4l2buf[v4l2_buf.index]);
        if (ret != 0) {
            CameraLogW("preview queue full");
            // usleep(50000);
            goto DEC_REF;
        }
        // CameraLogD("capture thread 2028 ID %d refCnt %d",v4l2_buf.index,
        // mV4l2buf[v4l2_buf.index].refCnt);

        //  signal a new frame for preview
        pthread_cond_signal(&mPreviewCond);

        CameraLogV("mTakePictureState is %d", mTakePictureState);
        if (mTakePictureState == TAKE_PICTURE_FAST || mTakePictureState == TAKE_PICTURE_NORMAL ||
            mTakePictureState == TAKE_PICTURE_RECORD) {
            //#if !USE_G2D_COPY
            //            // add reference count
            //            mV4l2buf[v4l2_buf.index].refCnt++;
            //#endif
            // enqueue picture buffer
            CameraLogV("before OSAL_Queue");
            ret = OSAL_Queue(&mQueueBufferPicture, &mV4l2buf[v4l2_buf.index]);
            if (ret != 0) {
                CameraLogW("picture queue full");
                pthread_cond_signal(&mPictureCond);
                goto DEC_REF;
            }

            CameraLogD("capture thread index:%d refCnt:%d", v4l2_buf.index,
                       mV4l2buf[v4l2_buf.index].refCnt);
            mTakePictureState = TAKE_PICTURE_NULL;
            mIsPicCopy = false;
            pthread_cond_signal(&mPictureCond);
        }
    }

DEC_REF:
    pthread_mutex_unlock(&mCaptureMutex);

    // CameraLogD("capture thread 2119 cid =%d ID %d refCnt %d",mV4l2Info.device_id,v4l2_buf.index,
    // mV4l2buf[v4l2_buf.index].refCnt);
    releasePreviewFrame(v4l2_buf.index);

#if ENABLE_DEBUG_CAP_TIMEOUT
    long long cap_time = (int64_t)systemTime() / 1000000;
    if (cap_time - cap_base_time > CAP_TIMEOUT)
        CameraLogD("[test] ---- capture takes %lld ms, cur time:%lld, pre time:%lld",
                   cap_time - cap_base_time, cap_time, cap_base_time);
    cap_base_time = cap_time;
#endif
    return true;
}

bool V4L2CameraDevice::previewThread() {
    V4L2BUF_t *pbuf = (V4L2BUF_t *)OSAL_Dequeue(&mQueueBufferPreview);
    if (pbuf == NULL) {
        CameraLogV("picture queue no buffer, sleep...");
        pthread_mutex_lock(&mPreviewMutex);
        pthread_cond_wait(&mPreviewCond, &mPreviewMutex);
        pthread_mutex_unlock(&mPreviewMutex);
        return true;
    } else {
        CameraLogV("pbuf addrVirY = 0x%x", pbuf->addrVirY);
    }

    if (mMapMem.mem[pbuf->index] == NULL ||
        ((pbuf->addrPhyY == 0) && ((mCaptureFormat != V4L2_PIX_FMT_NV21)))) {
        CameraLogV("preview buffer have been released...");
        return true;
    }

#ifdef ADAS_ENABLE
    if (mAdasStarted && ((adasIgnorFrameCnt % 2) == 0)) {
        g2dScale(mV4l2G2DHandle, (unsigned char *)pbuf->addrPhyY, pbuf->width, pbuf->height,
                 (unsigned char *)mAdasBuffer.addrPhyY, ADAS_WIDTH, ADAS_HEIGHT);
        mCurV4l2Buf = (unsigned char *)mAdasBuffer.addrVirY;
        pthread_mutex_lock(&mAdasMutex);
        pthread_cond_signal(&mAdasCond);
        pthread_mutex_unlock(&mAdasMutex);
    }
    adasIgnorFrameCnt++;
#endif

#ifdef WATERMARK
    if (mWaterMarkDispMode == WATER_MARK_DISP_MODE_VIDEO_ONLY) {
        addWaterMark((unsigned char *)pbuf->addrVirY, pbuf->width, pbuf->height);
    }
#endif

#if !USE_G2D_COPY
    pthread_mutex_lock(&mCaptureMutex);
#endif
    CameraLogV("Camera[%d] before onNextFrameAvailable %d %d",mCameraId,
               mCallbackNotifier->isMessageEnabled(CAMERA_MSG_VIDEO_FRAME),
               mCallbackNotifier->isVideoRecordingEnabled());
    if (mCallbackNotifier->isMessageEnabled(CAMERA_MSG_VIDEO_FRAME) &&
        mCallbackNotifier->isVideoRecordingEnabled()) {
        // pbuf->refCnt++;
        mCallbackNotifier->onNextFrameAvailable((void *)pbuf, mUseHwEncoder, -1);
    }

    if (mCallbackNotifier->isMessageEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        if (!mNeedHalfFrameRate || mShouldPreview) {
            dma_mem_des_t  memops;
            memops.vir = pbuf->addrVirY;
            memops.size = mV4l2CameraMemops.size;
            flushCache(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
            mPreviewWindow->onNextFrameAvailable((void *)pbuf, mCameraFlipStatus, 0);
        } else {
            mShouldPreview = mShouldPreview ? false : true;
        }
    }
#if !USE_G2D_COPY
    pthread_mutex_unlock(&mCaptureMutex);

    releasePreviewFrame(pbuf->index);
#endif
    return true;
}

// singal picture
bool V4L2CameraDevice::pictureThread() {
    V4L2BUF_t *pbuf = (V4L2BUF_t *)OSAL_Dequeue(&mQueueBufferPicture);
    if (pbuf == NULL) {
        CameraLogV("picture queue no buffer, sleep...");
        pthread_mutex_lock(&mPictureMutex);
        pthread_cond_wait(&mPictureCond, &mPictureMutex);
        pthread_mutex_unlock(&mPictureMutex);
        return true;
    }

    // notify picture cb
    // mCameraHardware->notifyPictureMsg((void *) pbuf);

    mCallbackNotifier->takePicture((void *)pbuf);

#if !USE_G2D_COPY
    if (!mIsPicCopy) {
        releasePreviewFrame(pbuf->index);
    }
#endif

    return true;
}

#ifdef NPU_DETECT_ENABLE
bool V4L2CameraDevice::npuThread() {
    V4L2BUF_t *pbuf = (V4L2BUF_t *)OSAL_Dequeue(&mQueueBufferNpuDetect);
    if (pbuf == NULL) {
        CameraLogV("npu queue no buffer, sleep...");
        pthread_mutex_lock(&mNpuWorkMutex);
        pthread_cond_wait(&mNpuWorkCond, &mNpuWorkMutex);
        pthread_mutex_unlock(&mNpuWorkMutex);
        return true;
    }

    npuDetectWork(pbuf);

    return true;
}
#endif


void V4L2CameraDevice::setTakePictureState(TAKE_PICTURE_STATE state) {
    pthread_mutex_lock(&mCaptureMutex);
    CameraLogV("setTakePictureState:%d", state);
    mTakePictureState = state;
    pthread_mutex_unlock(&mCaptureMutex);
}

// -----------------------------------------------------------------------------
// extended interfaces here <***** star *****>
// -----------------------------------------------------------------------------
int V4L2CameraDevice::openCameraDev(HALCameraInfo *halInfo) {
    CAMERA_LOG;

    int ret = -1;
    struct v4l2_input inp;
    struct v4l2_capability cap;
    // bool foundDev = false;

    if (halInfo == NULL) {
        CameraLogE("error HAL camera info");
        return -1;
    }
    if ((access(halInfo->device_name, F_OK)) != 0) {
        CameraLogE("do not find halInfo->device_name:%s", halInfo->device_name);
    }

    mCameraFd = open(halInfo->device_name, O_RDWR | O_NONBLOCK, 0);
    if (mCameraFd == -1) {
        CameraLogE("ERROR opening %s: %s", halInfo->device_name, strerror(errno));
        return -1;
    }
    //-------------------------------------------------
    // check v4l2 device capabilities
    ret = ioctl(mCameraFd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        CameraLogE("Error opening device: unable to query device.");
        goto END_ERROR;
    }
    if (!strcmp((char *)cap.driver, "uvcvideo")) {
        mCameraType = CAMERA_TYPE_UVC;
        CameraLogD("mCameraType = CAMERA_TYPE_UVC");
        mV4l2_memory = V4L2_MEMORY_MMAP; /*V4L2_MEMORY_MMAP */
    } else if (!strcmp((char *)cap.driver, "sunxi-tvd")) {
        int tvinType;
        tvinType = getTVINSystemType(mCameraFd, halInfo->cvbs_type);
        if (tvinType == TVD_PAL) {
            mCameraType = CAMERA_TYPE_TVIN_PAL;
            mFrameWidth = 720;
            mFrameHeight = 576;
            if (halInfo->device_id == 8) {
                mFrameWidth = 1440;
                mFrameHeight = 1152;
            }
            config_set_width(halInfo->device_id, mFrameWidth);
            config_set_heigth(halInfo->device_id, mFrameHeight);
            CameraLogD("mCameraType = CAMERA_TYPE_TVIN_PAL");
        } else if (tvinType == TVD_NTSC) {
            mCameraType = CAMERA_TYPE_TVIN_NTSC;
            mFrameWidth = 720;
            mFrameHeight = 480;

            if (halInfo->device_id == 8) {
                mFrameWidth = 1440;
                mFrameHeight = 960;
            }
            if (halInfo->device_id == 360) {
                mFrameWidth = 1440;
                mFrameHeight = 960;
            }

            config_set_width(halInfo->device_id, mFrameWidth);
            config_set_heigth(halInfo->device_id, mFrameHeight);
            CameraLogD("mCameraType = CAMERA_TYPE_TVIN_NTSC");
        } else if (tvinType == TVD_YPBPR) {
            mCameraType = CAMERA_TYPE_TVIN_YPBPR;
            CameraLogD("mCameraType = CAMERA_TYPE_TVIN_YPBPR");
        } else {
#ifdef DEFAULT_CVBS_CAMERA_TYPE_PAL
            mCameraType = CAMERA_TYPE_TVIN_PAL;
            CameraLogD("Default mCameraType = CAMERA_TYPE_TVIN_PAL");
#else
            mCameraType = CAMERA_TYPE_TVIN_NTSC;
            CameraLogD("Default mCameraType = CAMERA_TYPE_TVIN_NTSC");
#endif
        }
        mV4l2_memory = V4L2_MEMORY_DMABUF;
    } else {
        mCameraType = CAMERA_TYPE_CSI;
        CameraLogD("mCameraType = CAMERA_TYPE_CSI");
        mV4l2_memory = V4L2_MEMORY_DMABUF;
    }
    CameraLogD("The name of the Camera is '%s'", cap.card);

    if (mCameraType == CAMERA_TYPE_CSI) {
        inp.index = 0;
        if (-1 == ioctl(mCameraFd, VIDIOC_S_INPUT, &inp)) {
            CameraLogE("VIDIOC_S_INPUT error id=%d!", halInfo->device_id);
            goto END_ERROR;
        }

        if (halInfo->device_id == 360) {
            mFrameWidth = 2560;
            mFrameHeight = 1440;
        }

        halInfo->facing = CAMERA_FACING_FRONT;
        mIsThumbUsedForVideo = true;
    }
    if (mCameraType == CAMERA_TYPE_UVC) {
        if (tryFmt(V4L2_PIX_FMT_YUYV) == OK) {
            mCaptureFormat = V4L2_PIX_FMT_YUYV;  // maybe usb camera
            mVideoFormat = mCaptureFormat;
            CameraLogV("capture format: V4L2_PIX_FMT_YUYV");
        } else if (tryFmt(V4L2_PIX_FMT_MJPEG) == OK) {
            mCaptureFormat = V4L2_PIX_FMT_MJPEG;  // maybe usb camera
            mVideoFormat = mCaptureFormat;
            CameraLogE("capture format: V4L2_PIX_FMT_MJPEG");
        } else if (tryFmt(V4L2_PIX_FMT_H264) == OK) {
            mCaptureFormat = V4L2_PIX_FMT_H264;
            mVideoFormat = mCaptureFormat;
            CameraLogV("capture format: V4L2_PIX_FMT_H264");
        } else {
            CameraLogE("driver is not support YUYV/MJPEG/H264 format");
            goto END_ERROR;
        }
    } else {
        mCaptureFormat = V4L2_PIX_FMT_NV21;
        mVideoFormat = mCaptureFormat;
    }
    return OK;

END_ERROR:
    if (mCameraFd != 0) {
        close(mCameraFd);
        mCameraFd = 0;
    }
    return -1;
}

void V4L2CameraDevice::closeCameraDev() {
    CAMERA_LOG;

    if (mCameraFd != 0) {
        close(mCameraFd);
        mCameraFd = 0;
    }
}

int V4L2CameraDevice::v4l2SetVideoParams(int width, int height, uint32_t pix_fmt) {
    int ret = UNKNOWN_ERROR;
    struct v4l2_format format;

    CameraLogV("%s, line: %d, w: %d, h: %d, pfmt: %d", __FUNCTION__, __LINE__, width, height,
               pix_fmt);

    memset(&format, 0, sizeof(format));
    if (mCameraType == CAMERA_TYPE_CSI) {
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        format.fmt.pix_mp.width = width;
        format.fmt.pix_mp.height = height;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;  // V4L2_FIELD_INTERLACED;
        format.fmt.pix_mp.pixelformat = pix_fmt;
    } else {
        int i = 0;
        if (mCameraType == CAMERA_TYPE_TVIN_PAL || mCameraType == CAMERA_TYPE_TVIN_NTSC) {
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            while (ioctl(mCameraFd, VIDIOC_G_FMT, &format) && (i++ < 20)) {
                CameraLogD("+++get tvin signal failed.");
                return -1;
            }
        }
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = width;
        format.fmt.pix.height = height;
        if (mCaptureFormat == V4L2_PIX_FMT_YUYV) {
            format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        } else if (mCaptureFormat == V4L2_PIX_FMT_MJPEG) {
            format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        } else if (mCaptureFormat == V4L2_PIX_FMT_H264) {
            format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
        } else {
            CameraLogE("pix_fmt is set to default V4L2_PIX_FMT_YUYV");
            format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
            // format.fmt.pix.pixelformat = pix_fmt;
        }
        format.fmt.pix.field = V4L2_FIELD_NONE;
    }

    ret = ioctl(mCameraFd, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        CameraLogE("VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }
    ret = ioctl(mCameraFd, VIDIOC_G_FMT, &format);
    if (ret < 0) {
        CameraLogE("VIDIOC_G_FMT Failed: %s", strerror(errno));
        return ret;
    }
    if ((mCameraType == CAMERA_TYPE_CSI)) {
        nPlanes = format.fmt.pix_mp.num_planes;
        CameraLogD("VIDIOC_G_FMT resolution = %d*%d num_planes = %d", format.fmt.pix_mp.width,
                   format.fmt.pix_mp.height, format.fmt.pix_mp.num_planes);
        mFrameWidth = format.fmt.pix_mp.width;
        mFrameHeight = format.fmt.pix_mp.height;
    } else {
        CameraLogD("VIDIOC_G_FMT resolution = %d*%d", format.fmt.pix.width, format.fmt.pix.height);
        mFrameWidth = format.fmt.pix.width;
        mFrameHeight = format.fmt.pix.height;
    }
    return OK;
}

int V4L2CameraDevice::v4l2ReqBufs(int *buf_cnt) {
    CAMERA_LOG;
    int ret = UNKNOWN_ERROR;
    struct v4l2_requestbuffers rb;

    CameraLogV("TO VIDIOC_REQBUFS count: %d", *buf_cnt);

    memset(&rb, 0, sizeof(rb));
    if (mCameraType == CAMERA_TYPE_CSI) {
        rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    rb.memory = mV4l2_memory;
    rb.count = *buf_cnt;

    ret = ioctl(mCameraFd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        CameraLogE("VIDIOC_REQBUFS failed:%s", strerror(errno));
        return ret;
    }

    *buf_cnt = rb.count;
    CameraLogV("VIDIOC_REQBUFS count:%d", *buf_cnt);
    return OK;
}

int buffer_export(int v4lfd, enum v4l2_buf_type bt, int index, int *dmafd) {
    struct v4l2_exportbuffer expbuf;

    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = bt;
    expbuf.index = index;
    expbuf.plane = 0;

    if (ioctl(v4lfd, VIDIOC_EXPBUF, &expbuf) == -1) {
        perror("VIDIOC_EXPBUF");
        *dmafd = -1;
        return -1;
    }

    *dmafd = expbuf.fd;

    return 0;
}

int V4L2CameraDevice::v4l2QueryBuf() {
    CameraLogE("bill test LOG_NDEBUG = %d", LOG_NDEBUG);
    CAMERA_LOG;
    int ret = UNKNOWN_ERROR;
    struct v4l2_buffer buf;
    for (int i = 0; i < mBufferCnt; i++) {
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        if (mCameraType == CAMERA_TYPE_CSI) {
            struct v4l2_plane planes[VIDEO_MAX_PLANES];
            memset(planes, 0, VIDEO_MAX_PLANES * sizeof(struct v4l2_plane));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.length = nPlanes;
            buf.m.planes = planes;
            if (NULL == buf.m.planes) {
                CameraLogE("buf.m.planes malloc failed!");
            }
        } else {
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
        buf.memory = mV4l2_memory;
        buf.index = i;

        ret = ioctl(mCameraFd, VIDIOC_QUERYBUF, &buf);
        if (ret < 0) {
            CameraLogE("Unable to query buffer (%s)", strerror(errno));
            return ret;
        }

        if (mCameraType == CAMERA_TYPE_CSI) {
            switch (buf.memory) {
                case V4L2_MEMORY_MMAP:
                    for (unsigned int j = 0; j < nPlanes; j++) {
                        mMapMem.mem[i] = mmap(0, buf.m.planes[j].length, PROT_READ | PROT_WRITE,
                                              MAP_SHARED, mCameraFd, buf.m.planes[j].m.mem_offset);

                        mMapMem.length = buf.m.planes[j].length;

                        if (mMapMem.mem[i] == MAP_FAILED) {
                            CameraLogE("Unable to map buffer (%s)", strerror(errno));
                            for (int j = 0; j < i; j++) {
                                munmap(mMapMem.mem[j], mMapMem.length);
                            }
                            return -1;
                        }
                        // buffer_export(mCameraFd,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,i,&mMapMem.dmafd[i]);
                        CameraLogV("MMAP: index:%d, mem:0x%x, len:%d, dmafd:%d", i,
                                   (unsigned long)mMapMem.mem[i], buf.length, mMapMem.dmafd[i]);
                    }
                    break;
                case V4L2_MEMORY_USERPTR:
                    CameraLogW("V4L2_MEMORY_USERPTR has been deprecated since 2022. Nov 30");
                    break;
                case V4L2_MEMORY_DMABUF:
                    for (unsigned int j = 0; j < nPlanes; j++) {
                        mV4l2CameraMemops.size = buf.m.planes[j].length;
                        ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
                        if (ret < 0) {
                            CameraLogE("allocAlloc for mMapMem.mem[%d] failed", i);
                            return -1;
                        }
                        mMapMem.mem[i] = (void *)mV4l2CameraMemops.vir;
                        allocVir2phy(MEM_TYPE_CDX_NEW,&mV4l2CameraMemops,NULL);
                        mMapMem.phy[i] =  mV4l2CameraMemops.phy;
                        mMapMem.dmafd[i] = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;
                        mMapMem.length = buf.m.planes[j].length;
                        // buf.m.planes[j].m.userptr = (unsigned long)mV4l2CameraMemops.vir;
                        buf.m.planes[j].m.fd = mMapMem.dmafd[i];
                        buf.m.planes[j].length = mV4l2CameraMemops.size;

                        CameraLogD("DMABUF: index:%d, mem:%p,phy:%p len:%d, dmafd:%d", i,
                                   mMapMem.mem[i],mMapMem.phy[i],buf.m.planes[j].length,
                                   mMapMem.dmafd[i]);
                    }
                    break;
            }
        } else {
            switch (buf.memory) {
                case V4L2_MEMORY_MMAP:
                    mMapMem.length = buf.length;
                    mMapMem.mem[i] = mmap(0, mMapMem.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                          mCameraFd, buf.m.offset);
                    if (mMapMem.mem[i] == MAP_FAILED) {
                        CameraLogE("Unable to map buffer (%s)", strerror(errno));
                        munmap(mMapMem.mem[i], mMapMem.length);
                        return -1;
                    }
                    CameraLogV("MMAP: index:%d, mem:0x%x, %p len:%d", i,
                               (unsigned long)mMapMem.mem[i], mMapMem.mem[i], buf.length);
                    break;
                case V4L2_MEMORY_USERPTR:
                    CameraLogW("V4L2_MEMORY_USERPTR has been deprecated since 2022. Nov 30");
                    break;
                case V4L2_MEMORY_DMABUF:
                    mV4l2CameraMemops.size = buf.length;
                    ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
                    if (ret < 0) {
                        CameraLogE("allocAlloc for mMapMem.mem[%d] failed", i);
                        return -1;
                    }
                    mMapMem.length = buf.length;
                    mMapMem.mem[i] = (void *)mV4l2CameraMemops.vir;
                    mMapMem.dmafd[i] = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;
                    // buf.m.userptr = (unsigned long) mV4l2CameraMemops.vir;
                    buf.m.fd = mMapMem.dmafd[i];
                    buf.length = mV4l2CameraMemops.size;

                    CameraLogE("DMABUF: index:%d, mem:0x%x, len:%d, dmafd:%d", i,
                               (unsigned long)mMapMem.mem[i], buf.length, mMapMem.dmafd[i]);
                    break;
            }
        }

        // start with all buffers in queue
        ret = ioctl(mCameraFd, VIDIOC_QBUF, &buf);
        if (ret < 0) {
            CameraLogE("VIDIOC_QBUF Failed %s", strerror(errno));
            return ret;
        }

        // alloc buffer for mVideoBuffer
        // if ((mCaptureFormat == V4L2_PIX_FMT_MJPEG)
        //     || (mCaptureFormat == V4L2_PIX_FMT_YUYV)
        //     || (mCaptureFormat == V4L2_PIX_FMT_H264)) {
        if (mCameraType == CAMERA_TYPE_UVC) {
            int buffer_len = ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight) * 2;

            mV4l2CameraMemops.size = buffer_len;
            ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
            if (ret < 0) {
                CameraLogE("mVideoBuffer:allocAlloc failed");
            }
            mVideoBuffer.buf_vir_addr[i] = mV4l2CameraMemops.vir;
            mVideoBuffer.buf_phy_addr[i] = mV4l2CameraMemops.phy;
            mVideoBuffer.dma_fd[i] = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;

            CameraLogV("video buffer: index:%d, fd:%d, vir:0x%x, phy:0x%x, len:%d", i,
                       mVideoBuffer.dma_fd[i], mVideoBuffer.buf_vir_addr[i],
                       mVideoBuffer.buf_phy_addr[i], buffer_len);
        }

        //    //memset((void *) mVideoBuffer.buf_vir_addr[i], 0x10, mFrameWidth * mFrameHeight);
        //    //memset((void *) mVideoBuffer.buf_vir_addr[i] + mFrameWidth * mFrameHeight,
        //    //       0x80, mFrameWidth * mFrameHeight / 2);
        //}
#if USE_G2D_COPY
        mV4l2CameraMemops.size = ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight) * 3 / 2;
        ret = allocAlloc(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        if (ret < 0) {
            CameraLogE("mG2DBuffer: allocAlloc failed");
        }
        mG2DBuffer.buf_vir_addr[i] = mV4l2CameraMemops.vir;
        mG2DBuffer.buf_phy_addr[i] = mV4l2CameraMemops.phy;
        mG2DBuffer.dma_fd[i] = mV4l2CameraMemops.ion_buffer.fd_data.aw_fd;

        CameraLogV("g2d buffer: index:%d, fd:%d, vir:0x%x, phy:0x%x, len:%d", i,
                   mG2DBuffer.dma_fd[i], mG2DBuffer.buf_vir_addr[i], mG2DBuffer.buf_phy_addr[i],
                   mV4l2CameraMemops.size);

        memset((void *)mG2DBuffer.buf_vir_addr[i], 0x10, mFrameWidth * mFrameHeight);
        memset((void *)mG2DBuffer.buf_vir_addr[i] + mFrameWidth * mFrameHeight, 0x80,
               mFrameWidth * mFrameHeight / 2);
#endif
    }
    return OK;
}

int V4L2CameraDevice::v4l2StartStreaming() {
    CAMERA_LOG;
    int ret = UNKNOWN_ERROR;
    enum v4l2_buf_type type;
    if (mCameraType == CAMERA_TYPE_CSI) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    ret = ioctl(mCameraFd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        CameraLogE("StartStreaming: Unable to start capture: %s", strerror(errno));
        return ret;
    }
    return OK;
}

int V4L2CameraDevice::v4l2StopStreaming() {
    CAMERA_LOG;
    int ret = UNKNOWN_ERROR;
    enum v4l2_buf_type type;
    if (mCameraType == CAMERA_TYPE_CSI) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    ret = ioctl(mCameraFd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        CameraLogE("StopStreaming: Unable to stop capture: %s", strerror(errno));
        return ret;
    }
    CameraLogV("V4L2Camera::v4l2StopStreaming OK");

    return OK;
}

int V4L2CameraDevice::v4l2UnmapBuf() {
    CAMERA_LOG;
    int ret = UNKNOWN_ERROR;

    for (int i = 0; i < mBufferCnt; i++) {
        if (mV4l2_memory == V4L2_MEMORY_MMAP) {
            ret = munmap(mMapMem.mem[i], mMapMem.length);
            if (ret < 0) {
                CameraLogE("v4l2CloseBuf Unmap failed");
                return ret;
            }

            mMapMem.mem[i] = NULL;
        } else {
            if (mMapMem.mem[i] != 0) {
                mV4l2CameraMemops.vir = (uint8_t *)mMapMem.mem[i];
                allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
                mMapMem.mem[i] = 0;
                mMapMem.dmafd[i] = 0;
            }
        }

        if (mVideoBuffer.buf_vir_addr[i] != 0) {
            mV4l2CameraMemops.vir = mVideoBuffer.buf_vir_addr[i];
            allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        }
#if USE_G2D_COPY
        if (mG2DBuffer.buf_vir_addr[i] != 0) {
            mV4l2CameraMemops.vir = mG2DBuffer.buf_vir_addr[i];
            allocFree(MEM_TYPE_CDX_NEW, &mV4l2CameraMemops, NULL);
        }
#endif
        if (mMapMem.dmafd[i] > 0) {
            close(mMapMem.dmafd[i]);
            mMapMem.dmafd[i] = -1;
        }
    }
    mVideoBuffer.buf_unused = NB_BUFFER;
    mVideoBuffer.read_id = 0;

    return OK;
}


void V4L2CameraDevice::releasePreviewFrame(int index,bool force) {
    CameraLogV("camera[%d] index = %d  mV4l2buf[%d].refCnt %d",mCameraId,index,index, mV4l2buf[index].refCnt);
    int ret = UNKNOWN_ERROR;
    // struct v4l2_buffer buf;
    if (index >= NB_BUFFER || index < 0) {
        CameraLogE("error releasePreviewFrame: index = %d", index);
        return;
    }
    if(force)
    {
        mV4l2buf[index].refCnt = 1;
    }
    pthread_mutex_lock(&mCaptureMutex);
    if (mV4l2buf[index].refCnt > 0 && --mV4l2buf[index].refCnt == 0) {
    // if (--mV4l2buf[index].refCnt <= 0) {
        // memset(&buf, 0, sizeof(v4l2_buffer));
        // if (mCameraType == CAMERA_TYPE_CSI) {
        //     struct v4l2_plane planes[VIDEO_MAX_PLANES];
        //     buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        //     buf.length = nPlanes;
        //     buf.m.planes = planes;
        // } else {
        //     buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // }
        // buf.memory = mV4l2_memory;
        // buf.index = index;
        // if (buf.memory == V4L2_MEMORY_USERPTR) {
        //     for (unsigned int j = 0; j < nPlanes; j++) {
        //         buf.m.planes[j].m.userptr = (unsigned long)mMapMem.mem[index];
        //         buf.m.planes[j].length = mMapMem.length;
        //     }
        // } else if (buf.memory == V4L2_MEMORY_DMABUF) {
        //     for (unsigned int j = 0; j < nPlanes; j++) {
        //         // buf.m.planes[j].m.userptr = (unsigned long)mMapMem.mem[index];
        //         buf.m.planes[j].m.fd = mMapMem.dmafd[index];
        //         buf.m.planes[j].length = mMapMem.length;
        //     }
        // }
        struct v4l2_buffer buf;
        if (mCameraType == CAMERA_TYPE_CSI) {
            struct v4l2_plane planes[VIDEO_MAX_PLANES];
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.length = nPlanes;
            buf.m.planes = planes;
            buf.memory = mV4l2_memory;
            buf.index = index;
            for (unsigned int j = 0; j < nPlanes; j++) {
                // buf.m.planes[j].m.userptr = mV4l2buf[index].buf.addrVirY;
                buf.m.planes[j].m.fd = mV4l2buf[index].dmafd;
                // buf.m.planes[j].length = ALIGN_32B(mFrameWidth) * ALIGN_32B(mFrameHeight) * 3 / 2;
                buf.m.planes[j].length = mV4l2CameraMemops.size;
            }
        } else {
            buf = mV4l2buf[index].buf;
        }

        CameraLogV("releasePreviewFrame Camera[%d] bufindex:%d  dmafd:%d",mCameraId,index,buf.m.planes[0].m.fd);
        ret = ioctl(mCameraFd, VIDIOC_QBUF, &buf);
        if (ret != 0) {
            CameraLogE("releasePreviewFrame: VIDIOC_QBUF Failed: index = %d, ret = %d, %s",
                       buf.index, ret, strerror(errno));
        } else {
            // memset(&mV4l2buf[index], 0, sizeof(V4L2BUF_t));
            mCurAvailBufferCnt++;
            CameraLogV("buffer queue succeed!! index %d", buf.index);
        }
        // CameraLogD("r ID: %d cid=%d cur ava bug =%d",
        // buf.index,mV4l2Info.device_id,mCurAvailBufferCnt);
    }
    pthread_mutex_unlock(&mCaptureMutex);
}

int V4L2CameraDevice::getPreviewFrame(v4l2_buffer *buf) {
    int ret = UNKNOWN_ERROR;

    if (mCameraType == CAMERA_TYPE_CSI) {
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf->length = nPlanes;
        buf->m.planes = planes;
    } else {
        buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    buf->memory = mV4l2_memory;

    ret = ioctl(mCameraFd, VIDIOC_DQBUF, buf);
    if (ret < 0) {
        CameraLogW("GetPreviewFrame: VIDIOC_DQBUF Failed, %s", strerror(errno));
        return __LINE__;  // can not return false
    }

    testFrameRate();
    return OK;
}

int V4L2CameraDevice::tryFmt(int format) {
    struct v4l2_fmtdesc fmtdesc;
    if (mCameraType == CAMERA_TYPE_CSI) {
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    for (int i = 0; i < 12; i++) {
        fmtdesc.index = i;
        if (-1 == ioctl(mCameraFd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            break;
        }
        CameraLogV("format index = %d, name = %s, v4l2 pixel format = %x", i, fmtdesc.description,
                   fmtdesc.pixelformat);

        if (fmtdesc.pixelformat == (__u32)format) {
            return OK;
        }
    }

    return -1;
}

int V4L2CameraDevice::tryFmtSize(int *width, int *height) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_format fmt;

    CameraLogV("V4L2Camera::TryFmtSize: w: %d, h: %d", *width, *height);

    memset(&fmt, 0, sizeof(fmt));
    if (mCameraType == CAMERA_TYPE_CSI) {
        mVideoFormat = V4L2_PIX_FMT_NV21;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.pixelformat = mVideoFormat;
        fmt.fmt.pix_mp.width = *width;
        fmt.fmt.pix_mp.height = *height;
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;  // V4L2_FIELD_INTERLACED;
        fmt.fmt.pix_mp.pixelformat = mVideoFormat;
        CameraLogD("bill video 0x%x %d x %d NV21 is 0x%x", mVideoFormat, fmt.fmt.pix_mp.width,
                   fmt.fmt.pix_mp.height, V4L2_PIX_FMT_NV21, V4L2_PIX_FMT_YUYV);
    } else {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = *width;
        fmt.fmt.pix.height = *height;
        if (mCaptureFormat == V4L2_PIX_FMT_MJPEG) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        } else if (mCaptureFormat == V4L2_PIX_FMT_YUYV) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        } else if (mCaptureFormat == V4L2_PIX_FMT_H264) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
        } else {
            fmt.fmt.pix.pixelformat = mVideoFormat;
        }
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
    }

    ret = ioctl(mCameraFd, VIDIOC_TRY_FMT, &fmt);
    if (ret < 0) {
        CameraLogE("VIDIOC_TRY_FMT Failed: %s", strerror(errno));
        return ret;
    }

    // driver surpport this size
    if (mCameraType == CAMERA_TYPE_CSI) {
        *width = fmt.fmt.pix_mp.width;
        *height = fmt.fmt.pix_mp.height;
    } else {
        *width = fmt.fmt.pix.width;
        *height = fmt.fmt.pix.height;
    }
    CameraLogD("After V4L2Camera::TryFmtSize: w: %d, h: %d", *width, *height);

    return 0;
}

int V4L2CameraDevice::setFrameRate(int rate) {
    CameraLogD("setFrameRate:%d", rate);
    mFrameRate = rate;
    return OK;
}

int V4L2CameraDevice::getFrameRate() {
    CAMERA_LOG;
    int ret = -1;

    struct v4l2_streamparm parms;
    if (mCameraType == CAMERA_TYPE_CSI) {
        parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    ret = ioctl(mCameraFd, VIDIOC_G_PARM, &parms);
    if (ret < 0) {
        CameraLogE("VIDIOC_G_PARM getFrameRate error, %s", strerror(errno));
        return ret;
    }

    int numerator = parms.parm.capture.timeperframe.numerator;
    int denominator = parms.parm.capture.timeperframe.denominator;

    CameraLogV("frame rate: numerator = %d, denominator = %d", numerator, denominator);

    if (numerator != 0 && denominator != 0) {
        return denominator / numerator;
    } else {
        CameraLogW("unsupported frame rate: %d/%d", denominator, numerator);
        return 30;
    }
}

int V4L2CameraDevice::setImageEffect(int effect) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_COLORFX;
    ctrl.value = effect;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setImageEffect failed!");
    } else {
        CameraLogV("setImageEffect ok");
    }
    return ret;
}

int V4L2CameraDevice::setWhiteBalance(int wb) {
    struct v4l2_control ctrl;
    int ret = -1;

    ctrl.id = V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE;
    ctrl.value = wb;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setWhiteBalance failed, %s", strerror(errno));
    } else {
        CameraLogV("setWhiteBalance ok");
    }
    return ret;
}

int V4L2CameraDevice::setTakePictureCtrl() {
    struct v4l2_control ctrl;
    int ret = -1;
    if (mV4l2Info.fast_picture_mode) {
        ctrl.id = V4L2_CID_TAKE_PICTURE;
        ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
        if (ret < 0) {
            CameraLogV("setTakePictureCtrl failed, %s", strerror(errno));
        } else {
            CameraLogV("setTakePictureCtrl ok");
        }
        return ret;
    }
    return 0;
}

// ae mode
int V4L2CameraDevice::setExposureMode(int mode) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_EXPOSURE_AUTO;
    ctrl.value = mode;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setExposureMode failed, %s", strerror(errno));
    } else {
        CameraLogV("setExposureMode ok");
    }
    return ret;
}

// ae compensation
int V4L2CameraDevice::setExposureCompensation(int val) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_EXPOSURE_BIAS;
    ctrl.value = val;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setExposureCompensation failed, %s", strerror(errno));
    } else {
        CameraLogV("setExposureCompensation ok");
    }
    return ret;
}

int V4L2CameraDevice::setExposureWind(int num, void *wind) {
    UNUSED(wind);
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_EXPOSURE_WIN_NUM;
    ctrl.value = num;
    // ctrl.user_pt = (unsigned int)wind;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogE("setExposureWind failed, %s", strerror(errno));
    } else {
        CameraLogV("setExposureWind ok");
    }
    return ret;
}

// flash mode
int V4L2CameraDevice::setFlashMode(int mode) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_FLASH_LED_MODE;
    ctrl.value = mode;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setFlashMode failed, %s", strerror(errno));
    } else {
        CameraLogV("setFlashMode ok");
    }
    return ret;
}

// af init
int V4L2CameraDevice::setAutoFocusInit() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    if (mCameraType == CAMERA_TYPE_CSI) {
        ctrl.id = V4L2_CID_AUTO_FOCUS_INIT;
        ctrl.value = 0;
        ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
        if (ret < 0) {
            CameraLogE("setAutoFocusInit failed, %s", strerror(errno));
        } else {
            CameraLogV("setAutoFocusInit ok");
        }
    }
    return ret;
}

// af release
int V4L2CameraDevice::setAutoFocusRelease() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_FOCUS_RELEASE;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogE("setAutoFocusRelease failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusRelease ok");
    }
    return ret;
}

// af range
int V4L2CameraDevice::setAutoFocusRange(int af_range) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_FOCUS_AUTO;
    ctrl.value = 1;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setAutoFocusRange id V4L2_CID_FOCUS_AUTO failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusRange id V4L2_CID_FOCUS_AUTO ok");
    }
    ctrl.id = V4L2_CID_AUTO_FOCUS_RANGE;
    ctrl.value = af_range;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogV("setAutoFocusRange id V4L2_CID_AUTO_FOCUS_RANGE failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusRange id V4L2_CID_AUTO_FOCUS_RANGE ok");
    }
    return ret;
}

// af wind
int V4L2CameraDevice::setAutoFocusWind(int num, void *wind) {
    UNUSED(wind);
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_FOCUS_WIN_NUM;
    ctrl.value = num;
    // ctrl.user_pt = (unsigned int)wind;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogE("setAutoFocusCtrl failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusCtrl ok");
    }
    return ret;
}

// af start
int V4L2CameraDevice::setAutoFocusStart() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_FOCUS_START;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogE("setAutoFocusStart failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusStart ok");
    }
    return ret;
}

// get horizontal visual angle
int V4L2CameraDevice::getHorVisualAngle() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    if (mCameraType == CAMERA_TYPE_CSI) {
        ctrl.id = V4L2_CID_HOR_VISUAL_ANGLE;
        ret = ioctl(mCameraFd, VIDIOC_G_CTRL, &ctrl);
        if (ret < 0) {
            CameraLogE("getHorVisualAngle failed(%s)", strerror(errno));
            return ret;
        }
        return ctrl.value;
    }

    return ret;
}

// get vertical visual angle
int V4L2CameraDevice::getVerVisualAngle() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    if (mCameraType == CAMERA_TYPE_CSI) {
        ctrl.id = V4L2_CID_VER_VISUAL_ANGLE;
        ret = ioctl(mCameraFd, VIDIOC_G_CTRL, &ctrl);
        if (ret < 0) {
            CameraLogE("getVerVisualAngle failed(%s)", strerror(errno));
            return ret;
        }
        return ctrl.value;
    }

    return ret;
}
// af stop
int V4L2CameraDevice::setAutoFocusStop() {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_AUTO_FOCUS_STOP;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        CameraLogE("setAutoFocusStart failed, %s", strerror(errno));
    } else {
        CameraLogV("setAutoFocusStart ok");
    }
    return ret;
}

// get af statue
int V4L2CameraDevice::getAutoFocusStatus() {
    // CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    if (mCameraFd == 0) {
        return 0xFF000000;
    }

    ctrl.id = V4L2_CID_AUTO_FOCUS_STATUS;
    ret = ioctl(mCameraFd, VIDIOC_G_CTRL, &ctrl);
    if (ret >= 0) {
        // CameraLogV("getAutoFocusCtrl ok");
    }

    return ret;
}

int V4L2CameraDevice::getSnrValue() {
    // CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;
    struct v4l2_queryctrl qc_ctrl;

    if (mCameraFd == 0) {
        return 0xFF000000;
    }

    ctrl.id = V4L2_CID_GAIN;
    qc_ctrl.id = V4L2_CID_GAIN;

    if (-1 == ioctl(mCameraFd, VIDIOC_QUERYCTRL, &qc_ctrl)) {
        return 0;
    }

    ret = ioctl(mCameraFd, VIDIOC_G_CTRL, &ctrl);
    return ret;
}

int V4L2CameraDevice::set3ALock(int lock) {
    CAMERA_LOG;
    int ret = -1;
    struct v4l2_control ctrl;

    ctrl.id = V4L2_CID_3A_LOCK;
    ctrl.value = lock;
    ret = ioctl(mCameraFd, VIDIOC_S_CTRL, &ctrl);
    if (ret >= 0) {
        CameraLogV("set3ALock ok");
    }
    return ret;
}

int V4L2CameraDevice::v4l2setCaptureParams() {
    CAMERA_LOG;
    int ret = -1;

    struct v4l2_streamparm params;
    params.parm.capture.timeperframe.numerator = 1;
    params.parm.capture.timeperframe.denominator = mFrameRate;
    if (mCameraType == CAMERA_TYPE_CSI) {
        params.parm.capture.reserved[0] = 0;
        params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    if (mTakePictureState == TAKE_PICTURE_NORMAL) {
        params.parm.capture.capturemode = V4L2_MODE_IMAGE;
    } else {
        params.parm.capture.capturemode = V4L2_MODE_VIDEO;
    }

    CameraLogV("v4l2setCaptureParams FrameRate:%d, CaptureMode:%d", mFrameRate,
               params.parm.capture.capturemode);

    ret = ioctl(mCameraFd, VIDIOC_S_PARM, &params);
    if (ret < 0) {
        CameraLogE("v4l2setCaptureParams failed, %s", strerror(errno));
    } else {
        CameraLogV("v4l2setCaptureParams ok");
    }
    return ret;
}

int V4L2CameraDevice::enumSize(char *pSize, int len) {
    if (pSize == NULL) {
        CameraLogE("error input params");
        return -1;
    }

    char str[16];
    memset(str, 0, 16);
    memset(pSize, 0, len);

    if (mCameraType == CAMERA_TYPE_TVIN_NTSC) {
        sprintf(str, "%dx%d", 720, 480);
        strcat(pSize, str);
        return OK;
    }
    if (mCameraType == CAMERA_TYPE_TVIN_PAL) {
        sprintf(str, "%dx%d", 720, 576);
        strcat(pSize, str);
        return OK;
    }

    struct v4l2_frmsizeenum size_enum;
    if (mCameraType == CAMERA_TYPE_CSI) {
        size_enum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        size_enum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    size_enum.pixel_format = mCaptureFormat;

    for (int i = 0; i < 20; i++) {
        size_enum.index = i;
        if (-1 == ioctl(mCameraFd, VIDIOC_ENUM_FRAMESIZES, &size_enum)) {
            break;
        }
        CameraLogV("format index = %d, size_enum: %dx%d", i, size_enum.discrete.width,
                   size_enum.discrete.height);
        sprintf(str, "%dx%d", size_enum.discrete.width, size_enum.discrete.height);
        if (i != 0) {
            strcat(pSize, ",");
        }
        strcat(pSize, str);
    }

    return OK;
}

int V4L2CameraDevice::getFullSize(int *full_w, int *full_h) {
    if (mCameraType == CAMERA_TYPE_TVIN_NTSC) {
        *full_w = 720;
        *full_h = 480;
        CameraLogV("getFullSize: %dx%d", *full_w, *full_h);
        return OK;
    }
    if (mCameraType == CAMERA_TYPE_TVIN_PAL) {
        *full_w = 720;
        *full_h = 576;
        CameraLogV("getFullSize: %dx%d", *full_w, *full_h);
        return OK;
    }

    struct v4l2_frmsizeenum size_enum;
    if (mCameraType == CAMERA_TYPE_CSI) {
        size_enum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else {
        size_enum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    size_enum.pixel_format = mCaptureFormat;
    size_enum.index = 0;
    if (-1 == ioctl(mCameraFd, VIDIOC_ENUM_FRAMESIZES, &size_enum)) {
        CameraLogE("getFullSize failed");
        return -1;
    }

    *full_w = size_enum.discrete.width;
    *full_h = size_enum.discrete.height;

    CameraLogV("getFullSize: %dx%d", *full_w, *full_h);

    return OK;
}

int V4L2CameraDevice::getSuitableThumbScale(int full_w, int full_h) {
    int scale = 1;
    if (mIsThumbUsedForVideo == true) {
        scale = 2;
    }
    if (full_w * full_h > 10 * 1024 * 1024)  // maybe 12m,13m,16m
        return 2;
    else if (full_w * full_h > 4.5 * 1024 * 1024)  // maybe 5m,8m
        return 2;
    else
        return scale;  // others
    return 1;          // failed
}

int V4L2CameraDevice::getTVINSystemType() {
    // int tvinType = -1;
    if (mCameraFd < 0) return -1;

    struct v4l2_format format;
    struct v4l2_frmsizeenum frmsize;
    // enum v4l2_buf_type type;
    int i = 0;
    // int temp_height = 0;
    int fd = mCameraFd;
    int cvbs_type = 1;
    memset(&format, 0, sizeof(struct v4l2_format));
    memset(&frmsize, 0, sizeof(struct v4l2_frmsizeenum));
    frmsize.pixel_format = V4L2_PIX_FMT_NV21;
    frmsize.index = 0;
    while ((!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) && (i < 20)) {
        CameraLogV("i = %d", i);
        CameraLogV("getTVINSystemType width = %d, height = %d", frmsize.discrete.width,
                   frmsize.discrete.height);
        i++;
        frmsize.index++;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;

    switch (cvbs_type) {
        case 0:
        case 1:
            i = 0;
            while (ioctl(fd, VIDIOC_G_FMT, &format) && (i++ < 20)) {
                CameraLogE("get tvin signal failed.");
                return -1;
            }

            if (format.fmt.pix.height == 480) {
                return TVD_NTSC;
            } else if (format.fmt.pix.height == 576) {
                return TVD_PAL;
            } else {
                CameraLogE("not NTSC and PAL get tvin signal failed.");
                return -1;
            }
        case 2:
            format.fmt.pix.width = 1440;
            format.fmt.pix.height = 960;
            return TVD_NTSC;

        case 3:
            format.fmt.pix.width = 1440;
            format.fmt.pix.height = 1152;
            return TVD_PAL;

        default:
            format.fmt.pix.width = 720;
            format.fmt.pix.height = 576;
            if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
                CameraLogE("set tvin image format failed 4");
                return -1;
            }
            return TVD_PAL;
    }
}

int V4L2CameraDevice::getTVINSystemType(int fd, int cvbs_type) {
    struct v4l2_format format;
    struct v4l2_frmsizeenum frmsize;
    // enum v4l2_buf_type type;
    int i = 0;
    // int temp_height = 0;

    memset(&format, 0, sizeof(struct v4l2_format));
    memset(&frmsize, 0, sizeof(struct v4l2_frmsizeenum));
    frmsize.pixel_format = V4L2_PIX_FMT_NV21;
    frmsize.index = 0;
    while ((!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) && (i < 20)) {
        CameraLogV("i = %d", i);
        CameraLogV("getTVINSystemType2 width = %d, height = %d", frmsize.discrete.width,
                   frmsize.discrete.height);
        i++;
        frmsize.index++;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;

    switch (cvbs_type) {
        case 0:
        case 1:
            i = 0;
            while (ioctl(fd, VIDIOC_G_FMT, &format) && (i++ < 20)) {
                printf("get tvin signal failed.");
                return -1;
            }
            // after VIDIOC_G_FMT,return's format contains pix.width and pix.height and fmt.raw_data
            // only

            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;

            if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
                CameraLogV("set tvin image format failed 1");
                return -1;
            }

            if (format.fmt.pix.height == 480) {
                return TVD_NTSC;
            } else if (format.fmt.pix.height == 576) {
                return TVD_PAL;
            } else {
                CameraLogE("not NTSC and PAL get tvin signal failed.");
                return -1;
            }
        case 2:
            format.fmt.pix.width = 1440;
            format.fmt.pix.height = 960;
            return TVD_NTSC;
        case 3:
            format.fmt.pix.width = 1440;
            format.fmt.pix.height = 1152;
            return TVD_PAL;
        default:
            format.fmt.pix.width = 720;
            format.fmt.pix.height = 576;
            if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
                CameraLogE("set tvin image format failed 4");
                return -1;
            }
            return TVD_PAL;
    }
}

void V4L2CameraDevice::setVolatileCtrl(int type, int value) {
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;

    memset(&queryctrl, 0, sizeof(queryctrl));
    queryctrl.id = type;

    memset(&control, 0, sizeof(control));
    control.id = type;
    control.value = value;
    if (-1 == ioctl(mCameraFd, VIDIOC_S_CTRL, &control)) {
        CameraLogE("set setVolatileCtrl type:0x%lx error!,error=%s", type, strerror(errno));
        return;
    }
}

int V4L2CameraDevice::getFrameWidth()
{
    return mFrameWidth;
}

int V4L2CameraDevice::getFrameHeight()
{
    return mFrameHeight;
}


void V4L2CameraDevice::setAnalogInputColor(int brightness, int contrast, int saturation) {
    CameraLogV("setAnCameraLogInputColor brightness:%d contrast:%d saturation:%d", brightness,
               contrast, saturation);
    setVolatileCtrl(V4L2_CID_BRIGHTNESS, brightness);
    setVolatileCtrl(V4L2_CID_CONTRAST, contrast);
    setVolatileCtrl(V4L2_CID_SATURATION, saturation);
}

int V4L2CameraDevice::getCameraType() { return mCameraType; }

#ifdef CAMERA_MANAGER_ENABLE
void V4L2CameraDevice::setCameraManager(CameraManager *manager) {
    mCameraManager = manager;
    CameraLogD("setCameraManager manager=0x%x", manager);
}
#endif

}; /* namespace android */
