#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include "hwdisp2.h"

#define LOG_TAG "hwdisplay2"
#include "sdklog.h"

#define sync_layer_cfg(dst_pcfg, src_cfg)  do                               \
{                                                                           \
    dst_pcfg->channel  = src_cfg->channel;                                  \
    dst_pcfg->layer_id = src_cfg->layer_id;                                 \
    dst_pcfg->enable   = src_cfg->enable;                                   \
                                                                            \
                                                                            \
    dst_pcfg->info.mode         = src_cfg->info.mode;                       \
    dst_pcfg->info.zorder       = src_cfg->info.zorder;                     \
    dst_pcfg->info.alpha_mode   = src_cfg->info.alpha_mode;                 \
    dst_pcfg->info.alpha_value  = src_cfg->info.alpha_value;                \
                                                                            \
    dst_pcfg->info.screen_win.x      = src_cfg->info.screen_win.x;          \
    dst_pcfg->info.screen_win.y      = src_cfg->info.screen_win.y;          \
    dst_pcfg->info.screen_win.width  = src_cfg->info.screen_win.width;      \
    dst_pcfg->info.screen_win.height = src_cfg->info.screen_win.height;     \
                                                                            \
    dst_pcfg->info.b_trd_out    = src_cfg->info.b_trd_out;                  \
    dst_pcfg->info.out_trd_mode = src_cfg->info.out_trd_mode;               \
    dst_pcfg->info.id           = src_cfg->info.id;                         \
                                                                            \
    dst_pcfg->info.fb.size[0].width  = src_cfg->info.fb.size[0].width ;     \
    dst_pcfg->info.fb.size[0].height = src_cfg->info.fb.size[0].height;     \
    dst_pcfg->info.fb.size[1].width  = src_cfg->info.fb.size[1].width ;     \
    dst_pcfg->info.fb.size[1].height = src_cfg->info.fb.size[1].height;     \
    dst_pcfg->info.fb.size[2].width  = src_cfg->info.fb.size[2].width ;     \
    dst_pcfg->info.fb.size[2].height = src_cfg->info.fb.size[2].height;     \
                                                                            \
    dst_pcfg->info.fb.align[0]  = src_cfg->info.fb.align[0];                \
    dst_pcfg->info.fb.align[1]  = src_cfg->info.fb.align[1];                \
    dst_pcfg->info.fb.align[2]  = src_cfg->info.fb.align[2];                \
                                                                            \
    dst_pcfg->info.fb.format        = src_cfg->info.fb.format;              \
    dst_pcfg->info.fb.color_space   = src_cfg->info.fb.color_space;         \
    dst_pcfg->info.fb.pre_multiply  = src_cfg->info.fb.pre_multiply;        \
                                                                            \
    dst_pcfg->info.fb.crop.x      = src_cfg->info.fb.crop.x;                \
    dst_pcfg->info.fb.crop.y      = src_cfg->info.fb.crop.y;                \
    dst_pcfg->info.fb.crop.width  = src_cfg->info.fb.crop.width;            \
    dst_pcfg->info.fb.crop.height = src_cfg->info.fb.crop.height;           \
                                                                            \
    dst_pcfg->info.fb.flags  = src_cfg->info.fb.flags;                      \
    dst_pcfg->info.fb.scan  = src_cfg->info.fb.scan;                        \
                                                                            \
} while(0)


