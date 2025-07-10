/**
 * File: hht_daemon_main.c
 * Auth: fly@ococci.com
 * Desc: daemon proesss
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <syslog.h>
#include<sys/wait.h> 
#include<fcntl.h> 
#include<limits.h> 

#define DAEMON_INFO 0
#define DAEMON_DBUG 1
#define DAEMON_WARN 1
#define DAEMON_EROR 1

#if DAEMON_INFO
#define daemon_info(fmt, ...) printf("\e[0;32m[WIFI_INFO] [%s %d] : "fmt"\e[0m\n", __func__, __LINE__, ##__VA_ARGS__);
#else
#define daemon_info(fmt, ...)
#endif

#if DAEMON_DBUG
#define daemon_debug(fmt, ...) printf("\e[0m[WIFI_DBUG] [%s %d] : "fmt"\e[0m\n", __func__, __LINE__, ##__VA_ARGS__);
#else
#define daemon_debug(fmt, ...)
#endif

#if DAEMON_WARN
#define daemon_warn(fmt, ...) printf("\e[0;33m[WIFI_WARN] [%s %d] : "fmt"\e[0m\n", __func__, __LINE__, ##__VA_ARGS__);
#else
#define daemon_warn(fmt, ...)
#endif

#if DAEMON_EROR
#define daemon_error(fmt, ...) printf("\e[0;31m[WIFI_EROR] [%s %d] : "fmt"\e[0m\n", __func__, __LINE__, ##__VA_ARGS__);
#else
#define daemon_error(fmt, ...)
#endif
/*
 /usr/bin/sys_info &
 /usr/bin/show_led &
 /usr/bin/HappyMQ &
 /usr/bin/local_voice &
 /usr/bin/player_ctrl &  
 /usr/bin/adc_btn &
 wifi_config &
 /usr/bin/h5_ws &
*/
 //程序名字
#define PS_applauncher "qtlauncher"
#define PS_MediaUI "MediaUI"
#define PS_fltest_qt_backlight "fltest_qt_backlight"
#define PS_test_qt_keypad "qt_test_keypad"
#define PS_test_qt_ping_test "test_qt_ping_test"
#define PS_fltest_qt_qopenglwidget "fltest_qt_qopenglwidget"
#define PS_test_qt_watchdog "qt_test_watchdog"
#define PS_test_qt_rtc "qt_test_rtc"
#define PS_WIFI_CONFIG "wifi_config" 
//#define DIR_OUT_FILE "/tmp/out" 
 //要运行的程序 
#define RUN_applauncher "qtlauncher &"
#define RUN_MediaUI "MediaUI &" 
#define RUN_fltest_qt_backlight "fltest_qt_backlight &"
#define RUN_test_qt_keypad "qt_test_keypad &"
#define RUN_test_qt_ping_test "test_qt_ping_test &"
#define RUN_fltest_qt_qopenglwidget "fltest_qt_qopenglwidget &"
#define RUN_test_qt_watchdog "qt_test_watchdog &"
#define RUN_test_qt_rtc "qt_test_rtc &"
#define RUN_WIFI_CONFIG "wifi_config &"

#define KILL_applauncher "killall qtlauncher"
#define KILL_MediaUI "killall MediaUI " 
#define KILL_fltest_qt_backlight "killall fltest_qt_backlight"
#define KILL_test_qt_keypad "killall qt_test_keypad"
#define KILL_test_qt_ping_test "killall test_qt_ping_test"
#define KILL_fltest_qt_qopenglwidget "killall fltest_qt_qopenglwidget"
#define KILL_test_qt_watchdog "killall qt_test_watchdog"
#define KILL_test_qt_rtc "killall qt_test_rtc"
#define KILLALL "killall MediaUI qt_test_keypad qt_test_watchdog qt_test_rtc"


 #define BUFSZ PIPE_BUF 
 //#define DIR_OUT_FILE "/rf/out" 
 //#define NAME "gnome-keyring" 
 //#define NAME_FIND "gnome" 
 //#define DIR_OUT_FILE "/root/test/out"
