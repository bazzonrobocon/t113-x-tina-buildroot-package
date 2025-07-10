#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <awnn_lib.h>
#include "yolov3_postprocess.h"

#define MODEL_WIDTH 640
#define MODEL_HEIGHT 384
#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 384

void *load_file(const char *fn, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;

    data = 0;
    fd = open(fn, O_RDONLY);
    if(fd < 0) return 0;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz + 1);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);
    data[sz] = 0;

    if(_sz) *_sz = sz;
    return data;

oops:
    close(fd);
    if(data != 0) free(data);
    return 0;
}

int main(int argc, char **argv) {
    printf("%s nbg input\n", argv[0]);
    if(argc < 3)
    {
        printf("Arguments count %d is incorrect!\n", argc);
        return -1;
    }
    const char* nbg = argv[1];
    const char* input = argv[2];

    // npu init
    awnn_init();
    // create network
    Awnn_Context_t *context = awnn_create(nbg);
    // copy input
    unsigned int size;
    unsigned char *data = (unsigned char *)load_file(input, &size);
    unsigned char *input_buffers[2] = {data, data + IMAGE_WIDTH * IMAGE_HEIGHT};
    awnn_set_input_buffers(context, input_buffers);
    // process network
    awnn_run(context);
    // get result
    float **results = awnn_get_output_buffers(context);
    Awnn_Result_t res;
    // post process
    yolov3_postprocess(results, &res, MODEL_WIDTH, MODEL_HEIGHT, IMAGE_WIDTH, IMAGE_HEIGHT, 0.35);
    printf("Num: %d\n", res.valid_cnt);
    for (int i = 0; i < res.valid_cnt; i++) {
	printf("%d: cls %d, prob %f, rect [%d, %d, %d, %d]\n", i, res.boxes[i].label, res.boxes[i].score, res.boxes[i].xmin, res.boxes[i].ymin, res.boxes[i].xmax, res.boxes[i].ymax);
    }

    free(data);
    // destroy network
    awnn_destroy(context);
    // npu uninit
    awnn_uninit();

    return 0;
}

