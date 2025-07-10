#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/time.h>

#include "awf_detection.h"

int main(int argc, char* argv[])
{
    char *nbg_path = argv[1];
    char *file_name = argv[2];

    printf("Read the image....\n");
    AW_Img rgb_img = aw_load_image_stb(file_name, 3);
    int dst_w = 480;
    int dst_h = 480;

    AW_Img resized_img = aw_resize_image(&rgb_img, dst_w, dst_h);
    printf("\nPartial img ...\n");
    for(int i = 0; i < 60; i++)
    {
        printf("%10d", resized_img.data[i]);
        if( (i+1) % 6 == 0)
        {
            printf("\n");
        }
    }
    printf("Finish!\n");

    printf("Init....\n");
    AWF_Det_Container *container = ( AWF_Det_Container  *)malloc(sizeof(AWF_Det_Container));
    AWF_Det_Outputs  *outputs =(AWF_Det_Outputs *)malloc(sizeof(AWF_Det_Container));
    
    awf_make_det_container(container,  nbg_path,  outputs);
    float confid_thrs = 0.3;
    float nms_thrs = 0.45;
    awf_set_det_runtime(container,  confid_thrs,  nms_thrs);
    printf("Finish!\n");

    printf("Run....\n");
    awf_run_det(container, &resized_img, outputs);
    float scale_w = rgb_img.w * 1.0 / resized_img.w;
    float scale_h = rgb_img.h * 1.0 / resized_img.h;
    for (int i = 0; i < outputs->num; i++)
	{
        AW_Box face_box = outputs->faces[i].bbox;
        AW_LDMK face_ldmk = outputs->faces[i].ldmk;
        face_box.tl_x = (int)(outputs->faces[i].bbox.tl_x * scale_w + 0.5);
        face_box.tl_y = (int)(outputs->faces[i].bbox.tl_y * scale_h + 0.5);
        face_box.br_x = (int)(outputs->faces[i].bbox.br_x * scale_w + 0.5);
        face_box.br_y = (int)(outputs->faces[i].bbox.br_y * scale_h + 0.5);
        
        face_box.width = face_box.br_x - face_box.tl_x;
        face_box.height = face_box.br_y - face_box.tl_y;
        face_box.area = face_box.width * face_box.height;

		for (int j = 0; j < NUM_KEYPOINTS; j++)
		{
            face_ldmk.cx[j] = (int)(outputs->faces[i].ldmk.cx[j] * scale_w + 0.5);
			face_ldmk.cy[j] = (int)(outputs->faces[i].ldmk.cy[j] * scale_h + 0.5);
		}
		printf("score: %f\n", outputs->faces[i].confid_score);
		printf("xmin/ymin/xmax/ymax/w/h: %d %d %d %d %d %d.\n", face_box.tl_x, face_box.tl_y, face_box.br_x,
			face_box.br_y, face_box.width, face_box.height);
		printf("Ldmk:\n");
		for (int j = 0; j < NUM_KEYPOINTS; j++)
		{
			printf("%d %d.\n", face_ldmk.cx[j], face_ldmk.cy[j]);
		}
	}
    printf("Finish!\n");
    
    printf("Free....\n");
    awf_free_det_container(container, outputs);
    free(container);
    free(outputs);
    aw_free_image(&rgb_img);
    aw_free_image(&resized_img);
    printf("Finish!\n");
}
