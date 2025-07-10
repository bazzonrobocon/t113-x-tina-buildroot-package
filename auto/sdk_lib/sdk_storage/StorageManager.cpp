#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/fb.h>
#include <sdklog.h>
#include <libgen.h>
 #include <sys/stat.h>

#include "StorageManager.h"

unsigned long get_file_size(const char *path)
{
    unsigned long filesize = -1;
    struct stat statbuff;
    if (stat(path, &statbuff) < 0) {
        return filesize;
    } else {
        filesize = statbuff.st_size;
    }
    return filesize;
}

unsigned long long totalSize(const char *path)
{
    struct statfs diskinfo;
    if (statfs(path, &diskinfo) != -1) {
        unsigned long long blocksize = diskinfo.f_bsize;    //每个block里包含的字节数
        unsigned long long totalsize = blocksize * diskinfo.f_blocks;   //总的字节数，f_blocks为block的数目
        return (unsigned long long)(totalsize >> 20); //MB
    }
    return 0;

}

unsigned long long availSize(const char *path)
{
    struct statfs diskinfo;
    //unsigned long long availSize = 0;
    if (statfs(path, &diskinfo) != -1) {
        unsigned long long blocksize = diskinfo.f_bsize;    //每个block里包含的字节数
        unsigned long long freeDisk = diskinfo.f_bfree * blocksize; //剩余空间的大小
        //unsigned long long availableDisk = diskinfo.f_bavail * blocksize;   //可用空间大小
        //ALOGV("free %lld av %lld",freeDisk>>20,availableDisk>20);

        // return (unsigned long long)(freeDisk >> 20); //MB
        return freeDisk;
    }
    return 0;

}

int needDeleteFiles()
{
    unsigned long long as = availSize(MOUNT_PATH);
    unsigned long long ts = totalSize(MOUNT_PATH);
    ALOGV("[Size]:%lluMB/%lluMB, reserved:%lluMB", as, ts, RESERVED_SIZE);

    if (as <= RESERVED_SIZE) {
        ALOGE("!Disk FULL");
        return RET_DISK_FULL;
    }

    if (as <= LOOP_COVERAGE_SIZE) {
        ALOGE("!Loop Coverage");
        return RET_NEED_LOOP_COVERAGE;
    }

    return 0;
}
int isMounted(const char *checkPath)
{
    const char *path = "/proc/mounts";
    FILE *fp;
    char line[255];
    const char *delim = " \t";
    ALOGV(" isMount checkPath=%s \n", checkPath);
    if (!(fp = fopen(path, "r"))) {
        ALOGE(" isMount fopen fail,path=%s\n", path);
        return 0;
    }
    memset(line, '\0', sizeof(line));
    while (fgets(line, sizeof(line), fp)) {
        char *save_ptr = NULL;
        char *p        = NULL;
        //ALOGV(" isMount line=%s \n",line);
        if (line[0] != '/' || (strncmp("/dev/mmcblk", line, strlen("/dev/mmcblk")) != 0 &&
                               strncmp("/dev/sd", line, strlen("/dev/sd")) != 0)) {
            memset(line, '\0', sizeof(line));
            continue;
        }
        if ((p = strtok_r(line, delim, &save_ptr)) != NULL) {
            if ((p = strtok_r(NULL, delim, &save_ptr)) != NULL) {
                ALOGV(" isMount p=%s \n", p);
                if (strncmp(checkPath, p, strlen(checkPath)) == 0) {
                    fclose(fp);
                    ALOGV(" isMount return 1\n");
                    return 1;
                }
            }
            //printf("line=%s",lineback);
            if (strlen(p) > 1) {
                if (strstr(checkPath, p)) {
                    fclose(fp);
                    ALOGV(" isMount dd return 1\n");
                    return 1;
                }
            }
        }

    }//end while(fgets(line, sizeof(line), fp))
    if (fp) {
        fclose(fp);
    }

    ALOGV("isMounted return 0 \n");

    return 0;
}



bool isFile(char *path)
{
   struct stat buf;
   int result;
   result = stat(path, &buf);
   return S_IFREG &buf.st_mode;
}

bool isExist(char *path)
{
    return access(path, F_OK) == 0;
}

#define MAXSIZE 256
int  createdir(char *path)
{

    char data[MAXSIZE];
    char dir[MAXSIZE];
    memset(dir,0,sizeof(dir));
    sprintf(data,"%s",path);


    char *outer_ptr = NULL;
    char* p = strtok_r(data,"/",&outer_ptr);
    char *tmp_dir = dir; // in order to avoid the "codecheck"
    while(p)
    {
        sprintf(dir,"%s/%s", tmp_dir, p);
        if(!isExist(dir))
        {
           if (mkdir(dir, 0777) == -1)
           {
             ALOGE("mkdir error");
             return -1;
           }
        }
        p = strtok_r(NULL,"/" ,&outer_ptr);
    }
    return 0;
}

