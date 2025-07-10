#ifndef __CAM_UTILS__H_
#define __CAM_UTILS__H_
#include <sys/time.h>
#include <stdio.h>
#include "StorageManager.h"
#include "CameraLog.h"
#include <unistd.h>
#include <dirent.h>
#include <list>
#include <sys/types.h>
#include <linux/magic.h>


#define REMAIN_STORAGE_SIZE 1024*1024*1024
class Utils
{

public:

static int64_t getSystemTimeNow()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000000ll + tv.tv_usec; //us
}

static void generateVideoName(char* name,char* dir,const char* suffix,int id)
{
    int64_t time = getSystemTimeNow();
    sprintf(name,"/%s/cam%d_%ld.%s",dir,id,time,suffix);
}

static int createDir(char* filePath)
{
    bool ret = false;
    ret = createdir(filePath);
    return 0;
}

static int createFile(char* filePath)
{
    FILE * file = fopen(filePath, "wb+");
    if(file == NULL)
    {
        CameraLogE("createFile error:%s",strerror(errno));
        return -1;
    }
    int ret = fclose(file);
    return ret;
}

/**
 * 获得预分配需要的空间大小
 *
 * @param
 * @return
 */
static int calcPreFileSize(int bitrate, int second)
{
    int size = (second) * (bitrate >> 3);
    // 预分配10%余量, 防止实际写入大小超过计算的预分配大小
    double redundancy_size = size * 0.1;
    if (redundancy_size < (3 * (1 << 20)))//3M
        redundancy_size  = 3 * (1 << 20);
    size += (int)redundancy_size;
    CameraLogI("bitrate:%d, time: %d, size: %d, redundancy_size:%d",bitrate,second,size,(int)redundancy_size);
    return size;
}

static int preAllocatefallocate(char* fileName,int size)
{
    int fd = -1;
    int ret = -1;
    fd = open(fileName, O_CREAT | O_RDWR, 0777);
    if(fd < 0) {
        CameraLogE("fallocate open %s failed %s,fd:%d",fileName,strerror(errno),fd);
        return ret;
    }
    ret = fallocate(fd, 0x01, 0, size);
    if(ret){
        CameraLogE("fallocate %s failed %s,fd:%d",fileName,strerror(errno),fd);
    }
    close(fd);
    return ret;
}

static int preAllocateTruncate(char* fileName,int size){
    int ret = -1;
    ret = truncate(fileName, size);
    return ret;
}

static bool isVfat32(char* path)
{
    struct statfs fs;
    if (statfs(path, &fs) == -1) {
        CameraLogE("Error:%s  isVfat32",strerror(errno));
        return 1;
    }
    return fs.f_type == MSDOS_SUPER_MAGIC;
}

static bool isEmpty(char* str)
{
    if(NULL == str){return true;}
    if(!strcmp("",str)){return true;}
    return false;
}


static bool isExist(char* file)
{
    if(NULL == file){return false;}
    if(!strcmp("",file)){return false;}
    return access(file, F_OK) == 0;
}

static void getFileList(char* dir,std::list<char*>* files)
{
    DIR *dirp;
    struct dirent *entry;
    dirp = opendir(dir);
    while ((entry = readdir(dirp)) != NULL) {
            files->push_back(entry->d_name);
    }
    closedir(dirp);
}

static void getVideoFileList(char* dir,std::list<char*>* files)
{
    DIR *dirp;
    struct dirent *entry;
    dirp = opendir(dir);
    int i = 0;
    while ((entry = readdir(dirp)) != NULL) {
        char* suffix = strrchr(entry->d_name, '.');
        if (suffix && (!strcmp(suffix + 1, "ts") || !strcmp(suffix + 1, "mp4"))) {
            // CameraLogE("i:%d push_back file:%s",i,entry->d_name);
            files->push_back(entry->d_name);
            i++;
        }
    }
    closedir(dirp);
    files->sort(Utils::compareFileName);
}


static bool compareFileName(char* str1,char* str2)
{
    long num1 = getFileNameTime(str1);
    long num2 = getFileNameTime(str2);
    return num1 > num2;
}


static long getFileNameTime(char* name)
{
    char *pos_ = strchr(name, '_');
    char *pos_dot = strchr(name, '.');
    char tmp[32];
    memset(tmp,0,sizeof(tmp));
    memcpy((void*)tmp,pos_+1,pos_dot-pos_-1);
    long result = atol(tmp);
    return result;
}

static char* getDir(char* path)
{
    int index = 0;
    int lastSeperateIndex = 0;
    while (char c = path[index])
    {
        index++;
        if (c == '/')
        {
            lastSeperateIndex = index;
        }
    }
    char*dir = (char*)malloc(index);
    memset(dir,0,index);
    memcpy(dir,path,lastSeperateIndex);
    return dir;
}

static bool checkSpaceEnough(char* filePath)
{
    char* dir  = getDir(filePath);
    unsigned long long size = availSize(dir);
    free(dir);
    unsigned long long remainSize = REMAIN_STORAGE_SIZE;

    if (size >= remainSize)
    {
        return true;
    }
    return false;
}

static int saveframe(char *str, void *p, int length, int is_oneframe)
{
    FILE *fd;

    if (is_oneframe) {
        fd = fopen(str, "wb");
    } else {
        fd = fopen(str, "a");
    }

    if (!fd) {
        CameraLogE("Open file error");
        return -1;
    }
    int written = 0;
    if ((written = fwrite(p, 1, length, fd)) == length) {
        CameraLogD("Write file successfully");
        fclose(fd);
        return 0;
    } else {
        CameraLogE("Write file fail %s", strerror(errno));
        fclose(fd);
        return -1;
    }
}


};

#endif