namespace android
{

pthread_mutex_t HwDisplay::sLock = PTHREAD_MUTEX_INITIALIZER;
bool HwDisplay::mInitialized = false;
HwDisplay* HwDisplay::sHwDisplay = NULL;

HwDisplay* HwDisplay::getInstance()
{
    if (!mInitialized)
        ALOGD("libsdk_disp v1.version:%s", API1_MODULE_VERSION);

    if (sHwDisplay == NULL) {
        pthread_mutex_lock(&sLock);
        if (sHwDisplay == NULL) {
            sHwDisplay = new HwDisplay;
        }
        pthread_mutex_unlock(&sLock);
    }
    return sHwDisplay;
}

HwDisplay::HwDisplay()
{
    mDisp_fd = -1;
    cur_screen1_mode = -1;

    // init layer_cfg's layer_id and channel_id
    int sc, ch, lyr;
    for (sc = 0; sc < SCREEN_NUM; sc++) {
        for (ch = 0; ch < CHANNEL_NUM; ch++) {
            for (lyr = 0; lyr < LAYER_NUM; lyr++) {
                layer_cfg[sc][ch][lyr].layer_id = lyr;
                layer_cfg[sc][ch][lyr].channel = ch;
                layer_cfg[sc][ch][lyr].enable = 0;
                layer_stat[sc][ch][lyr] = 0;
            }
        }
    }
    hwd_init();
}

HwDisplay::~HwDisplay()
{
    if (mInitialized) {
        hwd_exit();
        mInitialized = false;
    }
}

int HwDisplay::hwd_exit(void)
{
    int ret = RET_OK;
    if (mDisp_fd > 0) {
        close(mDisp_fd);
    }
    mDisp_fd = -1;
    lcdxres = 0;
    lcdyres = 0;
    return ret;
}

int HwDisplay::hwd_init(void)
{
    int ret = RET_OK;
    int fb0_fd;
    struct fb_var_screeninfo var0;
    // struct fb_fix_screeninfo fix0;

    if (mInitialized) {
        return RET_FAIL;
    }

    mDisp_fd = open(DISP_DEV, O_RDWR);
    if (mDisp_fd < 0) {
        ALOGE("Failed to open disp device, ret:%d, errno:%d", mDisp_fd, errno);
        return  RET_FAIL;
    }

    mInitialized = true;

    if ((fb0_fd = open("/dev/fb0", O_RDWR)) == -1) {
        ALOGE("open file /dev/fb0 fail.");
        return 0;
    }

    // ioctl(fb0_fd, FBIOGET_FSCREENINFO, &fix0);
    // ALOGD("disp fb0 smem =%p",fix0.smem_start);

    if (ioctl(fb0_fd, FBIOGET_VSCREENINFO, &var0)) {
        ALOGE("Error reading variable information/n");
    }
    lcdxres = var0.xres;
    lcdyres = var0.yres;

    close(fb0_fd);
    return ret;
}

int HwDisplay::aut_hwd_layer_request(struct view_info* surface, unsigned int screen_id, unsigned int channel, unsigned int layer_id)
{
    if ((screen_id > SCREEN_NUM) || (channel > CHANNEL_NUM) || (layer_id > LAYER_NUM)) {
        ALOGE("aut_hwd_layer_request failed, screen_id=%d channel=%d layer_id=%d",
              screen_id, channel, layer_id);
        return -1;
    }

    disp_layer_config2* pcfg = &layer_cfg[screen_id][channel][layer_id];
    int stat = layer_stat[screen_id][channel][layer_id];
    if (stat == 1) {
        return COMP_LAYER_HDL(screen_id, channel, layer_id);
    }

    unsigned long arg[3];
    int ret = 0;

    memset(pcfg, 0, sizeof(disp_layer_config2));

    pcfg->channel = channel;
    pcfg->layer_id = layer_id;
    arg[0] = screen_id;
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1; //one layer
    ret = ioctl(mDisp_fd, DISP_LAYER_GET_CONFIG2, (void*)arg);
    if (ret < 0) {
        ALOGE("DISP_LAYER_GET_CONFIG2 fail");
    }

    //default
    if (pcfg->enable != 1) {
        pcfg->info.zorder = 0;
        pcfg->info.alpha_mode = 1; //global alpha
        pcfg->info.alpha_value = 0xff;
        pcfg->info.fb.format = DISP_FORMAT_YUV420_SP_VUVU;
        pcfg->info.mode = LAYER_MODE_BUFFER;

        pcfg->info.screen_win.x = surface->x;
        pcfg->info.screen_win.y = surface->y;
        pcfg->info.screen_win.width = surface->w;
        pcfg->info.screen_win.height = surface->h;

        if (pcfg->info.fb.color_space == 0) {
            pcfg->info.fb.color_space =
                (surface->h < 720) ? DISP_BT601 : DISP_BT709;
        }
        pcfg->info.atw.cof_fd = -1;
        pcfg->info.fb.align[0] = 4;
    }

    layer_stat[screen_id][channel][layer_id] = 1; //done

    return COMP_LAYER_HDL(screen_id, channel, layer_id);
}

int HwDisplay::aut_hwd_layer_release(unsigned int layer_hdl)
{
    if (layer_stat[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)] == 0) {
        return -1;
    }

    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        aut_hwd_layer_close(layer_hdl);
    }

    layer_stat[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)] = 0;

    return 0;
}

