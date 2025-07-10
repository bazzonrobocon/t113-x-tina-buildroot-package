#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "sunxi_display2.h"

void printUsage()
{
    printf("================Usage================\n");
    printf("lcd_bright_test 1 0	  means: get lcd brightness\n");
    printf("lcd_bright_test 2 50	  means: set lcd brightness to 50, up to 255\n");
#ifndef _T113_
    printf("lcd_bright_test 3 1	  means: enable lcd backlight\n");
    printf("lcd_bright_test 3 0	  means: disable lcd backlight\n");
#endif
    printf("================usage================\n");
}

int main(int argc, char **argv)
{
    printf("lcd_bright_test test version:%s\n", MODULE_VERSION);
    unsigned long arg[4] = {0};
    int dispfd;
    int ret;

    if (argc < 3) {
        printUsage();
        exit(-1);
    }

    int cmd = atoi(argv[1]);
    /* open disp dev */
    if ((dispfd = open("/dev/disp", O_RDWR)) == -1) {
        printf("open display device fail!\n");
        return -1;
    }

    switch (cmd) {
        case 1:
            /* get the brightness */
        {
            int bl = 0;
            arg[0] = 0; //显示通道0
            bl = ioctl(dispfd, DISP_LCD_GET_BRIGHTNESS, (void *)arg);
            bl = 0xff - bl;
            printf("Get lcd brightness is %d\n", bl);
        }
        break;

        case 2:
            /* set the brightness */
        {
            int bl = 0xff - atoi(argv[2]);
            arg[0] = 0; //显示通道0
            arg[1] = bl;
            ret = ioctl(dispfd, DISP_LCD_SET_BRIGHTNESS, (void *)arg);
            if (ret != 0) {
                printf("DISP_LCD_SET_BRIGHTNESS FAILED!\n");
                break;
            }

            bl = ioctl(dispfd, DISP_LCD_GET_BRIGHTNESS, (void *)arg);
            bl = 0xff - bl;
            printf("Set lcd brightness to %d\n", bl);
        }
        break;
#ifndef _T113_
        case 3:
            /* console the backlight */
        {
            int bl_status = atoi(argv[2]);
            arg[0] = 0;
            if (bl_status == 0) {
                ret = ioctl(dispfd, DISP_LCD_BACKLIGHT_DISABLE, (void *)arg);
                if (ret != 0) {
                    printf("DISP_LCD_BACKLIGHT_ENABLE FAILED!\n");
                } else {
                    printf("Disable lcd backlight\n");
                }
            } else {
                ret = ioctl(dispfd, DISP_LCD_BACKLIGHT_ENABLE, (void *)arg);
                if (ret != 0) {
                    printf("DISP_LCD_BACKLIGHT_DISABLE FAILED!\n");
                } else {
                    printf("Enable lcd backlight\n");
                }
            }
        }
        break;
#endif
        default:
            printUsage();
            printf("please input right params!\n");
            break;
    }

    return 0;
}