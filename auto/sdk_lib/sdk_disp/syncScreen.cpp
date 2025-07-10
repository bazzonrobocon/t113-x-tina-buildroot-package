#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "hwdisp2.h"

//fbMappingToScreen1(1,1,1,0,720,480,0);

int fbMappingToScreen1(int fbnode, int toScreenId, int toChannelId, int toLayerId, int screen1Width, int screen1Height, int fmt)
{
    int fd;
    char devName[20];
    memset(devName, 0, sizeof(char) * 20);
    sprintf(devName, "/dev/fb%d", fbnode);
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if ((fd = open(devName, O_RDWR)) == -1) {
        printf("open file %s fail. \n", devName);
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return -1;
    }
    close(fd);


    int fbPhyaddr = fix.smem_start;
    unsigned int fbWidth = var.width;
    unsigned int fbHeight = var.height;

#if 0
    fd = 0;
    memset(&fix, 0, sizeof(fix));
    memset(&var, 0, sizeof(var));
    if ((fd = open("dev/fb1", O_RDWR)) == -1) {
        printf("open file /dev/fb1 fail. \n");
        return 0;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return NULL;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return NULL;
    }
    close(fd);

    int fb1Phyaddr = fix.smem_start;
    int fb1Width = var.width;
    int fb1Height = var.height;
#endif


    android::HwDisplay* mcd = android::HwDisplay::getInstance();
    if (mcd == NULL) {
        printf("syncScreen1Thread is NULL\n");
        return -1;
    }


    struct view_info frame = {0, 0, fbWidth, fbHeight};
    struct view_info screen1Sufaceview = {0, 0, (unsigned int)screen1Width, (unsigned int)screen1Height};

    struct src_info src = {fbWidth, fbHeight, (unsigned int)fmt};

    int fbMappingToLayer = mcd->aut_hwd_layer_request(&frame, toScreenId, toChannelId, toLayerId);
    mcd->aut_hwd_layer_sufaceview(fbMappingToLayer, &screen1Sufaceview);
    mcd->aut_hwd_layer_set_src(fbMappingToLayer, &src, fbPhyaddr);
    mcd->aut_hwd_layer_set_rect(fbMappingToLayer, &frame);
    //mcd->aut_hwd_layer_set_zorder(fbMappingToLayer,6);
    mcd->aut_hwd_layer_open(fbMappingToLayer);

#if 0
    int fb1MappingToLayer = mcd->aut_hwd_layer_request(&frame, fb1MappingToScreenId, fb1MappingToChannelId, fb1MappingToLayerId);
    mcd->aut_hwd_layer_sufaceview(fb1MappingToLayer, &screen1Sufaceview);
    mcd->aut_hwd_layer_set_src(fb1MappingToLayer, &src, fb1Phyaddr);
    mcd->aut_hwd_layer_set_rect(fb1MappingToLayer, &frame);
    //mcd->aut_hwd_layer_set_zorder(fb1MappingToLayer,6);
    mcd->aut_hwd_layer_open(fb1MappingToLayer);


    volatile int backyoffset = 0;

    int i = 0;
    int width = ((int*)param)[i++];
    int height = ((int*)param)[i++];
    int src_info_fmt = ((int*)param)[i++];  //src_info_fmt
    int addrPhy = ((int*)param)[i++]; //addrPhy;
    int screenId = ((int*)param)[i++];  //screenid
    int channelId = ((int*)param)[i++];  //ch_id
    int layerId = ((int*)param)[i++];  //lyr_id
    int devnode = ((int*)param)[i++];  //dev/fbX
    int offsetflagEn = ((int*)param)[i++];
    int output_width = ((int*)param)[i++];  //output_width
    int output_height = ((int*)param)[i++];  //output_height
    char devName[20];
    memset(devName, 0, sizeof(char) * 20);
    sprintf(devName, "/dev/fb%d", devnode);

    struct fb_var_screeninfo var;
    if ((Fb0_fd = open(devName, O_RDWR)) == -1) {
        printf("open file /dev/fb0 fail. \n");
        return NULL;
    }

    if (ioctl(Fb0_fd, FBIOGET_VSCREENINFO, &var) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return NULL;
    }

    printf("var xres=%d,xoffset=%d,bits_per_pixel=%d,fbActivate=%d,h=%d,w=%d,yoffset=0x%x\n",
           var.xres, var.xoffset, var.bits_per_pixel, var.activate, var.height, var.width, var.yoffset);

    if (offsetflagEn) {
        backyoffset = var.yoffset;
    }

    struct fb_fix_screeninfo fix;
    if (ioctl(Fb0_fd, FBIOGET_FSCREENINFO, &fix) == -1) {
        printf("syncScreen1Thread get screen information failure\n");
        return NULL;
    }
    printf("fix smem =%x,smem_len=%d,type=%d,xpanstep=%d,ypanstep,ywrapstep,mmio_start=%p,mmio_len=%d\n",
           fix.smem_start, fix.smem_len, fix.type, fix.xpanstep, fix.ypanstep, fix.ywrapstep, fix.mmio_start, fix.mmio_len);



    addrPhy = (int)fix.smem_start;



    android::HwDisplay* mcd = android::HwDisplay::getInstance();
    if (mcd == NULL) {
        printf("syncScreen1Thread is NULL\n");
        return NULL;
    }
    printf("syncScreen1Thread:from(w=%d,h=%d,fmt=%d fb=%d)\nto:(screenId=%d,channelId=%d,layerId=%d)\n",
           width, height, src_info_fmt, devnode, screenId, channelId, layerId);
    int addrOffset;

    mcd->hwd_other_screen(1, DISP_OUTPUT_TYPE_TV, DISP_TV_MOD_NTSC);

    struct view_info frame = {0, 0, width, height};
    //struct view_info screen1Sufaceview={0,0,720, 480};

    struct src_info src = {width, height, src_info_fmt};

    int fbMappingToLayer = mcd->aut_hwd_layer_request(&frame, screenId, channelId, layerId);
    //mcd->aut_hwd_layer_screen1Sufaceview(fbMappingToLayer,&screen1Sufaceview);
    mcd->aut_hwd_layer_set_src(fbMappingToLayer, &src, (int)fix.smem_start);
    //mcd->aut_hwd_layer_set_rect(fbMappingToLayer,&frame);
    //mcd->aut_hwd_layer_set_zorder(fbMappingToLayer,6);
    mcd->aut_hwd_layer_open(fbMappingToLayer);


    int videoChannelId = 0;
    int fb1MappingToLayer = mcd->aut_hwd_layer_request(&frame, screenId, videoChannelId, layerId);
    int videoAddrPhy = 0x65600000;//(int)fix.smem_start;//0x65600000;

    //mcd->aut_hwd_layer_screen1Sufaceview(fb1MappingToLayer,&screen1Sufaceview);
    mcd->aut_hwd_layer_set_src(fb1MappingToLayer, &src, videoAddrPhy);
    //mcd->aut_hwd_layer_set_rect(fb1MappingToLayer,&frame);
    //mcd->aut_hwd_layer_set_zorder(fb1MappingToLayer,6);
    mcd->aut_hwd_layer_open(fb1MappingToLayer);



#endif
    return 0;
}//;

//}