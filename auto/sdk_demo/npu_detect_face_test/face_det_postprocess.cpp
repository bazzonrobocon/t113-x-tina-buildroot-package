#include <stdio.h>
#include <stdlib.h>
#include<math.h>
#include <vector>
#include <algorithm>

#include "face_det_postprocess.h"

#define NUM_KEYPOINTS 5

typedef struct
{
	float tl_x;  // top left
	float tl_y;  // top left
	float br_x;   // bottom right
	float br_y;   // top right
	float area;
}BBox;

typedef struct 
{
	float x;
	float y;
	float prob;
}LDMKPoint;

typedef struct
{
	BBox bbox;
	float score;
	LDMKPoint landmark[NUM_KEYPOINTS];
} FaceObject;

static inline float intersection_area(const BBox &a, const BBox &b)
{
	float a_xmax = a.br_x;
	float b_xmax = b.br_x;
	float a_ymax = a.br_y;
	float b_ymax = b.br_y;
	if (a.tl_x > b_xmax || a_xmax < b.tl_x || a.tl_y > b_ymax || a_ymax < b.tl_y)
	{
		// no intersection
		return 0.f;
	}

	float inter_width = std::min(a_xmax, b_xmax) - std::max(a.tl_x, b.tl_x);
	float inter_height = std::min(a_ymax, b_ymax) - std::max(a.tl_y, b.tl_y);

	return inter_width * inter_height;
}

static inline float sigmoid(float x)
{
	return static_cast<float>(1.f / (1.f + exp(-x)));
}


void generate_proposals(float *inputs, std::vector<int> anchor, int input_w, int input_h, 
	int stride, float thresh,
	std::vector<FaceObject>& proposals)
{
	int channels = 21; 
	int feat_w = int(input_w / stride);
	int feat_h = int(input_h / stride);
	int feat_sz = feat_h * feat_w;
	for (int c = 0; c < anchor.size() / 2; c++)
	{
		float anchor_w = float(anchor[c * 2 + 0]);
		float anchor_h = float(anchor[c * 2 + 1]);
		int c_index = c * channels;
		float *ptr_x = inputs + feat_sz * (c_index + 0);
		float *ptr_y = inputs + feat_sz * (c_index + 1);
		float *ptr_w = inputs + feat_sz * (c_index + 2);
		float *ptr_h = inputs + feat_sz * (c_index + 3);
		float *ptr_s = inputs + feat_sz * (c_index + 4);
		float *ptr_c = inputs + feat_sz * (c_index + 5);

		for (int i = 0; i < feat_h; i++)
		{
			for (int j = 0; j < feat_w; j++)
			{
				int index = i * feat_w + j;
				float bbox_score = sigmoid(ptr_s[index]);
				if (bbox_score > thresh)
				{
					float confidence = sigmoid(ptr_s[index]) * sigmoid(ptr_c[index]);
					if (confidence > thresh)
					{
						FaceObject face;
						float dx = sigmoid(ptr_x[index]);
						float dy = sigmoid(ptr_y[index]);
						float dw = sigmoid(ptr_w[index]);
						float dh = sigmoid(ptr_h[index]);

						float pb_cx = (dx * 2.f - 0.5f + j) * stride;
						float pb_cy = (dy * 2.f - 0.5f + i) * stride;

						float pb_w = pow(dw * 2.f, 2) * anchor_w;
						float pb_h = pow(dh * 2.f, 2) * anchor_h;

						face.score = confidence;
						face.bbox.tl_x = pb_cx - pb_w * 0.5f;
						face.bbox.tl_y = pb_cy - pb_h * 0.5f;
						face.bbox.br_x = pb_cx + pb_w * 0.5f;
						face.bbox.br_y = pb_cy + pb_h * 0.5f;
						face.bbox.area = pb_h * pb_w;

						for (int l = 0; l < 5; l++)
						{
							float ldmk_x = inputs[feat_sz*(c_index + l * 3 + 6) + index];
							face.landmark[l].x = (ldmk_x * 2 - 0.5 + j) * stride;
							float ldmk_y = inputs[feat_sz*(c_index + l * 3 + 7) + index];
							face.landmark[l].y = (ldmk_y * 2 - 0.5 + i) * stride;
							float prob = inputs[feat_sz*(c_index + l * 3 + 8) + index];
							face.landmark[l].prob = prob;
						}
						proposals.push_back(face);
					}

				}
			}
		}
	}
}