int HwDisplay::aut_hwd_layer_open(unsigned int layer_hdl)
{
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
    unsigned long arg[4];
    int ret = RET_OK;
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);

    arg[0] = HDL_2_SCREENID(layer_hdl);
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1; // layer num

    //ret = ioctl(mDisp_fd, DISP_LAYER_GET_CONFIG2, (void*)arg);
    if (!pcfg->enable) {
        pcfg->enable = 1;

        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_layer_open fail to set layer config");
        } else {
            ALOGD("success to open the layer");
        }
    }

    // ALOGD("open sc=%d ch=%d layer=%d",HDL_2_SCREENID(layer_hdl),HDL_2_CHANNELID(layer_hdl),HDL_2_LAYERID(layer_hdl));
    // ALOGD("open pcfgsc=%d pcfgch=%d pcfglayer=%d",HDL_2_SCREENID(layer_hdl),pcfg->channel,pcfg->layer_id);
    return ret;
}

int HwDisplay::aut_hwd_layer_close(unsigned int layer_hdl)
{
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
    unsigned long arg[4];

    pcfg->enable = 0;
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    arg[0] = HDL_2_SCREENID(layer_hdl);
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1; // layer num
    ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
    if (ret != 0) {
        ALOGE("aut_hwd_layer_close fail to set layer config");
        pcfg->enable = 1;
    }
    return ret;
}

