#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include "sunxi_disp_hal.h"

#if USE_LOGCAT
#define LOG_TAG "sunxi_disp_hal"
#include "sdklog.h"
#else
#define ALOGD(x, arg...) printf(x"\n", ##arg)
#define ALOGW ALOGD
#define ALOGE ALOGD
#endif

pthread_mutex_t SunxiDisplay::p_mutex_t = PTHREAD_MUTEX_INITIALIZER;
SunxiDisplay* SunxiDisplay::m_disp = NULL;

SunxiDisplay::SunxiDisplay()
{
    ALOGD("libsdk_disp v2.version:%s", API2_MODULE_VERSION);

    // init layer_cfg's layer_id and channel_id
    int sc, ch, lyr;
    for (sc = 0; sc < SCREEN_NUM; sc++) {
        for (ch = 0; ch < CHANNEL_NUM; ch++) {
            for (lyr = 0; lyr < LAYER_NUM; lyr++) {
                sdk_layer_cfg[sc][ch][lyr].layer_cfg2.channel = ch;
                sdk_layer_cfg[sc][ch][lyr].layer_cfg2.layer_id = lyr;
                sdk_layer_cfg[sc][ch][lyr].layer_cfg2.enable = 0;
                sdk_layer_cfg[sc][ch][lyr].layer_status = 0;
            }
        }
    }

    disp_fd = open(DISP_DEV, O_RDWR);
    if (disp_fd < 0) {
        ALOGE("Failed to open /dev/disp device, ret:%d, errno:%d", disp_fd, errno);
    }

    int fb0_fd;
    struct fb_var_screeninfo fb_var;

    if ((fb0_fd = open("/dev/fb0", O_RDWR)) == -1) {
        ALOGE("Failed to open /dev/fb0 device, ret:%d, errno:%d", fb0_fd, errno);
    }

    if (ioctl(fb0_fd, FBIOGET_VSCREENINFO, &fb_var)) {
        ALOGE("ioctl FBIOGET_VSCREENINFO fail");
    }
    fb0_width = fb_var.xres;
    fb0_height = fb_var.yres;

    close(fb0_fd);
}

SunxiDisplay::~SunxiDisplay()
{
    if (disp_fd > 0) {
        ALOGD("close disp_fd:%d", disp_fd);
        close(disp_fd);
    }
    disp_fd = -1;
    fb0_width = 0;
    fb0_height = 0;
}

SunxiDisplay* SunxiDisplay::get_instance()
{
    if (m_disp == NULL) {
        pthread_mutex_lock(&p_mutex_t);
        if (m_disp == NULL) {
            m_disp = new SunxiDisplay;
        }
        pthread_mutex_unlock(&p_mutex_t);
    }
    return m_disp;
}

bool check_layer_id_valid(int screen_id, int channel, int layer_id)
{
    if ((screen_id > SCREEN_NUM-1) || (screen_id < 0)) {
        ALOGE("screen_id(%d) is invalid, please check!", screen_id);
        return false;
    }
    if ((channel > CHANNEL_NUM-1) || (channel < 0)) {
        ALOGE("channel(%d) is invalid, please check!", channel);
        return false;
    }
    if ((layer_id > LAYER_NUM-1) || (layer_id < 0)) {
        ALOGE("layer_id(%d) is invalid, please check!", layer_id);
        return false;
    }
    return true;
}

bool check_layer_hdl_valid(int layer_hdl)
{
    if (!check_layer_id_valid(HDL_2_SCREENID(layer_hdl), HDL_2_CHANNELID(layer_hdl),
            HDL_2_LAYERID(layer_hdl))) {
        ALOGE("layer_hdl(%d) is invalid, please check!", layer_hdl);
        return false;
    }
    return true;
}