int hht_daemon(int nochdir,int noclose) 
{
	pid_t pid; 
	//让init进程成为新产生进程的父进程 
	pid = fork(); 
	//如果创建进程失败 
	if (pid < 0) 
	{ 
		daemon_error("fork error"); 
		return -1; 
	}
	//父进程退出运行 
	if (pid != 0) { 
		exit(0); 
	} 
	//创建新的会话 
	pid = setsid(); 
	if (pid < -1) 
	{ 
		daemon_error("set sid"); 
		return -1; 
	} 
	//更改当前工作目录,将工作目录修改成根目录 
	if (!nochdir) { 
		chdir("/"); 
	}
	//关闭文件描述符，并重定向标准输入，输出合错误输出 
	//将标准输入输出重定向到空设备 
	if (!noclose) 
	{ 
		int fd; 
		fd = open("/dev/null",O_RDWR,0); 
	if (fd != -1) 
	{ 
		dup2(fd,STDIN_FILENO); 
		dup2(fd,STDOUT_FILENO); 
		dup2(fd,STDERR_FILENO); 
		if (fd > 2) { close(fd);
	} } 
	} 
	//设置守护进程的文件权限创建掩码 
	umask(0027); 
	return 0; 
}
int proess_is_exit(char *proess_name) {
	FILE* fp; 
	int count; 
	char buf[BUFSZ]; 
	char command[150];
	int ret=-1;
	sprintf(command, "ps| grep %s | grep -v grep | wc -l", proess_name ); 
	
	if((fp = popen(command,"r")) == NULL) 
	daemon_error("popen eror"); 
	
	if( (fgets(buf,BUFSZ,fp))!= NULL ) 
	{
	count = atoi(buf);
	//daemon_warn("count =%d",count); 
	syslog(LOG_INFO,"count =%d",count);	
	if((count - 1) <  0) {
		//daemon_warn("%s not found\n",proess_name);
		syslog(LOG_INFO,"%s not found\n",proess_name);
		ret = 0;
	} 
	else{
		ret = 1;
		//daemon_warn("process : %s total is %d\n",proess_name,(count - 1));
		syslog(LOG_INFO,"process : %s total is %d\n",proess_name,count);
	} 
	}
	pclose(fp); 
	return ret;
}
int main(int argc,char *argv[]){
	int fd = 0;
	//char buf[100];
	//hht_daemon(0,0);
	bool is_applauncher,is_mediaui,is_backlight,is_keypad,is_ping_test,is_opengl,is_watchdog,is_rtc;
	int need_killall = 1;
	
	while(1){
		//打开日志 
		//openlog(argv[0],LOG_CONS|LOG_PID,LOG_USER); 
		//查看程序是否运行 
		//新建输出文件 
		//system("touch "DIR_OUT_FILE); 
		//获得程序ID 
		//system("ps -w|grep "PS_H5_WS" >> "DIR_OUT_FILE);
		//打开输出文件 
		//fd = open(DIR_OUT_FILE,O_CREAT|O_RDONLY,0777);
		//清空缓存 
		//memset(buf,0,100); 
		//读取全部 
		//read(fd,buf,100); 
		//判断是否有程序文件运行 
		if (proess_is_exit(PS_applauncher)) { 
			is_applauncher = true;
		} else { 
			is_applauncher = false;
		} 
		//sleep(1);
		if (proess_is_exit(PS_MediaUI)) { 
			is_mediaui =true;
		} else { 
			is_mediaui= false;
			//运行程序 
			//system(RUN_SYS_INFO); 
		} 
		//sleep(1);
		if (proess_is_exit(PS_fltest_qt_backlight)) { 
			is_backlight = true;
		} else { 
			is_backlight = false;
		} 
		//sleep(1);
		if (proess_is_exit(PS_test_qt_keypad)) { 
			is_keypad = true;; 
		} else { 
			is_keypad =false;
		}
		//sleep(1);
		if (proess_is_exit(PS_test_qt_ping_test)) { 
			is_ping_test = true;
		} else { 
			is_ping_test = false;
		}
		//sleep(1);
		if (proess_is_exit(PS_fltest_qt_qopenglwidget)) { 
			is_opengl = true;
		} else { 
			is_opengl = false;
		}
		//sleep(1);
		if (proess_is_exit(PS_test_qt_watchdog)) { 
			is_watchdog = true;
		} else { 
			is_watchdog = false;
		}
		//sleep(1);
		if (proess_is_exit(PS_test_qt_rtc)) { 
			is_rtc =true;
		} else { 
			is_rtc= false;
		}
        //daemon_warn("is_applauncher=%d,is_mediaui=%d,is_backlight=%d,is_keypad=%d,is_ping_test=%d,is_opengl=%d,is_watchdog=%d,is_rtc=%d\n",is_applauncher,is_mediaui,is_backlight,is_keypad,is_ping_test,is_opengl,is_watchdog,is_rtc);
		if(is_applauncher && need_killall ==1){
			system(KILLALL);
			daemon_warn("KILL ALL\n");
			need_killall =0;
		}
		if((is_backlight || is_keypad || is_mediaui ||is_opengl || is_ping_test || is_rtc || is_watchdog) && is_applauncher)
		{
			system(KILL_applauncher);
			daemon_warn("KILL applauncher\n");
		}
		if(!is_backlight && !is_keypad && !is_mediaui && !is_opengl && !is_ping_test && !is_rtc && !is_watchdog && !is_applauncher)
		{
			daemon_warn("run applauncher\n");
			system(RUN_applauncher);
			need_killall = 1;
		}
		
		sleep(1);
		//usleep(10*1000);
	}
	return 0;
}
