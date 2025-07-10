#ifndef _YOLOV3_POSTPROCESS_H_
#define _YOLOV3_POSTPROCESS_H_

#ifdef __cplusplus
       extern "C" {
#endif

typedef struct
{
    int xmin; // 左侧坐标
    int ymin; // 顶部坐标
    int xmax; // 右侧坐标
    int ymax; // 底部坐标
    int label; // 标签：人形（脸）:0，非人形（脸）>0。
    float score; // 概率值
    int landmark_x[5]; // 五官坐标x，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
    int landmark_y[5]; // 五官坐标y，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
} BBoxRect_t;

typedef struct
{
    int valid_cnt;                  // 检测结果数量
    BBoxRect_t boxes[100];          // 检测结果目标信息
} Awnn_Result_t;

int yolov3_postprocess(float **tensor, Awnn_Result_t *res, int width, int height, int img_w, int img_h, float thresh);

#ifdef __cplusplus
}
#endif

#endif