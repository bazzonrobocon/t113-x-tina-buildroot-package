#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "hwdisp2.h"
#include "memory/dma-heap.h"

#define LOG_TAG MODULE_NAME
#include "sdklog.h"

using namespace android;

#define SCREEN0 0
#define SCREEN1 1

#define LVDS_SCREEN_ID SCREEN0
#define HDMI_SCREEN_ID SCREEN1
#define VGA_SCREEN_ID SCREEN1
#define CVBS_SCREEN_ID SCREEN1

int open_hdmi()
{
	unsigned long arg[3];
	struct disp_device_config devconfig;
	int fd_disp;

	fd_disp = open("/dev/disp", O_RDWR);
	if (fd_disp < 0) {
		ALOGE("%s:%d failed to open /dev/disp!\n", __func__, __LINE__);
		return -1;
	}
	ALOGD("%s:%d set output HDMI\n", __func__, __LINE__);

	memset(&devconfig, 0, sizeof(devconfig));
	devconfig.type = DISP_OUTPUT_TYPE_HDMI;
	devconfig.bits = DISP_DATA_8BITS;
	devconfig.eotf = DISP_EOTF_GAMMA28;
	devconfig.cs = DISP_BT709;
	devconfig.dvi_hdmi = DISP_HDMI;
	devconfig.range = DISP_COLOR_RANGE_0_255;
	devconfig.format = DISP_CSC_TYPE_RGB;
	devconfig.mode = DISP_TV_MOD_1080P_60HZ;
	// devconfig.format = DISP_CSC_TYPE_YUV420;
	arg[0] = HDMI_SCREEN_ID;
	arg[1] = (unsigned long)&devconfig;

	if (ioctl(fd_disp, DISP_DEVICE_SET_CONFIG, (void *)arg)) {
		ALOGE("%s:%d %s:%d DISP_DEVICE_SET_CONFIG error.\n", __func__,
			  __LINE__);
		close(fd_disp);
		return -1;
	}
	close(fd_disp);
	return 0;
}

