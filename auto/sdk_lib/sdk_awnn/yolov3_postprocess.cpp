// yolo_v3_post_process.cpp : Defines the entry point for the console application.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <string>
#include <iostream>

#include "yolo_layer.h"
#include <time.h>
#include "yolov3_postprocess.h"
#define DUMP_LOG                0

#define MAX_BOX    100

#if DUMP_LOG
#define BILLION                                 1000000000
static uint64_t get_perf_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)((uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec * BILLION);

}
#endif

using namespace std;


int yolov3_postprocess(float **tensor, Awnn_Result_t *res, int width, int height, int img_w, int img_h, float thresh)
{
   
    vector<blob> blobs;
    for (int i=0; i<2; i++)
    {
        blob temp_blob = {0, 0, NULL};
        //get_tensor_data(ouput_file_path[i], &temp_blob.data);
	    temp_blob.data =tensor[i];
        blobs.push_back(temp_blob);
    }
    blobs[0].w = width / 32;
    blobs[0].h = height / 32;
    blobs[1].w = blobs[0].w * 2;
    blobs[1].h = blobs[0].h * 2;

    int classes = 11;
    if (thresh <= 0) thresh = 0.25;
    float nms = 0.6;
    int nboxes = 0;

#if DUMP_LOG
    uint64_t tmsStart, tmsEnd, msVal, usVal;
    tmsStart = get_perf_count();
#endif

    //printf("postprocess thresh = %f\n", thresh);
    detection *dets = get_detections(blobs, img_w, img_h, width, height, &nboxes, classes, thresh, nms);

    // detect_res_t *get_ret_boxes(void);
    // detect_res_t *res = get_ret_boxes();
    int i,j;
	int boxesnum = 0;

    for(i=0;i< nboxes;++i){
        //char labelstr[4096] = {0};
        int cls = -1;
        for(j=0;j<classes;++j){
            if(dets[i].prob[j] > 0.1){
                if(cls < 0){
                    cls = j;
                    break;
                }
            }
        }

        if(cls != -1)
        {
            int iter = boxesnum ++;
            box b = dets[i].bbox;
            if(iter >= MAX_BOX)
            {
                printf("warning: the detect result num > max capacity\n");
                return iter;
            }
            int left  = (b.x-b.w/2.)*img_w;
            int right = (b.x+b.w/2.)*img_w;
            int top   = (b.y-b.h/2.)*img_h;
            int bot   = (b.y+b.h/2.)*img_h;
            res->boxes[iter].label      = cls;
            res->boxes[iter].score      = dets[i].prob[cls];
            res->boxes[iter].xmin    = left < 0 ? 0 : left;
            res->boxes[iter].ymin    = top < 0 ? 0 : top;
            res->boxes[iter].xmax    = right > img_w ? img_w : right;
            res->boxes[iter].ymax    = bot > img_h ? img_h : bot;
            // printf("============= prob %.2f %d %d %d %d===================\n",dets[i].prob[cls],left,top,right,bot);
        }
        
    }
	res->valid_cnt = boxesnum;

#if DUMP_LOG
    tmsEnd = get_perf_count();
    msVal = (tmsEnd - tmsStart)/1000000;
    usVal = (tmsEnd - tmsStart)/1000;
    printf("postprocess used %.2lldms or %.2lldus\n", msVal, usVal);
#endif

    free_detections(dets, nboxes);

  return boxesnum;
}

