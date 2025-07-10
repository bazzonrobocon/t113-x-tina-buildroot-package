/*
 * This confidential and proprietary software should be used
 * under the licensing agreement from Allwinner Technology.

 * Copyright (C) 2020-2030 Allwinner Technology Limited
 * All rights reserved.

 * Author:zhengwanyu <zhengwanyu@allwinnertech.com>

 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from Allwinner Technology Limited.
 */
#include "sdklog.h"
#define LOG_TAG "render"

#include <stdio.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>


#include <renderengine_uapi.h>
#include <drm_fourcc.h>
#include "sunxiMemInterface.h"

//#define ON_SCREEN

int getDisplayScreenInfo(unsigned int *width, unsigned int *height)
{
	struct fb_var_screeninfo vinfo;
	int fd;

	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		ALOGE("failed to open dev/fb0\n");
		return -1;
	}

	memset(&vinfo, 0, sizeof(vinfo));
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		ALOGE("failed to get fb info\n");
		return -1;
	}

	*width = vinfo.xres;
	*height = vinfo.yres;

	ALOGD("fbdev size:%dx%d\n", *width, *height);

	close(fd);
	return 0;
}

int loadFile(paramStruct_t  *mem, const char *path, int size)
{
	FILE *file;
	ssize_t n;

	file = fopen(path, "w+");
	if (file < 0) {
		ALOGE("fopen %s err.\n", path);
		return -1;
	}

	n = fwrite(mem->vir, 1, size, file);
	if (n < size) {
		ALOGE("fwrite is unormal, wrote totally:%d  but need:%ld\n",
			(unsigned int)n, size);
	}

	fclose(file);

	return 0;
}


int sendForDisplay(paramStruct_t  *mem)
{
	struct fb_var_screeninfo vinfo;
	int fd;
	unsigned char *fbbuf;

	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		ALOGE("failed to open dev/fb0\n");
		return 0;
	}

	memset(&vinfo, 0, sizeof(vinfo));
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		ALOGE("failed to get fb info\n");
		return 0;
	}

	if ((fbbuf = (unsigned char *)mmap(0, mem->size,
		PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, 0)) == (void*) -1) {
		ALOGE("fbdev map video error\n");
		return -1;
	}

//Note: This is only dma buffer transmission. so do NOT need to flush cache
	memcpy(fbbuf, mem->vir, mem->size);

	if (munmap(fbbuf, mem->size)) {
		ALOGE("munmap failed\n");
		return -1;
	}

	close(fd);
	return 0;
}

static int dma_alloc(paramStruct_t* pops, unsigned int size)
{
	int iRet = 0;

	iRet = allocOpen(MEM_TYPE_DMA, pops, NULL);
    if (iRet < 0) {
        ALOGE("ion_alloc_open failed");
        return iRet;
    }
    pops->size = size;

    iRet = allocAlloc(MEM_TYPE_DMA, pops, NULL);
    if (iRet < 0) {
        ALOGE("allocAlloc failed");
        return iRet;
    }
    ALOGD("pops.vir=%p pops.phy=%p dmafd=%d,alloc len=%d", pops->vir,
          pops->phy, pops->ion_buffer.fd_data.aw_fd, pops->size);

	return 0;
}

void ion_free(paramStruct_t* pops)
{
    allocFree(MEM_TYPE_DMA, pops, NULL);
}

