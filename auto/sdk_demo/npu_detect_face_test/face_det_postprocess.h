#ifndef _FACE_DET_POSTPROCESS_H_
#define _FACE_DET_POSTPROCESS_H_

#include "awf_detection.h"
void postprocess_face_det(float **inputs,int width, int height, float confid_thresh,  float nms_thresh, AWF_Det_Outputs *outputs);



#endif