int HwDisplay::aut_hwd_layer_set_src(unsigned int layer_hdl, struct src_info* src, unsigned long fb_addr, int fb_share_fd)
{
    unsigned long arg[4];
    int width = src->w;
    int height = src->h;
    int ret = RET_OK;

    if (fb_share_fd != -1) {
        disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
        pcfg->info.fb.fd = fb_share_fd;

        switch (src->format) {
            case DISP_FORMAT_YUV444_P:
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width;
                pcfg->info.fb.size[1].height = height;
                pcfg->info.fb.size[2].width = width;
                pcfg->info.fb.size[2].height = height;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_YUV420_SP_VUVU:
            case DISP_FORMAT_YUV420_SP_UVUV:
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width / 2;
                pcfg->info.fb.size[1].height = height / 2;
                pcfg->info.fb.size[2].width = width / 2;
                pcfg->info.fb.size[2].height = height / 2;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_YUV422_I_YVYU:
            case DISP_FORMAT_YUV422_I_YUYV:
            case DISP_FORMAT_YUV422_I_UYVY:
            case DISP_FORMAT_YUV422_I_VYUY:
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_YUV420_P:
                pcfg->info.fb.format  = DISP_FORMAT_YUV420_P;
                pcfg->info.fb.size[0].width  = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width  = width  / 2;
                pcfg->info.fb.size[1].height = height / 2;
                pcfg->info.fb.size[2].width  = width  / 2;
                pcfg->info.fb.size[2].height = height / 2;
                pcfg->info.fb.align[1] = 0;
                pcfg->info.fb.align[2] = 0;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_ARGB_8888 :
            case DISP_FORMAT_ABGR_8888 :
            case DISP_FORMAT_RGBA_8888 :
            case DISP_FORMAT_BGRA_8888 :
            case DISP_FORMAT_XRGB_8888 :
            case DISP_FORMAT_XBGR_8888 :
            case DISP_FORMAT_RGBX_8888 :
            case DISP_FORMAT_BGRX_8888 :
            case DISP_FORMAT_RGB_888 :
            case DISP_FORMAT_BGR_888 :
            case DISP_FORMAT_RGB_565 :
            case DISP_FORMAT_BGR_565 :
            case DISP_FORMAT_ARGB_4444 :
            case DISP_FORMAT_ABGR_4444 :
            case DISP_FORMAT_RGBA_4444 :
            case DISP_FORMAT_BGRA_4444 :
            case DISP_FORMAT_ARGB_1555 :
            case DISP_FORMAT_ABGR_1555 :
            case DISP_FORMAT_RGBA_5551 :
            case DISP_FORMAT_BGRA_5551 :
                pcfg->info.fb.format  = (disp_pixel_format)src->format;
                pcfg->info.fb.flags = DISP_BF_NORMAL;
                pcfg->info.fb.scan = DISP_SCAN_PROGRESSIVE;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width;
                pcfg->info.fb.size[1].height = height;
                pcfg->info.fb.size[2].width = width;
                pcfg->info.fb.size[2].height = height;
                pcfg->info.fb.color_space = DISP_BT601;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            default:
                ALOGD("aut_hwd_layer_set_src format:%d don't support", src->format);
                return -1;
        }
        pcfg->channel = HDL_2_CHANNELID(layer_hdl);
        pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
        if (pcfg->enable == 1) {
            arg[0] = HDL_2_SCREENID(layer_hdl);
            arg[1] = (unsigned long)pcfg;
            arg[2] = 1; // layer num
            ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
            if (ret != 0) {
                ALOGE("aut_hwd_layer_set_src fail to set layer config");
            }
        }
    } else {
        if ((unsigned long )fb_addr == 0) {
            ALOGE("aut_hwd_layer_set_src error,both fd and addr are NULL.");
            return -1;
        }
        disp_layer_config2* pcfg2 = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
        disp_layer_config cfg;
        disp_layer_config* pcfg = &cfg;
        sync_layer_cfg(pcfg, pcfg2);


        //ALOGD("set disp addr:%lx",(unsigned long long)(*(unsigned long *)fb_addr));

        //ALOGD("enable=%d,channel=%d,layer_id=%d,zorder=%d",cfg.enable,cfg.channel,cfg.layer_id,cfg.info.zorder);
        //ALOGD("screen[%dx%d],corp[%dx%d],fmt=%x",cfg.info.screen_win.width,cfg.info.screen_win.height,
        //    cfg.info.fb.crop.width,cfg.info.fb.crop.width,cfg.info.fb.format);

        switch (src->format) {
            case DISP_FORMAT_YUV444_P :
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width;
                pcfg->info.fb.size[1].height = height;
                pcfg->info.fb.size[2].width = width;
                pcfg->info.fb.size[2].height = height;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }

                pcfg->info.fb.addr[0] = (unsigned long long)(*(unsigned long*)fb_addr);
                pcfg->info.fb.addr[1] = (unsigned long long)((*(unsigned long*)fb_addr) + width * height);
                pcfg->info.fb.addr[2] = (unsigned long long)((*(unsigned long*)fb_addr) + width * height * 2);;
                break;
            case DISP_FORMAT_YUV420_SP_VUVU :
            case DISP_FORMAT_YUV420_SP_UVUV :
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width / 2;
                pcfg->info.fb.size[1].height = height / 2;
                pcfg->info.fb.size[2].width = width / 2;
                pcfg->info.fb.size[2].height = height / 2;
                pcfg->info.fb.addr[0] = (unsigned long long)(*(unsigned long*)fb_addr);
                pcfg->info.fb.addr[1] = (unsigned long long)((*(unsigned long*)fb_addr) + width * height);
                pcfg->info.fb.addr[2] = (unsigned long long)((*(unsigned long*)fb_addr) + width * height * 5 / 4);

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_YUV422_I_YVYU :
            case DISP_FORMAT_YUV422_I_YUYV :
            case DISP_FORMAT_YUV422_I_UYVY :
            case DISP_FORMAT_YUV422_I_VYUY :
                pcfg->info.fb.format = (disp_pixel_format)src->format;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                pcfg->info.fb.addr[0] = (unsigned long long)(*(unsigned long*)fb_addr);
                break;
            case DISP_FORMAT_YUV420_P :
                pcfg->info.fb.format  = DISP_FORMAT_YUV420_P;
                pcfg->info.fb.addr[0] = (unsigned long)fb_addr;
                pcfg->info.fb.addr[1] = (unsigned long)fb_addr + (width * height) + (width * height / 4);
                pcfg->info.fb.addr[2] = (unsigned long)fb_addr + (width * height);
                pcfg->info.fb.size[0].width  = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width  = width  / 2;
                pcfg->info.fb.size[1].height = height / 2;
                pcfg->info.fb.size[2].width  = width  / 2;
                pcfg->info.fb.size[2].height = height / 2;
                pcfg->info.fb.align[1] = 0;
                pcfg->info.fb.align[2] = 0;

                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            case DISP_FORMAT_ARGB_8888 :
            case DISP_FORMAT_ABGR_8888 :
            case DISP_FORMAT_RGBA_8888 :
            case DISP_FORMAT_BGRA_8888 :
            case DISP_FORMAT_XRGB_8888 :
            case DISP_FORMAT_XBGR_8888 :
            case DISP_FORMAT_RGBX_8888 :
            case DISP_FORMAT_BGRX_8888 :
            case DISP_FORMAT_RGB_888 :
            case DISP_FORMAT_BGR_888 :
            case DISP_FORMAT_RGB_565 :
            case DISP_FORMAT_BGR_565 :
            case DISP_FORMAT_ARGB_4444 :
            case DISP_FORMAT_ABGR_4444 :
            case DISP_FORMAT_RGBA_4444 :
            case DISP_FORMAT_BGRA_4444 :
            case DISP_FORMAT_ARGB_1555 :
            case DISP_FORMAT_ABGR_1555 :
            case DISP_FORMAT_RGBA_5551 :
            case DISP_FORMAT_BGRA_5551 :
                pcfg->info.fb.format  = (disp_pixel_format)src->format;
                pcfg->info.fb.addr[0] = (unsigned long)fb_addr;
                pcfg->info.fb.addr[1] = 0;
                pcfg->info.fb.addr[2] = 0;
                pcfg->info.fb.flags = DISP_BF_NORMAL;
                pcfg->info.fb.scan = DISP_SCAN_PROGRESSIVE;
                pcfg->info.fb.size[0].width = width;
                pcfg->info.fb.size[0].height = height;
                pcfg->info.fb.size[1].width = width;
                pcfg->info.fb.size[1].height = height;
                pcfg->info.fb.size[2].width = width;
                pcfg->info.fb.size[2].height = height;
                pcfg->info.fb.color_space = DISP_BT601;
                if ((pcfg->info.fb.crop.width == 0) || (pcfg->info.fb.crop.width == 0)) {
                    pcfg->info.fb.crop.width = (unsigned long long)width << 32;
                    pcfg->info.fb.crop.height = (unsigned long long)height << 32;
                }
                break;
            default:
                ALOGD("aut_hwd_layer_set_src format=%d don't support", src->format);
                return -1;
        }

        //ALOGD("disp src:%x,%x,%x",pcfg->info.fb.addr[0],pcfg->info.fb.addr[1],pcfg->info.fb.addr[2]);

        //if (pcfg->enable == 1)
        {
            arg[0] = HDL_2_SCREENID(layer_hdl);
            arg[1] = (unsigned long)pcfg;
            arg[2] = 1; // layer num
            ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG, (void*)arg);
            if (ret != 0) {
                ALOGE("aut_hwd_layer_set_src fail to set layer config");
            }
        }
    }

    return ret;
}

