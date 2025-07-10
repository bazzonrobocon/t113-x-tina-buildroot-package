#include <memory.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math.h"

//#include "aw_errors.h"
#include "face_det_postprocess.h"
#include "awf_detection.h"

#define USE_ASYNC_PROCESS       0

#define MAX_IO_NUM              20

#define ONERROR(status, string) \
    if (status != VIP_SUCCESS) { \
        printf("error: %s\n", string); \
        goto error_exit; \
    }

typedef enum _file_type_e
{
    NN_FILE_NONE,
    NN_FILE_TENSOR,
    NN_FILE_BINARY
} file_type_e;

#define _CHECK_STATUS( stat, lbl) do {\
	if( VIP_SUCCESS != stat) {\
		printf("Error: %s, %s at %d\n", __FILE__, __FUNCTION__, __LINE__); \
		goto lbl;\
	}\
} while(0)

#define TIME_SLOTS   10
vip_uint64_t time_begin[TIME_SLOTS];
vip_uint64_t time_end[TIME_SLOTS];
static vip_uint64_t GetTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (vip_uint64_t)(time.tv_usec + time.tv_sec * 1000000);
}

static void TimeBegin(int id)
{
    time_begin[id] = GetTime();
}

static void TimeEnd(int id)
{
    time_end[id] = GetTime();
}

static vip_uint64_t TimeGet(int id)
{
    return time_end[id] - time_begin[id];
}

vip_status_e vip_memset(vip_uint8_t *dst, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
    memset(dst, 0, size);
    return status;
}

vip_status_e vip_memcpy(vip_uint8_t *dst, vip_uint8_t *src, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
    memcpy(dst, src, size);
    return status;
}

unsigned int load_file(const char *name, void *dst)
{
    FILE    *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fseek(fp, 0, SEEK_SET);
        fread(dst, size, 1, fp);

        fclose(fp);
    }
    else {
        printf("Lading file %s failed.\n", name);
    }

    return size;
}

static vip_float_t affine_to_fp32(vip_int32_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    result = ((vip_float_t)val - zeroPoint) * scale;
    return result;
}
unsigned int get_file_size(const char *name)
{
    FILE    *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fclose(fp);
    }
    else {
        printf("Checking file %s failed.\n", name);
    }

    return size;
}

vip_status_e query_hardware_info(void)
{
    vip_uint32_t version = vip_get_version();
    vip_uint32_t device_count = 0;
    vip_uint32_t cid = 0;
    vip_uint32_t *core_count = VIP_NULL;
    vip_uint32_t i = 0;

    if (version >= 0x00010601) {
        vip_query_hardware(VIP_QUERY_HW_PROP_CID, sizeof(vip_uint32_t), &cid);
        vip_query_hardware(VIP_QUERY_HW_PROP_DEVICE_COUNT, sizeof(vip_uint32_t), &device_count);
        core_count = (vip_uint32_t*)malloc(sizeof(vip_uint32_t) * device_count);
        vip_query_hardware(VIP_QUERY_HW_PROP_CORE_COUNT_EACH_DEVICE,
                          sizeof(vip_uint32_t) * device_count, core_count);
        printf("cid=0x%x, device_count=%d\n", cid, device_count);
        for (i = 0; i < device_count; i++) {
            printf("  device[%d] core_count=%d\n", i, core_count[i]);
        }
        free(core_count);
    }
    return VIP_SUCCESS;
}

