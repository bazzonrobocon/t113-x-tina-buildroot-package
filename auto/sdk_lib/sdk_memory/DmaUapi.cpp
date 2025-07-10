#define LOG_TAG "DmaUapi"

#include "dma_uapi.h"
#include "sdklog.h"
#include "ion_alloc_list.h"
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <asm-generic/ioctl.h>
#include "ion_alloc_list.h"
#include <linux/types.h>
#include <errno.h>
#include <stdint.h>

#define DEV_NAME "/dev/dma_heap/system"


// #ifdef USE_IOMMU
// #define DEV_NAME "/dev/dma_heap/system"
// #else
// #define DEV_NAME "/dev/dma_heap/reserved"
// #endif


struct DmaContext
{
    int dev_fd;
};

static struct DmaContext *g_Context = NULL;


int dma_open_dev()
{

    if(g_Context == NULL)
    {
        g_Context = (DmaContext *)malloc(sizeof(DmaContext));
        memset(g_Context,0,sizeof(DmaContext));
        g_Context->dev_fd = open(DEV_NAME, O_RDONLY | O_CLOEXEC);
    }
    return g_Context->dev_fd;
}

void dma_close_dev()
{
     close(g_Context->dev_fd);
     free(g_Context);
     g_Context = NULL;
}

int dma_alloc(int size)
{
    struct dma_heap_allocation_data heap_data;
    memset(&heap_data,0,sizeof(dma_heap_allocation_data));
    heap_data.len = size;
    heap_data.fd_flags =  O_RDWR | O_CLOEXEC;
    int ret = ioctl(g_Context->dev_fd, DMA_HEAP_IOC_ALLOC, &heap_data);
    return heap_data.fd;
}


// unsigned int getVirByfd(int dmafd)
// {
//     struct sunxi_drm_phys_data buffer_data;
//     memset(&buffer_data,0,sizeof(sunxi_drm_phys_data));
//     buffer_data.handle = (int)dmafd;
//     int ret = ioctl(g_Context->dev_fd, DMA_HEAP_GET_ADDR, &buffer_data);
//     if (ret != 0)
//     {
//         return -1;
//     }
//     return buffer_data.tee_addr;
// }

// unsigned int getPhyByfd(int dmafd)
// {
//     struct sunxi_drm_phys_data buffer_data;
//     memset(&buffer_data,0,sizeof(sunxi_drm_phys_data));
//     buffer_data.handle = (int)dmafd;
//     int ret = ioctl(g_Context->dev_fd, DMA_HEAP_GET_ADDR, &buffer_data);
//     if (ret != 0)
//     {
//         return -1;
//     }
//     return buffer_data.phys_addr;
// }

int dma_sync(int dmafd)
{
    int flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_START;
    int ret = ioctl(dmafd, DMA_BUF_IOCTL_SYNC, flags);
    if (ret != 0)
    {
        return -1;
    }

    flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END;
    ret = ioctl(dmafd, DMA_BUF_IOCTL_SYNC, flags);

    if (ret != 0)
    {
        return -1;
    }
}

void dma_free(int fd)
{
    close(fd);
}

// int IonAllocOpen();
// int IonAllocClose();
// uint8_t* IonAlloc(int size);
// int IonFree(void *pbuf);
// int IonVir2fd(void *pbuf);
// unsigned long IonVir2ShareFd(void *pbuf);
// unsigned long IonVir2phy(void *pbuf);
// int mem_addr_vir2phy(unsigned long vir, unsigned long *phy);
// uint8_t* IonPhy2vir(void *pbuf);
// void IonFlushCache(void *startAddr, int size);
// void IonFlushCacheAll();
// uint8_t* IonAllocDrm(int size);
// int GetIonTotalMem();
// int IonDmaSync(int dmafd);