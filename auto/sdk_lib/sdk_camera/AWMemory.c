//AWMemory.c
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define LOG_TAG "AWCameraMemory"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <ion/ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include "CameraLog.h"
#include "AWMemory.h"
// #include "AWCameraCommon.h"
//#include "hal_public.h"
//#include <nativebase/nativebase.h>
//#include <media/hardware/HardwareAPI.h>

//namespace android {
#define ION_DEV_NAME        "/dev/ion"
#define CEDAR_DEV_NAME      "/dev/cedar_dev"

enum IOCTL_CMD
{
    IOCTL_UNKOWN = 0x100,
    IOCTL_GET_ENV_INFO,
    IOCTL_WAIT_VE_DE,
    IOCTL_WAIT_VE_EN,
    IOCTL_RESET_VE,
    IOCTL_ENABLE_VE,
    IOCTL_DISABLE_VE,
    IOCTL_SET_VE_FREQ,

    IOCTL_CONFIG_AVS2 = 0x200,
    IOCTL_GETVALUE_AVS2,
    IOCTL_PAUSE_AVS2,
    IOCTL_START_AVS2,
    IOCTL_RESET_AVS2,
    IOCTL_ADJUST_AVS2,
    IOCTL_ENGINE_REQ,
    IOCTL_ENGINE_REL,
    IOCTL_ENGINE_CHECK_DELAY,
    IOCTL_GET_IC_VER,
    IOCTL_ADJUST_AVS2_ABS,
    IOCTL_FLUSH_CACHE,
    IOCTL_SET_REFCOUNT,
    IOCTL_FLUSH_CACHE_ALL,
    IOCTL_TEST_VERSION,

    IOCTL_GET_LOCK = 0x310,
    IOCTL_RELEASE_LOCK,

    IOCTL_SET_VOL = 0x400,

    IOCTL_WAIT_JPEG_DEC = 0x500,
    /*for get the ve ref_count for ipc to delete the semphore*/
    IOCTL_GET_REFCOUNT,

    /*for iommu*/
    IOCTL_GET_IOMMU_ADDR,
    IOCTL_FREE_IOMMU_ADDR,

    /*for debug*/
    IOCTL_SET_PROC_INFO,
    IOCTL_STOP_PROC_INFO,
    IOCTL_COPY_PROC_INFO,

    IOCTL_SET_DRAM_HIGH_CHANNAL = 0x600,
};

typedef struct _user_iommu_param
{
    int             fd;
    unsigned int    iommu_addr;
} user_iommu_param;

typedef struct _AWMEMORY_CONTEXT
{
    int ionFd;
    int cedarFd;
    int ionRefcnt;
    int cedarRefcnt;
    int flag_iommu;
} AWMEMORY_CONTEXT;

AWMEMORY_CONTEXT *g_awmemory_context = NULL;
pthread_mutex_t g_mutex_memory = PTHREAD_MUTEX_INITIALIZER;

static int aw_memory_init()
{
    CameraLogD("%s-%d",__FUNCTION__,__LINE__);
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context != NULL) {
        pthread_mutex_unlock(&g_mutex_memory);
        CameraLogD("%s-%d",__FUNCTION__,__LINE__);
        return 1;
    }
    g_awmemory_context = (AWMEMORY_CONTEXT*) malloc(sizeof(AWMEMORY_CONTEXT));
    if (g_awmemory_context == NULL) {
        CameraLogE("create AWMEMORY_CONTEXT, out of memory");
        pthread_mutex_unlock(&g_mutex_memory);
        return -1;
    }
    memset((void*)g_awmemory_context, 0, sizeof(AWMEMORY_CONTEXT));
    pthread_mutex_unlock(&g_mutex_memory);
    CameraLogD("%s-%d",__FUNCTION__,__LINE__);
    return 0;
}

static int aw_memory_deinit()
{
    CameraLogD("%s-%d",__FUNCTION__,__LINE__);
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context != NULL) {
        CameraLogD("%s-%d",__FUNCTION__,__LINE__);
        free(g_awmemory_context);
        g_awmemory_context = NULL;
        pthread_mutex_unlock(&g_mutex_memory);
        return 0;
   }
   pthread_mutex_unlock(&g_mutex_memory);
   return 1;
}

int open_cedar_dev()
{
    int ret = 0;
    CameraLogD("%s-%d",__FUNCTION__,__LINE__);
    aw_memory_init();
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context == NULL) {
        pthread_mutex_unlock(&g_mutex_memory);
        return -1;
    }
    CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
    if (g_awmemory_context->cedarRefcnt <= 0) {
       g_awmemory_context->cedarFd = open(CEDAR_DEV_NAME, O_RDONLY, 0);
    }
    g_awmemory_context->cedarRefcnt++;
    pthread_mutex_unlock(&g_mutex_memory);
    CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
    return 0;
}