static void nms_sorted_bboxes(const std::vector<FaceObject>& faceobjects, std::vector<int>& picked, float nms_threshold, bool agnostic = false)
{
	picked.clear();

	const int n = faceobjects.size();

	std::vector<float> areas(n);
	for (int i = 0; i < n; i++)
	{
		areas[i] = faceobjects[i].bbox.area;
	}

	for (int i = 0; i < n; i++)
	{
		const FaceObject& a = faceobjects[i];

		int keep = 1;
		for (int j = 0; j < (int)picked.size(); j++)
		{
			const FaceObject& b = faceobjects[picked[j]];

			/*if (!agnostic && a.label != b.label)
				continue;*/

			// intersection over union
			float inter_area = intersection_area(a.bbox, b.bbox);
			float union_area = areas[i] + areas[picked[j]] - inter_area;
			if (inter_area / union_area > nms_threshold)
				keep = 0;
		}
		if (keep)
			picked.push_back(i);
	}
}

static void qsort_descent_inplace(std::vector<FaceObject> &objects, int left, int right)
{
	int i = left;
	int j = right;
	float p = objects[(left + right) / 2].score;

	while (i <= j)
	{
		while (objects[i].score > p)
			i++;

		while (objects[j].score < p)
			j--;

		if (i <= j)
		{
			// swap
			std::swap(objects[i], objects[j]);

			i++;
			j--;
		}
	}

#pragma omp parallel sections
	{
#pragma omp section
	{
		if (left < j) qsort_descent_inplace(objects, left, j);
	}
#pragma omp section
	{
		if (i < right) qsort_descent_inplace(objects, i, right);
	}
	}
}

static void qsort_descent_inplace(std::vector<FaceObject> &objects)
{
	if (objects.empty())
		return;

	qsort_descent_inplace(objects, 0, objects.size() - 1);
}

void postprocess_face_det(float **inputs, int width, int height, float confid_thresh, float nms_thresh,
	AWF_Det_Outputs *outputs)
{
	std::vector<FaceObject> face_objects;
	// stride = 8
	{
		std::vector<int> anchor0 = { 4,5,  6,8,  10,12 };
		generate_proposals(inputs[0], anchor0, width, height, 8, confid_thresh, face_objects);
	}

	// stride = 16
	{
		std::vector<int> anchor1 = { 15,19,  23,30,  39,52 };
		generate_proposals(inputs[1], anchor1, width, height, 16, confid_thresh, face_objects);
	}

	// stride = 32
	{
		std::vector<int> anchor2 = { 72,97,  123,164,  209,297 };
		generate_proposals(inputs[2], anchor2, width, height, 32, confid_thresh, face_objects);
	}

	// sort all proposals by score from highest to lowest
	qsort_descent_inplace(face_objects);

	std::vector<int> picked;
	nms_sorted_bboxes(face_objects, picked, nms_thresh);

	outputs->num = picked.size();
	for (int i = 0; i < outputs->num; i++)
	{
		outputs->faces[i].bbox.tl_x = int(face_objects[picked[i]].bbox.tl_x + 0.5);
		outputs->faces[i].bbox.tl_y = int(face_objects[picked[i]].bbox.tl_y + 0.5);
		outputs->faces[i].bbox.br_x = int(face_objects[picked[i]].bbox.br_x  + 0.5);
		outputs->faces[i].bbox.br_y = int(face_objects[picked[i]].bbox.br_y  + 0.5);
		outputs->faces[i].bbox.width = outputs->faces[i].bbox.br_x - outputs->faces[i].bbox.tl_x;
		outputs->faces[i].bbox.height = outputs->faces[i].bbox.br_y - outputs->faces[i].bbox.tl_y;
		outputs->faces[i].confid_score = face_objects[picked[i]].score;

		for (int j = 0; j < NUM_KEYPOINTS; j++)
		{
			outputs->faces[i].ldmk.cx[j] = int(face_objects[picked[i]].landmark[j].x + 0.5);
			outputs->faces[i].ldmk.cy[j] = int(face_objects[picked[i]].landmark[j].y  + 0.5);
		}
	}
}