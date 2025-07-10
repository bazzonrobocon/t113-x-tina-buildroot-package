
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "dma-buf.h"
#include "dma-heap.h"

int dmabuf_heap_open(void)
{
#ifdef CONF_USE_IOMMU
#if IS_CACHED
    int fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
#else
    int fd = open("/dev/dma_heap/system-uncached", O_RDONLY | O_CLOEXEC);
#endif
#else
    int fd = open("/dev/dma_heap/reserved", O_RDONLY | O_CLOEXEC);
#endif
    if (fd < 0)
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
    return fd;
}

int dmabuf_heap_close(int fd)
{
    int ret = close(fd);
    if (ret < 0) {
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
        return -errno;
    }
    return ret;
}

int dmabuf_heap_alloc(int fd, size_t len)
{
    int ret = 0;
    struct dma_heap_allocation_data heap_data;
    heap_data.len = len;
    heap_data.fd_flags = O_RDWR | O_CLOEXEC;
    heap_data.fd = 0;
    heap_data.heap_flags = 0;

    ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
    if (ret < 0) {
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
        return -1;
    }
    return heap_data.fd;
}

void *dmabuf_heap_mmap(int buf_fd, size_t len)
{
    /* mmap to user */
    void *pVirAddr = NULL;
    pVirAddr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0);
    if (MAP_FAILED == pVirAddr) {
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
        return NULL;
    }

    return pVirAddr;
}

int dmabuf_heap_munmap(size_t len, void *pVirAddr)
{
    int ret;
    ret = munmap((void *)pVirAddr, len);
    if (ret) {
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
        return -errno;
    }

    return 0;
}

int dmabuf_sync(int fd, int start_stop)
{
    struct dma_buf_sync sync;
    int ret = 0;
    sync.flags = start_stop | DMA_BUF_SYNC_RW;

    ret = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret)
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
    return ret;
}

int dmabuf_set_name(int fd, const unsigned char *name)
{
    int ret = 0;
    ret = ioctl(fd, DMA_BUF_SET_NAME, name);
    if (ret)
        printf("dmabuf_debug:%s:%s:%d failed %s\n", __FILE__, __func__, __LINE__,
               strerror(errno));
    return ret;
}