int HwDisplay::aut_hwd_layer_render(unsigned int layer_hdl, void* fb_addr0, void* fb_addr1, int fb_share_fd)
{
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];

    struct src_info src;
    src.format = pcfg->info.fb.format;
    src.w = pcfg->info.fb.size[0].width;
    src.h = pcfg->info.fb.size[0].height;

    ret = aut_hwd_layer_set_src(layer_hdl, &src, (unsigned long)fb_addr0, fb_share_fd);

    return ret;
}

int HwDisplay::aut_hwd_layer_set_rect(unsigned int layer_hdl, struct view_info* src_rect)
{
    unsigned long arg[4];
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];

    pcfg->info.fb.crop.x = ((long long)src_rect->x) << 32;
    pcfg->info.fb.crop.y = ((long long)src_rect->y) << 32;
    pcfg->info.fb.crop.width = ((long long)src_rect->w) << 32;
    pcfg->info.fb.crop.height = ((long long)src_rect->h) << 32;
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        arg[0] = HDL_2_SCREENID(layer_hdl);
        arg[1] = (unsigned long)pcfg;
        arg[2] = 1; // layer num
        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_layer_set_rect fail to set layer config");
        }
    }

    return ret;
}

int HwDisplay::aut_hwd_layer_sufaceview(unsigned int layer_hdl, struct view_info* surface)
{
    unsigned long arg[4];
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];

    if ((pcfg->info.screen_win.width == surface->w) &&
            (pcfg->info.screen_win.height == surface->h) &&
            (pcfg->info.screen_win.x == (int)surface->x) &&
            (pcfg->info.screen_win.y == (int)surface->y)) {
        return 0;
    }

    pcfg->info.screen_win.x = surface->x;
    pcfg->info.screen_win.y = surface->y;
    pcfg->info.screen_win.width = surface->w;
    pcfg->info.screen_win.height = surface->h;
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        arg[0] = HDL_2_SCREENID(layer_hdl);
        arg[1] = (unsigned long)pcfg;
        arg[2] = 1; // layer num
        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_layer_sufaceview fail to set layer config");
        }
    }
    return ret;
}