static void *test_hdmi(void *arg)
{
	HwDisplay *mcd = HwDisplay::getInstance();
	void *user_addr = NULL;
	int dmabuf_fd = 0;
	int heap_fd = 0;
	int delay_time = 0;
	char **argv = (char **)arg;
	if (argv[1] != NULL)
		delay_time = atoi(argv[1]);
	if (delay_time <= 0) {
		delay_time = 5;
	}

	/* or open_hdmi() */
	mcd->hwd_other_screen(HDMI_SCREEN_ID, DISP_OUTPUT_TYPE_HDMI,
						  DISP_TV_MOD_1080P_60HZ);
	unsigned int width = mcd->getScreenWidth(HDMI_SCREEN_ID);
	unsigned int height = mcd->getScreenHeight(HDMI_SCREEN_ID);
	ALOGD("%s:%d screen %d size [%dx%d]\n", __func__, __LINE__, HDMI_SCREEN_ID,
		  width, height);
	int size = width * height * 4;

	dmabuf_fd = dmabuf_heap_open();
	ALOGD("%s:%d heap_fd %d \n", __func__, __LINE__, dmabuf_fd);
	heap_fd = dmabuf_heap_alloc(dmabuf_fd, size);
	user_addr = (void *)dmabuf_heap_mmap(heap_fd, size);
	memset((void *)user_addr, 0, size);
	int i = 0;
	for (i = 0; i < size / 4; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0x00;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0xFF;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	for (i = size / 4; i < size / 2; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0xFF;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0x00;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	for (i = size / 2; i < size - size / 4; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0xFF;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0x00;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0x00;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	for (i = size - size / 4; i < size; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0xFF;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0xFF;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	ALOGD("%s:%d fill data success!\n", __func__, __LINE__);

	/* set the pram of layer */
	struct view_info frame = {0, 0, width, height};
	struct src_info src = {width, height, DISP_FORMAT_ARGB_8888};
	int layer = mcd->aut_hwd_layer_request(&frame, HDMI_SCREEN_ID, 0, 0);
	mcd->aut_hwd_layer_set_src(layer, &src, 0, heap_fd);
	ALOGD("%s:%d set src success!\n", __func__, __LINE__);
	struct view_info rect = {0, 0, width, height};
	mcd->aut_hwd_layer_sufaceview(layer, &rect);
	mcd->aut_hwd_layer_set_zorder(layer, 10);
	mcd->aut_hwd_layer_set_alpha(layer, 1, 255);
	mcd->aut_hwd_layer_open(layer);

	sleep(delay_time);
	ALOGD("%s:%d show finish!\n", __func__, __LINE__);

	/* close the layers and release memory */
	mcd->aut_hwd_layer_close(layer);
	dmabuf_heap_munmap(size, user_addr);
	dmabuf_heap_close(dmabuf_fd);
	return arg;
}

static void *test_vga(void *arg)
{
	HwDisplay *mcd = HwDisplay::getInstance();
	void *user_addr = NULL;
	int dmabuf_fd = 0;
	int heap_fd = 0;
	int delay_time = 0;
	char **argv = (char **)arg;
	if (argv[1] != NULL)
		delay_time = atoi(argv[1]);
	if (delay_time <= 0) {
		delay_time = 5;
	}

	mcd->hwd_other_screen(VGA_SCREEN_ID, DISP_OUTPUT_TYPE_VGA,
						  DISP_VGA_MOD_1920_1080P_60);
	unsigned int width = mcd->getScreenWidth(VGA_SCREEN_ID);
	unsigned int height = mcd->getScreenHeight(VGA_SCREEN_ID);
	ALOGD("%s:%d screen %d size [%dx%d]\n", __func__, __LINE__, VGA_SCREEN_ID,
		  width, height);
	int size = width * height * 4;

	dmabuf_fd = dmabuf_heap_open();
	ALOGD("%s:%d heap_fd %d \n", __func__, __LINE__, dmabuf_fd);
	heap_fd = dmabuf_heap_alloc(dmabuf_fd, size);
	user_addr = (void *)dmabuf_heap_mmap(heap_fd, size);
	memset((void *)user_addr, 0, size);
	int i = 0;
	for (i = 0; i < size; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0x00;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0xFF;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	ALOGD("%s:%d fill data success!\n", __func__, __LINE__);

	/* set the pram of layer */
	struct view_info frame = {0, 0, width / 2, height / 2};
	struct src_info src = {width, height, DISP_FORMAT_ARGB_8888};
	int layer = mcd->aut_hwd_layer_request(&frame, VGA_SCREEN_ID, 0, 0);
	mcd->aut_hwd_layer_set_src(layer, &src, 0, heap_fd);
	ALOGD("%s:%d set src success!\n", __func__, __LINE__);
	struct view_info rect = {0, 0, width / 2, height / 2};
	mcd->aut_hwd_layer_sufaceview(layer, &rect);
	mcd->aut_hwd_layer_set_zorder(layer, 10);
	mcd->aut_hwd_layer_set_alpha(layer, 1, 255);
	mcd->aut_hwd_layer_open(layer);

	sleep(delay_time);
	ALOGD("%s:%d show finish!\n", __func__, __LINE__);

	/* close the layers and release memory */
	mcd->aut_hwd_layer_close(layer);
	dmabuf_heap_munmap(size, user_addr);
	dmabuf_heap_close(dmabuf_fd);
	return arg;
}

static void *test_cvbs(void *arg)
{
	HwDisplay *mcd = HwDisplay::getInstance();
	void *user_addr = NULL;
	int dmabuf_fd = 0;
	int heap_fd = 0;
	int delay_time = 0;
	char **argv = (char **)arg;
	if (argv[1] != NULL)
		delay_time = atoi(argv[1]);
	if (delay_time <= 0) {
		delay_time = 5;
	}

	mcd->hwd_other_screen(CVBS_SCREEN_ID, DISP_OUTPUT_TYPE_TV,
						  DISP_TV_MOD_NTSC);
	unsigned int width = mcd->getScreenWidth(CVBS_SCREEN_ID);
	unsigned int height = mcd->getScreenHeight(CVBS_SCREEN_ID);
	ALOGD("%s:%d screen %d size [%dx%d]\n", __func__, __LINE__, CVBS_SCREEN_ID,
		  width, height);
	int size = width * height * 4;

	dmabuf_fd = dmabuf_heap_open();
	ALOGD("%s:%d heap_fd %d \n", __func__, __LINE__, dmabuf_fd);
	heap_fd = dmabuf_heap_alloc(dmabuf_fd, size);
	user_addr = (void *)dmabuf_heap_mmap(heap_fd, size);
	memset((void *)user_addr, 0, size);
	int i = 0;
	for (i = 0; i < size; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0x00;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0xFF;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	ALOGD("%s:%d fill data success!\n", __func__, __LINE__);

	/* set the pram of layer */
	struct view_info frame = {0, 0, width / 2, height / 2};
	struct src_info src = {width, height, DISP_FORMAT_ARGB_8888};
	int layer = mcd->aut_hwd_layer_request(&frame, CVBS_SCREEN_ID, 0, 0);
	mcd->aut_hwd_layer_set_src(layer, &src, 0, heap_fd);
	ALOGD("%s:%d set src success!\n", __func__, __LINE__);
	struct view_info rect = {0, 0, width / 2, height / 2};
	mcd->aut_hwd_layer_sufaceview(layer, &rect);
	mcd->aut_hwd_layer_set_zorder(layer, 10);
	mcd->aut_hwd_layer_set_alpha(layer, 1, 255);
	mcd->aut_hwd_layer_open(layer);

	sleep(delay_time);
	ALOGD("%s:%d show finish!\n", __func__, __LINE__);

	/* close the layers and release memory */
	mcd->aut_hwd_layer_close(layer);
	dmabuf_heap_munmap(size, user_addr);
	dmabuf_heap_close(dmabuf_fd);
	return arg;
}

static void *test_lvds(void *arg)
{
	HwDisplay *mcd = HwDisplay::getInstance();
	void *user_addr = NULL;
	int dmabuf_fd = 0;
	int heap_fd = 0;
	int delay_time = 0;
	char **argv = (char **)arg;
	if (argv[1] != NULL)
		delay_time = atoi(argv[1]);
	if (delay_time <= 0) {
		delay_time = 5;
	}

	unsigned int width = mcd->getScreenWidth(LVDS_SCREEN_ID);
	unsigned int height = mcd->getScreenHeight(LVDS_SCREEN_ID);
	ALOGD("%s:%d screen %d size [%dx%d]\n", __func__, __LINE__, LVDS_SCREEN_ID,
		  width, height);
	int size = width * height * 4;

	dmabuf_fd = dmabuf_heap_open();
	ALOGD("%s:%d heap_fd %d \n", __func__, __LINE__, dmabuf_fd);
	heap_fd = dmabuf_heap_alloc(dmabuf_fd, size);
	user_addr = (void *)dmabuf_heap_mmap(heap_fd, size);
	memset((void *)user_addr, 0, size);
	int i = 0;
	for (i = 0; i < size; i++) {
		switch (i % 4) {
		case 0:
			*((char *)user_addr + i) = 0x00;  // b
			break;
		case 1:
			*((char *)user_addr + i) = 0xFF;  // g
			break;
		case 2:
			*((char *)user_addr + i) = 0x00;  // r
			break;
		case 3:
			*((char *)user_addr + i) = 0xFF;  // alpha
			break;
		}
	}
	ALOGD("%s:%d fill data success!\n", __func__, __LINE__);

	/* set the pram of layer */
	struct view_info frame = {0, 0, width / 2, height / 2};
	struct src_info src = {width, height, DISP_FORMAT_ARGB_8888};
	int layer = mcd->aut_hwd_layer_request(&frame, LVDS_SCREEN_ID, 0, 0);
	mcd->aut_hwd_layer_set_src(layer, &src, 0, heap_fd);
	ALOGD("%s:%d set src success!\n", __func__, __LINE__);
	struct view_info rect = {0, 0, width / 2, height / 2};
	mcd->aut_hwd_layer_sufaceview(layer, &rect);
	mcd->aut_hwd_layer_set_zorder(layer, 10);
	mcd->aut_hwd_layer_set_alpha(layer, 1, 255);
	mcd->aut_hwd_layer_open(layer);

	sleep(delay_time);
	ALOGD("%s:%d show finish!\n", __func__, __LINE__);

	/* close the layers and release memory */
	mcd->aut_hwd_layer_close(layer);
	dmabuf_heap_munmap(size, user_addr);
	dmabuf_heap_close(dmabuf_fd);
	return arg;
}

#define MAX_CHANNEL 2
typedef void *(*func_main)(void *arg);
func_main func[] = {
	test_lvds, test_hdmi,
	// test_cvbs,
	// test_vga,
};

static int func_thread(int argc, char **argv)
{
	pthread_t thread[MAX_CHANNEL];
	int ret;
	void *retval;
	int index;
	for (index = 0; index < MAX_CHANNEL; index++) {
		ret = pthread_create(&thread[index], NULL, func[index], argv);
		if (ret) {
			ALOGE("%s:%d pthread_create failed, ret = %d", __func__, __LINE__,
				  ret);
			return -1;
		}
	}
	for (index = 0; index < MAX_CHANNEL; index++) {
		ret = pthread_join(thread[index], &retval);
		if (ret) {
			ALOGE("%s:%d pthread_join failed, ret = %d", __func__, __LINE__,
				  ret);
			return -1;
		}
	}
	return 0;
}

void usage(void)
{
	ALOGE("usage: %s [sleep time]\n", MODULE_NAME);
	ALOGE("sleep time default 5s\n");
}

int main(int argc, char **argv)
{
	ALOGD("version:%s compile: %s %s\n", MODULE_VERSION, __DATE__, __TIME__);
	if (argc > 1 &&
		(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
		usage();
	func_thread(argc, argv);
	return 0;
}