int SunxiDisplay::request_layer(int screen_id, int channel, int layer_id)
{
    if (!check_layer_id_valid(screen_id, channel, layer_id)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;
    int layer_status = sdk_layer_cfg[screen_id][channel][layer_id].layer_status;
    if (layer_status == 1) {
        return COMP_LAYER_HDL(screen_id, channel, layer_id);
    }

    unsigned long args[3];
    int ret = 0;

    memset(pcfg2, 0, sizeof(disp_layer_config2));

    pcfg2->channel = channel;
    pcfg2->layer_id = layer_id;
    args[0] = screen_id;
    args[1] = (unsigned long)pcfg2;
    args[2] = 1;
    ret = ioctl(disp_fd, DISP_LAYER_GET_CONFIG2, (void*)args);
    if (ret < 0) {
        ALOGE("ioctl DISP_LAYER_GET_CONFIG2 failed, ret:%d", ret);
        return -1;
    }

    if (!pcfg2->enable) {
        pcfg2->info.zorder = 0;
        pcfg2->info.alpha_mode = 1; //global alpha
        pcfg2->info.alpha_value = 0xff;
        pcfg2->info.fb.format = DISP_FORMAT_YUV420_SP_VUVU;
        pcfg2->info.mode = LAYER_MODE_BUFFER;

        pcfg2->info.screen_win.x = 0;
        pcfg2->info.screen_win.y = 0;
        pcfg2->info.screen_win.width = fb0_width;
        pcfg2->info.screen_win.height = fb0_height;

        if (pcfg2->info.fb.color_space == 0) {
            pcfg2->info.fb.color_space =
                (fb0_height < 720) ? DISP_BT601 : DISP_BT709;
        }
        pcfg2->info.atw.cof_fd = -1;
        pcfg2->info.fb.align[0] = 4;
    }

    sdk_layer_cfg[screen_id][channel][layer_id].layer_status = 1;

    return COMP_LAYER_HDL(screen_id, channel, layer_id);
}

int SunxiDisplay::release_layer(int layer_hdl)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }

    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    if (sdk_layer_cfg[screen_id][channel][layer_id].layer_status == 0) {
        return -2;
    }

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    if (pcfg2->enable) {
        disable_layer(layer_hdl);
    }

    sdk_layer_cfg[screen_id][channel][layer_id].layer_status = 0;

    return 0;
}

int SunxiDisplay::enable_layer(int layer_hdl)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    unsigned long args[4];
    int ret = 0;
    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    pcfg2->enable = 1;
     args[0] = screen_id;
    args[1] = (unsigned long)pcfg2;
    args[2] = 1;
    ret = ioctl(disp_fd, DISP_LAYER_SET_CONFIG2, (void*)args);
    if (ret != 0) {
        ALOGE("ioctl DISP_LAYER_SET_CONFIG2 failed, ret:%d", ret);
        pcfg2->enable = 0;
        return -3;
    }

    return ret;
}

int SunxiDisplay::disable_layer(int layer_hdl)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;
    unsigned long args[4];
    int ret = 0;

    if (!pcfg2->enable) {
        ALOGE("the layer[%d][%d][%d] is not enabled", screen_id, channel, layer_id);
        return -2;
    }

    pcfg2->enable = 0;
    args[0] = screen_id;
    args[1] = (unsigned long)pcfg2;
    args[2] = 1;
    ret = ioctl(disp_fd, DISP_LAYER_SET_CONFIG2, (void*)args);
    if (ret != 0) {
        ALOGE("ioctl DISP_LAYER_SET_CONFIG failed, ret:%d", ret);
        pcfg2->enable = 1;
        return -3;
    }

    return ret;
}

int SunxiDisplay::set_layer_zorder(int layer_hdl, int zorder)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    pcfg2->info.zorder = zorder;

    return 0;
}

int SunxiDisplay::set_layer_alpha(int layer_hdl, int alpha_mode, int alpha_value)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    if (alpha_mode < 0 || alpha_value < 0) {
        ALOGE("[%s:%d] alpha check failed, mode:%d,value:%d", __func__, __LINE__,
            alpha_mode, alpha_value);
        return -2;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    pcfg2->info.alpha_mode = alpha_mode;
    pcfg2->info.alpha_value = alpha_value;

    return 0;
}

int SunxiDisplay::set_layer_frame(int layer_hdl, struct disp_rect* frame)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    pcfg2->info.screen_win.x = frame->x;
    pcfg2->info.screen_win.y = frame->y;
    pcfg2->info.screen_win.width = frame->width;
    pcfg2->info.screen_win.height = frame->height;

    return 0;
}