int HwDisplay::aut_hwd_layer_set_zorder(unsigned int layer_hdl, int zorder)
{
    unsigned long arg[4];
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
    if (pcfg->info.zorder == zorder) {
        return 0;
    }

    pcfg->info.zorder = zorder;

    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        arg[0] = HDL_2_SCREENID(layer_hdl);
        arg[1] = (unsigned long)pcfg;
        arg[2] = 1; // layer num
        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_layer_set_zorder fail to set layer config");
        }
    }
    return ret;
}

int HwDisplay::aut_hwd_layer_set_alpha(unsigned int layer_hdl, int alpha_mode, int alpha_value)
{
    int ret = RET_OK;
    unsigned long arg[4];
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];

    if ((pcfg->info.alpha_mode == alpha_mode) &&
            (pcfg->info.alpha_value == alpha_value)) {
        return 0;
    }

    pcfg->info.alpha_mode = alpha_mode;
    pcfg->info.alpha_value = alpha_value;
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        arg[0] = HDL_2_SCREENID(layer_hdl);
        arg[1] = (unsigned long)pcfg;
        arg[2] = 1; // layer num
        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_layer_set_alpha fail to set layer config");
        }
    }

    return ret;
}

int HwDisplay::aut_hwd_vsync_enable(int screen, int enable)
{
    int ret = RET_OK;
    unsigned long arg[4] = {0};

    arg[0] = screen;
    arg[1] = enable;
    ret = ioctl(mDisp_fd, DISP_VSYNC_EVENT_EN, (unsigned long)arg);
    return ret;
}

int HwDisplay::aut_hwd_set_layer_info(unsigned int layer_hdl, disp_layer_info2* info)
{
    unsigned long arg[3];
    int ret = RET_OK;
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];

    memcpy(&pcfg->info, info, sizeof(disp_layer_info));
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);
    if (pcfg->enable == 1) {
        arg[0] = HDL_2_SCREENID(layer_hdl);
        arg[1] = (unsigned long)pcfg;
        arg[2] = 1; // layer num
        ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
        if (ret != 0) {
            ALOGE("aut_hwd_set_layer_info fail to set layer info");
        }
    }
    return ret;
}