int close_cedar_dev()
{
    CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context == NULL) {
        pthread_mutex_unlock(&g_mutex_memory);
        CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
        return -1;
    }
    CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
    if (--g_awmemory_context->cedarRefcnt <= 0) {
        CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
        if (g_awmemory_context->cedarFd > 0) {
            CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
            close(g_awmemory_context->cedarFd);
            g_awmemory_context->cedarFd = 0;
        }
    }
    pthread_mutex_unlock(&g_mutex_memory);
    CameraLogD("%s-%d,cedarRefcnt:%d",__FUNCTION__,__LINE__,g_awmemory_context->cedarRefcnt);
    return 0;
}

int open_ion_dev()
{
    int ret = 0;
    aw_memory_init();
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context == NULL) {
        pthread_mutex_unlock(&g_mutex_memory);
        return -1;
    }
    if (g_awmemory_context->ionRefcnt <= 0) {
       g_awmemory_context->ionFd = open(ION_DEV_NAME, O_RDONLY, 0);
    }
    g_awmemory_context->ionRefcnt++;
    pthread_mutex_unlock(&g_mutex_memory);
    return 0;
}

int close_ion_dev()
{
    pthread_mutex_lock(&g_mutex_memory);
    if (g_awmemory_context == NULL) {
        pthread_mutex_unlock(&g_mutex_memory);
        return -1;
    }
    if (--g_awmemory_context->ionRefcnt <= 0) {
        if (g_awmemory_context->ionFd > 0) {
            close(g_awmemory_context->ionFd);
            g_awmemory_context->ionFd = 0;
        }
    }
    pthread_mutex_unlock(&g_mutex_memory);
    return 0;
}

int alloc_buffer(int size)
{
    return 0;
}

int free_buffer(int fd)
{
    return 0;
}

unsigned long ion_get_phyaddr_from_fd(int fd)
{
    int ret;
    unsigned long phy_addr = -1;

    CameraLogD("%s-%d,fd:%d",__FUNCTION__,__LINE__,fd);
    if (g_awmemory_context == NULL) {
        CameraLogE("g_awmemory_context is null");
        return -1;
    }

    if (g_awmemory_context->cedarFd < 0) {
        CameraLogE("open g_awmemory_context->cedarFd is error");
        return -1;
    }
    ret = ioctl(g_awmemory_context->cedarFd, IOCTL_ENGINE_REQ, 0);
    if (0 == ret) {
        user_iommu_param stUserIommuBuf;
        memset(&stUserIommuBuf, 0, sizeof(user_iommu_param));
        stUserIommuBuf.fd = fd;
        ret = ioctl(g_awmemory_context->cedarFd, IOCTL_GET_IOMMU_ADDR, &stUserIommuBuf);
        if (ret != 0) {
            CameraLogE("get iommu addr fail! ret:[%d]", ret);
            ret = ioctl(g_awmemory_context->cedarFd, IOCTL_ENGINE_REL, 0);
            if (ret != 0) {
                CameraLogE("fatal error! ENGINE_REL err, ret:%d", ret);
            }
            return -1;
        } else {
            phy_addr = stUserIommuBuf.iommu_addr;
        }
    } else {
        CameraLogE("fatal error! ENGINE_REQ err, ret %d", ret);
        return -1;
    }
    CameraLogD("%s-%d,fd:%d->0x%lx end",__FUNCTION__,__LINE__,fd,phy_addr);
    return phy_addr;
}

int ion_return_phyaddr(int fd, unsigned long phy_addr)
{
    int ret;
    user_iommu_param stUserIommuBuf;
    if (g_awmemory_context == NULL) {
        return -1;
    }
    memset(&stUserIommuBuf, 0, sizeof(user_iommu_param));
    stUserIommuBuf.fd = fd;
#ifdef TARGET_KERNEL_5_4
    stUserIommuBuf.iommu_addr = phy_addr;
#endif
    ret = ioctl(g_awmemory_context->cedarFd, IOCTL_FREE_IOMMU_ADDR, &stUserIommuBuf);
    if (ret != 0) {
        CameraLogE("(f:%s, l:%d) fatal error! free iommu addr fail[%d]", __FUNCTION__, __LINE__, ret);
    }
    ret = ioctl(g_awmemory_context->cedarFd, IOCTL_ENGINE_REL, 0);
    if (ret != 0) {
        CameraLogE("fatal error! ENGINE_REL err, ret %d", ret);
    }
    return ret;
}

unsigned long ion_get_viraddr_from_fd(int fd,int sizes)
{
    unsigned long addr_vir;
    //CameraLogD("ion_get_viraddr_from_fd:%d,%d",fd,sizes);
    addr_vir = (unsigned long) mmap(NULL, sizes,PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if ((unsigned long)MAP_FAILED == addr_vir) {
        CameraLogE("mmap err, ret %lu\n", (unsigned long)addr_vir);
        return (unsigned long)MAP_FAILED;
    }
    return addr_vir;
}

int ion_return_viraddr(unsigned long viraddr,int size)
{
    int ret;
    //CameraLogD("ion_return_viraddr:0x%x,size:%d",viraddr,size);
    ret = munmap((void*)viraddr, size);
    if (ret) {
        CameraLogE("%s:%d error",__FUNCTION__,__LINE__);
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