/* Create the network in the det_container. */
vip_status_e create_network( AWF_Det_Container *det_container, char *file_name)
{
    vip_status_e status = VIP_SUCCESS;
    int file_size = 0;
    int i = 0;
    int input_count = 0;
    vip_buffer_create_params_t param;

    /* Load nbg data. */
    file_size = get_file_size((const char *) file_name);
    if (file_size <= 0) 
    {
        printf("Network binary file %s can't be found.\n", file_name);
        status = VIP_ERROR_INVALID_ARGUMENTS;
        return status;
    }

    TimeBegin(1);

    status = vip_create_network(file_name, 0, VIP_CREATE_NETWORK_FROM_FILE, &det_container->network);

    if (status != VIP_SUCCESS) 
    {
        printf("Network creating failed. Please validate the content of %s.\n", file_name);
        return status;
    }

    /* Create input buffers. */
    vip_query_network(det_container->network, VIP_NETWORK_PROP_INPUT_COUNT, &det_container->input_count);

    det_container->input_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * det_container->input_count);
    for (i = 0; i < det_container->input_count; i++) 
    {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_input(det_container->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_input(det_container->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_input(det_container->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_input(det_container->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_input(det_container->network, i, VIP_BUFFER_PROP_NAME, name);
        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                vip_query_input(det_container->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                &param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                vip_query_input(det_container->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                &param.quant_data.affine.scale);
                vip_query_input(det_container->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                &param.quant_data.affine.zeroPoint);
                break;
            default:
            break;
        }

        printf("input %d dim %d %d %d %d, data_format=%d, quant_format=%d, name=%s",
                i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
                param.data_format, param.quant_format, name);

        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                       param.quant_data.affine.zeroPoint);
                break;
            default:
                printf(", none-quant\n");
        }

        status = vip_create_buffer(&param, sizeof(param), &det_container->input_buffers[i]);
        if (status != VIP_SUCCESS) {
            printf("fail to create input %d buffer, status=%d\n", i, status);
            return status;
        }
    }

    /* Create output buffer. */
    vip_query_network(det_container->network, VIP_NETWORK_PROP_OUTPUT_COUNT, &det_container->output_count);
    det_container->output_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * det_container->output_count);
    for (i = 0; i < det_container->output_count; i++) {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_output(det_container->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_output(det_container->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_output(det_container->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_output(det_container->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_output(det_container->network, i, VIP_BUFFER_PROP_NAME, name);
        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                vip_query_output(det_container->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                 &param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                vip_query_output(det_container->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                 &param.quant_data.affine.scale);
                vip_query_output(det_container->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                 &param.quant_data.affine.zeroPoint);
                break;
            default:
            break;
        }

        printf("ouput %d dim %d %d %d %d, data_format=%d, name=%s",
                i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
                param.data_format, name);

        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                       param.quant_data.affine.zeroPoint);
                break;
            default:
                printf(", none-quant\n");
        }

        status = vip_create_buffer(&param, sizeof(param), &det_container->output_buffers[i]);
         if (status != VIP_SUCCESS) {
             printf("fail to create output %d buffer, status=%d\n", i, status);
            return status;
         }
    }

    TimeEnd(1);
	printf("nbg name=%s\n", file_name);
    printf("create network %d: %lu us.\n", i, (unsigned long)TimeGet(1));
    {
        vip_uint32_t mem_pool_size = 0;
        vip_query_network(det_container->network, VIP_NETWORK_PROP_MEMORY_POOL_SIZE, &mem_pool_size);
        printf("memory pool size=%dbyte\n", mem_pool_size);
    }
    return status;
}

vip_status_e load_input_data(AWF_Det_Container *det_container, void *img_data)
{
    vip_status_e status = VIP_SUCCESS;
    void *data;
    vip_uint32_t buff_size, buff_size1;
    int i = 0;

    data = vip_map_buffer(det_container->input_buffers[i]);
    buff_size = vip_get_buffer_size(det_container->input_buffers[i]);
    vip_memcpy((vip_uint8_t *)data, (vip_uint8_t *)img_data, buff_size);
    vip_unmap_buffer(det_container->input_buffers[i]);

    return status;
}

vip_status_e set_network_input_output(AWF_Det_Container *det_container)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;

    /* Load input buffer data. */
    for (i = 0; i < det_container->input_count; i++) 
    {
        /* Set input. */
        status = vip_set_input(det_container->network, i, det_container->input_buffers[i]);
        if (status != VIP_SUCCESS) {
            printf("fail to set input %d\n", i);
            goto ExitFunc;
        }
    }

    for (i = 0; i < det_container->output_count; i++) 
    {
        if (det_container->output_buffers[i] != VIP_NULL) 
        {
            status = vip_set_output(det_container->network, i, det_container->output_buffers[i]);
            if (status != VIP_SUCCESS) 
            {
                printf("fail to set output\n");
                goto ExitFunc;
            }
        }
        else
         {
            printf("fail output %d is null. output_counts=%d\n", i, det_container->output_count);
            status = VIP_ERROR_FAILURE;
            goto ExitFunc;
        }
    }

ExitFunc:
    return status;
}

typedef union
{
    unsigned int u;
    float f;
} _fp32_t;

static float fp16_to_fp32(const short in)
{
    const _fp32_t magic = { (254 - 15) << 23 };
    const _fp32_t infnan = { (127 + 16) << 23 };
    _fp32_t o;
    // Non-sign bits
    o.u = (in & 0x7fff ) << 13;
    o.f *= magic.f;
    if(o.f  >= infnan.f)
    {
        o.u |= 255 << 23;
    }
    //Sign bit
    o.u |= (in & 0x8000 ) << 16;
    return o.f;
}

void destroy_network(AWF_Det_Container *det_container)
{
    int i = 0;

    if (det_container == VIP_NULL) {
        printf("failed det_container is NULL\n");
        return;
    }

    vip_destroy_network(det_container->network);

    for (i = 0; i < det_container->input_count; i++) {
        vip_destroy_buffer(det_container->input_buffers[i]);
    }
    free(det_container->input_buffers);

    for (i = 0; i < det_container->output_count; i++) {
        vip_destroy_buffer(det_container->output_buffers[i]);
    }
    free(det_container->output_buffers);
    det_container->output_buffers = VIP_NULL;
}

int awf_make_det_container(AWF_Det_Container *det_container, char *nbg_path, AWF_Det_Outputs *outputs)
{
    const char *fun = "awf_make_det_container";
    if(!det_container || !nbg_path || !outputs)
    {
        printf("%s: input[container/nbg_path/rgb_img] is nullptr\n", fun);
        return -1;
    }
    vip_status_e status = VIP_SUCCESS;
    status = vip_init();
    if (status != VIP_SUCCESS) 
    {
        printf("%s: failed to init vip\n", fun);
        return -1;
    }
    
    query_hardware_info();
    vip_uint32_t  version = vip_get_version();
    printf("init vip lite, driver version=0x%08x...\n", version);

    // load nbg file
    status = create_network(det_container, nbg_path);
    if (status != VIP_SUCCESS) {
        printf("%s: faceDet_init failed..\n", fun);
        return -1;
    }

    status = vip_prepare_network(det_container->network);
    if (status != VIP_SUCCESS) {
        printf("%s: fail prpare network, status=%d\n", fun, status);
        return -1;
    }

    outputs->faces = (AWF_Det_Face *)malloc(MAX_DET_NUM * sizeof(AWF_Det_Face));
    for(int i = 0; i < MAX_DET_NUM; i++)
    {
        outputs->faces[i].ldmk.cx = (short *)malloc(NUM_KEYPOINTS * sizeof(short));
        outputs->faces[i].ldmk.cy = (short *)malloc(NUM_KEYPOINTS * sizeof(short));
    }  
    return 0;
}

int awf_set_det_runtime(AWF_Det_Container *container,  float confid_thrs, float nms_thrs)
{
    const char *fun = "awf_set_det_runtime";
    if(!container)
    {
        printf("%s: input[container] is nullptr\n", fun);
        return -1;
    }

    AWF_Det_Settings *settings = (AWF_Det_Settings *)malloc(sizeof(AWF_Det_Settings));
    settings->confid_thrs = confid_thrs;
    settings->nms_thrs = nms_thrs;
    container->settings = (void *)settings;
    return 0;
}

void awf_run_det(AWF_Det_Container *det_container, AW_Img *rgb_img, AWF_Det_Outputs *outputs)
{
    const char *fun = "awf_run_det";
    if(!det_container || !outputs || !rgb_img->data)
    {
        printf("%s: input[container/outputs/rgb_img] is nullptr\n", fun);
    }

    vip_status_e status = VIP_SUCCESS;
    int k = 0;
    int i = 0;
    status = load_input_data(det_container, (void*)rgb_img->data);
    if (status != VIP_SUCCESS) 
    {
        printf("%s: fail to load_input_data\n", fun);
    }

    status = set_network_input_output(det_container);
    if (status != VIP_SUCCESS) 
    {
        printf("%s: fail to set_network_input_output\n", fun);
    }
    
     /* it is only necessary to call vip_flush_buffer() after set vpmdENABLE_FLUSH_CPU_CACHE to 2 */
    for (k = 0; k < det_container->input_count; k++) 
    {
        if ((vip_flush_buffer(det_container->input_buffers[k], VIP_BUFFER_OPER_TYPE_FLUSH)) != VIP_SUCCESS)
         {
            printf("%s: flush input%d cache failed.\n", fun, k);
        }
    }

    status = vip_run_network(det_container->network);
    if (status != VIP_SUCCESS) 
    {
        printf("%s: fail to run network, status=%d\n", fun, status);
    }

    for (k = 0; k < det_container->output_count; k++) 
    {
        if ((vip_flush_buffer(det_container->output_buffers[k], VIP_BUFFER_OPER_TYPE_INVALIDATE)) != VIP_SUCCESS)
        {
            printf("%s: flush output %d cache failed.\n", fun, k);
        }
    }

    float **output_tensor = (float **)malloc(sizeof(float *) * det_container->output_count);
    int buff_size = 0;
    void *out_data = NULL;
    vip_buffer_create_params_t buf_param;
    for (i = 0; i < det_container->output_count; i++)
    {
        buff_size = vip_get_buffer_size(det_container->output_buffers[i]);
        if (buff_size <= 0)
        {
            printf("%s: flush buff_size %d cache failed.\n", fun, buff_size);
        }
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_output(det_container->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &buf_param.data_format);
        status = vip_query_output(det_container->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &buf_param.num_of_dims);
        status = vip_query_output(det_container->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, buf_param.sizes);
        status = vip_query_output(det_container->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT, &buf_param.quant_data.affine.zeroPoint);
        status = vip_query_output(det_container->network, i, VIP_BUFFER_PROP_TF_SCALE, &buf_param.quant_data.affine.scale);

        out_data = vip_map_buffer(det_container->output_buffers[i]);

        vip_int32_t zeroPoint = buf_param.quant_data.affine.zeroPoint;
        vip_float_t scale = buf_param.quant_data.affine.scale;
        vip_uint8_t *data = (vip_uint8_t*)out_data;
        unsigned int element = 1;
        for (k = 0; k < buf_param.num_of_dims; k++) {
            element *= buf_param.sizes[k];
        }
        output_tensor[i] = (float *)malloc(element * sizeof(float));        

        for (int j = 0; j < element; j++) {
            output_tensor[i][j] = fp16_to_fp32(*((short *)data));
            data +=2; 
        }       
    }

    AWF_Det_Settings *setting = (AWF_Det_Settings *)det_container->settings;
    postprocess_face_det(output_tensor, rgb_img->w, rgb_img->h, setting->confid_thrs, setting->nms_thrs, outputs);

    for (i = 0; i < det_container->output_count; i++)
    {
        free(output_tensor[i]);
    }
    free(output_tensor);
}

void awf_free_det_container(AWF_Det_Container *det_container,  AWF_Det_Outputs *outputs)
{
    const char *fun = "awf_free_det_container";
    vip_status_e status = VIP_SUCCESS;
    if (det_container != VIP_NULL) {
        vip_finish_network(det_container->network);
        destroy_network(det_container);
    }

    status = vip_destroy();
    if (status != VIP_SUCCESS) {
        printf("%s: fail to destory vip\n", fun);
    }

    AWF_Det_Settings *setting =  (AWF_Det_Settings *)det_container->settings;
    free(setting);
    setting = NULL;

    if(outputs)
    {
        for(int i = 0; i  < MAX_DET_NUM; i++)
        {
            free(outputs->faces[i].ldmk.cx); 
            free(outputs->faces[i].ldmk.cy);

        }
        free(outputs->faces);
    }
}