disp_layer_config2* HwDisplay::aut_hwd_get_layer_config(unsigned int layer_hdl)
{
    unsigned long arg[3];
    disp_layer_config2* pcfg = &layer_cfg[HDL_2_SCREENID(layer_hdl)][HDL_2_CHANNELID(layer_hdl)][HDL_2_LAYERID(layer_hdl)];
    pcfg->channel = HDL_2_CHANNELID(layer_hdl);
    pcfg->layer_id = HDL_2_LAYERID(layer_hdl);

    arg[0] = HDL_2_SCREENID(layer_hdl);
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1;
    ioctl(mDisp_fd, DISP_LAYER_GET_CONFIG2, (void*)arg);

    return pcfg;
}

disp_layer_config2* HwDisplay::aut_hwd_get_layer_config_by_id(int screen_id, int channel, int layer_id)
{
    unsigned long arg[3];
    disp_layer_config2* pcfg = &layer_cfg[screen_id][channel][layer_id];
    pcfg->channel = channel;
    pcfg->layer_id = layer_id;

    arg[0] = screen_id;
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1;
    ioctl(mDisp_fd, DISP_LAYER_GET_CONFIG2, (void*)arg);

    return pcfg;
}

int HwDisplay::getScreenWidth(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(mDisp_fd, DISP_GET_SCN_WIDTH, (void *)args);
}

int HwDisplay::getScreenHeight(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(mDisp_fd, DISP_GET_SCN_HEIGHT, (void *)args);
}

int HwDisplay::getScreenOutputType(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(mDisp_fd, DISP_GET_OUTPUT_TYPE, (void*)args);
}

int HwDisplay::getLayerFrameId(int screen_id, int channel, int layer_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    args[1] = channel;
    args[2] = layer_id;
    return ioctl(mDisp_fd, DISP_LAYER_GET_FRAME_ID, (void*)args);
}

int HwDisplay::getDispFd(void)
{
    return mDisp_fd;
}

int HwDisplay::hwd_other_screen(int screen_id, int type, int mode)
{
    int ret = RET_OK;
    unsigned long args[4] = {0};

    args[0] = screen_id;
    args[1] = type;
    args[2] = mode;
    ret = ioctl(mDisp_fd, DISP_DEVICE_SWITCH, (void*)args);

    return ret;
}

int HwDisplay::hwd_screen1_mode(int mode)
{
    int ret = RET_OK;
    if (cur_screen1_mode == mode) {
        ALOGD("F:%s,L:%d screen1(%d) enabled already", __FUNCTION__, __LINE__, mode);
        return 0;
    }

    if (cur_screen1_mode > 0) {
        ALOGD("disable screen1 output(%d)", cur_screen1_mode);
        ret = hwd_screen1_disable_output();
    }

    cur_screen1_mode = mode;
    if (cur_screen1_mode > 0) {
        ALOGD("enable screen1 output(%d)", cur_screen1_mode);
        ret = hwd_screen1_enable_output();
    }
    return ret;
}

int HwDisplay::hwd_screen1_disable_output()
{
    int ret;
    unsigned long arg[3];

    disp_layer_config2* pcfg = &layer_cfg[1][FB1_CHANNEL_ID][FB1_LAYER_ID];
    pcfg->enable = 0;
    pcfg->channel = FB1_CHANNEL_ID;
    pcfg->layer_id = FB1_LAYER_ID;

    arg[0] = 1;
    arg[1] = (unsigned long)pcfg;
    arg[2] = 1; // layer num
    ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
    if (ret != 0) {
        ALOGE("fail to set layer config2");
    }
    return ret;
}