int
main(int argc, char *argv[])
{
	int ret;
	void *addr_file;
	int file_fd;
	unsigned int in_size, in_width, in_height;
	char *in_file;
	char *out_file[30];
	unsigned int in_format, out_format;
#ifndef ON_SCREEN
	unsigned int out_width, out_height;
	void *sync;
#endif
	int ion_fd;
	paramStruct_t in_mem, out_mem;

//!!!NOTE, before run this demo,please fill the parameters belows
	in_size = 1920 * 1080 * 3 / 2;
	in_file = "./pic_bin/1080_1920x1080.nv21";
	in_width = 1920;
	in_height = 1080;
	in_format = DRM_FORMAT_NV21;

	struct RE_rect crop = {
		.left = 0,
		.top = 0,
		.right = 1920,
		.bottom = 1080,
	};


	struct RE_rect dstWinRect = {
		.left = 550,
		.top = 200,
		.right = 750,
		.bottom = 400,
	};

	struct RE_rect dstWinRect1 = {
		.left = 200,
		.top = 100,
		.right = 600,
		.bottom = 700,
	};



#ifdef ON_SCREEN
	out_format = DRM_FORMAT_ARGB8888;
	RenderEngine_t render = renderEngineCreate(true, 1,
				in_format, out_format);
#else
	getDisplayScreenInfo(&out_width, &out_height);
	out_format = DRM_FORMAT_NV21;
	//create out memory
	ret = dma_alloc(&out_mem, out_width * out_height * 3 / 2);
	if (ret < 0) {
		ALOGE("dma alloc for out memory\n");
		return ret;
	}
	RenderEngine_t render = renderEngineCreate(false, 1,
					in_format, out_format);
	renderEngineSetOffScreenTarget(render,
				out_width,
				out_height,
				out_mem.ion_buffer.fd_data.aw_fd,
				0);
#endif

	float degree = 0.0f;

	while(1) {
//create in memory
		ret = dma_alloc(&in_mem, in_size);
		if (ret < 0) {
			ALOGE("dma alloc for in memory\n");
			return ret;
		}

		file_fd = open(in_file, O_RDWR);
		if (file_fd < 0) {
			ALOGE("open %s  err.\n", in_file);
			return -1;
		}
		addr_file = (void *)mmap((void *)0, in_size,
					 PROT_READ | PROT_WRITE, MAP_SHARED,
					 file_fd, 0);
		memcpy(in_mem.vir, addr_file , in_size);
		munmap(addr_file, in_size);
		close(file_fd);

		struct RESurface surface;

		surface.srcFd = in_mem.ion_buffer.fd_data.aw_fd;
		surface.srcDataOffset = 0;
		surface.srcWidth = in_width;
		surface.srcHeight = in_height;
		memcpy(&surface.srcCrop, &crop, sizeof(crop));
		memcpy(&surface.dstWinRect, &dstWinRect, sizeof(dstWinRect));
		surface.rotateAngle = degree;

		renderEngineSetSurface(render, &surface);

// fix mem leak
#if 0
		struct RESurface surface1;

		surface1.srcFd = in_mem.dmafd;
		surface1.srcDataOffset = 0;
		surface1.srcWidth = in_width;
		surface1.srcHeight = in_height;
		memcpy(&surface1.srcCrop, &crop, sizeof(crop));
		memcpy(&surface1.dstWinRect, &dstWinRect1, sizeof(dstWinRect1));

		renderEngineSetSurface(render, &surface1);
#endif

#ifdef ON_SCREEN
		ret = renderEngineDrawOnScreen(render);
		if (ret < 0) {
			ALOGE("drawLayer failed\n");
			return -1;
		}
#else
		ret = renderEngineDrawOffScreen(render, &sync);
		if (ret < 0) {
			ALOGE("drawLayer failed\n");
			return -1;
		}

		ret = renderEngineWaitSync(render, sync);
		if (ret < 0) {
			ALOGE("waitSync failed\n");
			return -1;
		}

		sprintf(out_file, "/tmp/%d_%d_outpic.bin", out_width, out_height);
		loadFile(&out_mem, out_file, out_width * out_height * 3 / 2);
		//sendForDisplay(&out_mem);
	
		/* NOTE!!! Must wait for finishing displaying the layer,
		 * Or will cause screen tearing*/
		usleep(16667);
#endif
		ion_free(&in_mem);

		getchar();
		degree += 45.0f;

		if (degree == 360.0f)
			degree = 0.0f;
	}


	ion_free(&out_mem);
	renderEngineDestroy(render);

	ion_close(ion_fd);

	ALOGD("render demo exit\n");
	return 0;
}