int update_layer_fb_info2(struct fb_src_info* fb_src, struct disp_fb_info2* fb_info)
{
    switch (fb_src->format) {
        case DISP_FORMAT_YUV444_P :
            fb_info->format = (disp_pixel_format) fb_src->format;
            fb_info->size[0].width = fb_src->width;
            fb_info->size[0].height = fb_src->height;
            fb_info->size[1].width = fb_src->width;
            fb_info->size[1].height = fb_src->height;
            fb_info->size[2].width = fb_src->width;
            fb_info->size[2].height = fb_src->height;
            break;
        case DISP_FORMAT_YUV420_SP_VUVU :
        case DISP_FORMAT_YUV420_SP_UVUV :
            fb_info->format = (disp_pixel_format) fb_src->format;
            fb_info->size[0].width = fb_src->width;
            fb_info->size[0].height = fb_src->height;
            fb_info->size[1].width = fb_src->width / 2;
            fb_info->size[1].height = fb_src->height / 2;
            fb_info->size[2].width = fb_src->width / 2;
            fb_info->size[2].height = fb_src->height / 2;
            break;
        case DISP_FORMAT_YUV422_I_YVYU :
        case DISP_FORMAT_YUV422_I_YUYV :
        case DISP_FORMAT_YUV422_I_UYVY :
        case DISP_FORMAT_YUV422_I_VYUY :
            fb_info->format = (disp_pixel_format) fb_src->format;
            fb_info->size[0].width = fb_src->width;
            fb_info->size[0].height = fb_src->height;
            break;
        case DISP_FORMAT_YUV420_P :
            fb_info->format  = (disp_pixel_format) fb_src->format;
            fb_info->size[0].width  = fb_src->width;
            fb_info->size[0].height = fb_src->height;
            fb_info->size[1].width  = fb_src->width  / 2;
            fb_info->size[1].height = fb_src->height / 2;
            fb_info->size[2].width  = fb_src->width  / 2;
            fb_info->size[2].height = fb_src->height / 2;
            fb_info->align[1] = 0;
            fb_info->align[2] = 0;
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
            fb_info->format  = (disp_pixel_format) fb_src->format;
            fb_info->flags = DISP_BF_NORMAL;
            fb_info->scan = DISP_SCAN_PROGRESSIVE;
            fb_info->color_space = DISP_BT601;
            fb_info->size[0].width = fb_src->width;
            fb_info->size[0].height = fb_src->height;
            fb_info->size[1].width = fb_src->width;
            fb_info->size[1].height = fb_src->height;
            fb_info->size[2].width = fb_src->width;
            fb_info->size[2].height = fb_src->height;
            break;
        default:
            ALOGE("fb_info format:%d don't support", fb_src->format);
            return -1;
    }

    if ((fb_info->crop.width == 0) || (fb_info->crop.width == 0)) {
        fb_info->crop.width = (unsigned long long)fb_src->width << 32;
        fb_info->crop.height = (unsigned long long)fb_src->height << 32;
    }

    return 0;
}

int SunxiDisplay::set_layer_fb_addr(int layer_hdl, struct fb_src_info* fb_src, int fb_fd)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }

    int ret = 0;
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    if (fb_fd > 0) {
        pcfg2->info.fb.fd = fb_fd;
        ret = update_layer_fb_info2(fb_src, &pcfg2->info.fb);
    } else {
        ALOGE("[%s:%d] fb buffer is invalid, please check!", __func__, __LINE__);
        return -2;
    }

    return ret;
}

int SunxiDisplay::set_layer_fb_crop(int layer_hdl, struct disp_rect64* fb_crop)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return -1;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    pcfg2->info.fb.crop.x = fb_crop->x << 32;
    pcfg2->info.fb.crop.y = fb_crop->y << 32;
    pcfg2->info.fb.crop.width = fb_crop->width << 32;
    pcfg2->info.fb.crop.height = fb_crop->height << 32;

    return 0;
}

int SunxiDisplay::get_layer_info2(int layer_hdl, struct disp_layer_info2* layer_info2)
{
    int ret = 0;
    ret = get_layer_info2_by_id(HDL_2_SCREENID(layer_hdl), HDL_2_CHANNELID(layer_hdl),
        HDL_2_LAYERID(layer_hdl), layer_info2);
    return ret;
}

int SunxiDisplay::get_layer_info2_by_id(int screen_id, int channel, int layer_id, struct disp_layer_info2* layer_info2)
{
    disp_layer_info2* pinfo2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2.info;

    int layer_status = sdk_layer_cfg[screen_id][channel][layer_id].layer_status;
    if (layer_status == 0) {
        ALOGE("this layer is not request, please call disp_layer_request() first");
        return -1;
    }

    memcpy(layer_info2, pinfo2, sizeof(struct disp_layer_info2));
    return 0;
}

int SunxiDisplay::set_layer_info2(int layer_hdl, struct disp_layer_info2* layer_info2)
{
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    memcpy(&pcfg2->info, layer_info2, sizeof(struct disp_layer_info2));

    return 0;
}