int HwDisplay::hwd_screen1_enable_output()
{
    int ret;
    unsigned long arg[3];
    disp_layer_config2 screen0_pcfg;
    memset(&screen0_pcfg, 0, sizeof(disp_layer_config2));
    disp_layer_config2* screen1_pcfg = &layer_cfg[1][FB1_CHANNEL_ID][FB1_LAYER_ID];
    int screen1_width = lcdxres;
    int screen1_height = lcdyres;

    switch (cur_screen1_mode) {
        case DISP_TV_MOD_PAL :
        case DISP_TV_MOD_576P :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_TV, cur_screen1_mode);
            screen1_width = 720;
            screen1_height = 576;
            break;
        case DISP_TV_MOD_NTSC :
        case DISP_TV_MOD_480P :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_TV, cur_screen1_mode);
            screen1_width = 720;
            screen1_height = 480;
            break;
        case DISP_TV_MOD_720P_60HZ :
        case DISP_TV_MOD_720P_50HZ :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_HDMI, cur_screen1_mode);
            screen1_width = 1280;
            screen1_height = 720;
            break;
        case DISP_TV_MOD_1080P_24HZ :
        case DISP_TV_MOD_1080P_25HZ :
        case DISP_TV_MOD_1080P_30HZ :
        case DISP_TV_MOD_1080P_50HZ :
        case DISP_TV_MOD_1080P_60HZ :
        case DISP_TV_MOD_1080I_50HZ :
        case DISP_TV_MOD_1080I_60HZ :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_HDMI, cur_screen1_mode);
            screen1_width = 1920;
            screen1_height = 1080;
            break;
        case DISP_VGA_MOD_640_480P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_640_480P_60);
            screen1_width = 640;
            screen1_height = 480;
            break;
        case DISP_VGA_MOD_800_600P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_800_600P_60);
            screen1_width = 800;
            screen1_height = 600;
            break;
        case DISP_VGA_MOD_1024_768P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_1024_768P_60);
            screen1_width = 1024;
            screen1_height = 768;
            break;
        case DISP_VGA_MOD_1280_768P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_1280_768P_60);
            screen1_width = 1280;
            screen1_height = 768;
            break;
        case DISP_VGA_MOD_1280_720P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_1280_720P_60);
            screen1_width = 1280;
            screen1_height = 720;
            break;
        case DISP_VGA_MOD_1920_1080P_60 :
            hwd_other_screen(1, DISP_OUTPUT_TYPE_VGA, DISP_VGA_MOD_1920_1080P_60);
            screen1_width = 1920;
            screen1_height = 1080;
            break;
        default:
            ALOGE("Screen1 Mode:%d not support", cur_screen1_mode);
            return -1;
    }

    //get fb0 config
    screen0_pcfg.channel = 1;
    screen0_pcfg.layer_id = 0;

    arg[0] = 0; //disp
    arg[1] = (unsigned long)&screen0_pcfg;
    arg[2] = 1; //layer number
    ret = ioctl(mDisp_fd, DISP_LAYER_GET_CONFIG2, (void*)arg);

    memcpy(screen1_pcfg, &screen0_pcfg, sizeof(disp_layer_config2));

    screen1_pcfg->info.screen_win.x = 0;
    screen1_pcfg->info.screen_win.y = 0;
    screen1_pcfg->info.screen_win.width = screen1_width;
    screen1_pcfg->info.screen_win.height = screen1_height;

    screen1_pcfg->info.fb.size[0].width = screen1_width;
    screen1_pcfg->info.fb.size[0].height = screen1_height;
    screen1_pcfg->info.fb.size[1].width = screen1_width;
    screen1_pcfg->info.fb.size[1].height = screen1_height;
    screen1_pcfg->info.fb.crop.width = (unsigned long long)screen1_width << 32;
    screen1_pcfg->info.fb.crop.height = (unsigned long long)screen1_height << 32;

    screen1_pcfg->enable = 1;
    screen1_pcfg->channel = FB1_CHANNEL_ID;
    screen1_pcfg->layer_id = FB1_LAYER_ID;

    arg[0] = 1;
    arg[1] = (unsigned long)screen1_pcfg;
    arg[2] = 1; // layer num
    ret = ioctl(mDisp_fd, DISP_LAYER_SET_CONFIG2, (void*)arg);
    if (ret != 0) {
        ALOGE("fail to set layer config");
    }
    return ret;
}

} // namespace android
