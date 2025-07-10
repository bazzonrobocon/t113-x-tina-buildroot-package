#ifndef __AWF_DETECTION_H__
#define __AWF_DETECTION_H__

#include "aw_image.h"
#include <vip_lite.h>


#define MAX_DET_NUM 30
#define NUM_KEYPOINTS 5


#ifdef __cplusplus
       extern "C" {
#endif

typedef struct _awf_detected_face AWF_Det_Face;
struct _awf_detected_face {
	AW_Box bbox;
	AW_LDMK ldmk;

	float confid_score;
};

typedef struct _awf_detection_container AWF_Det_Container;
struct _awf_detection_container{
	vip_network     network;
    int input_count;
    int output_count;
    vip_buffer     *input_buffers;
    vip_buffer     *output_buffers;
	void *models;
	void *settings;
	void *history;
};

typedef struct _awf_detection_settings AWF_Det_Settings;
struct _awf_detection_settings{
	float confid_thrs; // 3
	float nms_thrs;
};

typedef struct _awf_detected_outputs AWF_Det_Outputs;
struct _awf_detected_outputs{
	int num;
	AWF_Det_Face *faces;
};

typedef struct _aw_yuv_buf AW_Yuv_Buf;
struct _aw_yuv_buf {
	AW_YUV_TYPE fmt;
	unsigned int h;
	unsigned int w;
	unsigned int c;
	unsigned int phy_addr[3];
	unsigned int vir_addr[3];
};

int awf_make_det_container(AWF_Det_Container *container, char *nbg_path, AWF_Det_Outputs *outputs);
int awf_set_det_runtime(AWF_Det_Container *container,  float confid_thrs, float nms_thrs);
void awf_run_det(AWF_Det_Container *container, AW_Img *rgb_img, AWF_Det_Outputs *outputs);
void awf_free_det_container(AWF_Det_Container *det_container,  AWF_Det_Outputs *outputs);

#ifdef __cplusplus
}
#endif

#endif