int SunxiDisplay::enable_disp_vsync(int screen_id, int enable)
{
    int ret = 0;
    unsigned long args[4] = {0};

    args[0] = screen_id;
    args[1] = enable;
    ret = ioctl(disp_fd, DISP_VSYNC_EVENT_EN, (unsigned long)args);
    return ret;
}

int SunxiDisplay::get_screen_width(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(disp_fd, DISP_GET_SCN_WIDTH, (void *)args);
}

int SunxiDisplay::get_screen_height(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(disp_fd, DISP_GET_SCN_HEIGHT, (void *)args);
}

int SunxiDisplay::get_screen_output_type(int screen_id)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    return ioctl(disp_fd, DISP_GET_OUTPUT_TYPE, (void*)args);
}

int SunxiDisplay::switch_output_type(int screen_id, int output_type, int output_mode)
{
    unsigned long args[4] = {0};
    args[0] = screen_id;
    args[1] = output_type;
    args[2] = output_mode;
    return ioctl(disp_fd, DISP_DEVICE_SWITCH, (void*)args);
}

int SunxiDisplay::get_lcd_brightness(void)
{
    unsigned long args[4] = {0};
    return ioctl(disp_fd, DISP_LCD_GET_BRIGHTNESS, (void *)args);
}

int SunxiDisplay::set_lcd_brightness(int bright_value)
{
    unsigned long args[4] = {0};
    args[0] = 0;
    args[1] = bright_value;
    return ioctl(disp_fd, DISP_LCD_SET_BRIGHTNESS, (void *)args);
}

int SunxiDisplay::set_lcd_backlight(bool enable)
{
    unsigned long args[4] = {0};
    int ret = 0;
    if (enable) {
        ret = ioctl(disp_fd, DISP_LCD_BACKLIGHT_ENABLE, (void *)args);
    } else {
        ret = ioctl(disp_fd, DISP_LCD_BACKLIGHT_DISABLE, (void *)args);
    }
    return ret;
}

void SunxiDisplay::print_sdk_layer_cfg(int layer_hdl)
{
    if (!check_layer_hdl_valid(layer_hdl)) {
        ALOGE("[%s:%d] params check failed", __func__, __LINE__);
        return ;
    }
    int screen_id = HDL_2_SCREENID(layer_hdl);
    int channel = HDL_2_CHANNELID(layer_hdl);
    int layer_id = HDL_2_LAYERID(layer_hdl);

    disp_layer_config2* pcfg2 = &sdk_layer_cfg[screen_id][channel][layer_id].layer_cfg2;

    ALOGD("");
    ALOGD("-------------------------start dump layer config info-------------------------");
    ALOGD("");

    ALOGD("sdk_disp_layer_config[%d][%d][%d],layer_status:%d",
        screen_id, channel, layer_id,
        sdk_layer_cfg[screen_id][channel][layer_id].layer_status);

    ALOGD("==========================disp_layer_config2==========================");
    ALOGD("enable:%d,channel:%d, layer_id:%d", pcfg2->enable, pcfg2->channel, pcfg2->layer_id);
    ALOGD("disp_layer_mode:%d, zorder:%d, alpha_mode:%d, alpha_value:%d", pcfg2->info.mode,
        pcfg2->info.zorder, pcfg2->info.alpha_mode, pcfg2->info.alpha_value);
    ALOGD("layer frame[%d %d %d %d], fb crop[%lld %lld %lld %lld]",
        pcfg2->info.screen_win.x, pcfg2->info.screen_win.y,
        pcfg2->info.screen_win.width, pcfg2->info.screen_win.height,
        pcfg2->info.fb.crop.x >> 32, pcfg2->info.fb.crop.y >> 32,
        pcfg2->info.fb.crop.width >> 32, pcfg2->info.fb.crop.height >> 32);
    ALOGD("fb fd:%d, fb size[%dx%d %dx%d %dx%d]", pcfg2->info.fb.fd,
        pcfg2->info.fb.size[0].width, pcfg2->info.fb.size[0].height,
        pcfg2->info.fb.size[1].width, pcfg2->info.fb.size[1].height,
        pcfg2->info.fb.size[2].width, pcfg2->info.fb.size[2].height);
    ALOGD("fb format:%d, pre_multiply:%d, color_space:%d, flags:%d", pcfg2->info.fb.format,
        pcfg2->info.fb.pre_multiply, pcfg2->info.fb.color_space, pcfg2->info.fb.flags);

    ALOGD("------------------------------------------------------------------------------");
    ALOGD("");
}
