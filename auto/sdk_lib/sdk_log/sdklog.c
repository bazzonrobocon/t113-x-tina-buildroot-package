#include "sdklog.h"
#include "stdlib.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#define LOG_BUF_SIZE    1024
#define HAVE_LOCALTIME_R

int sdk_log_setlevel(int prio)
{
    return 0;
}

int sdk_log_print(int prio, int module_prio, const char *log_tag, const char *log_level, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];

    if (prio >= module_prio)
        return 0;

#ifdef HAVE_LOCALTIME_R
    struct tm tmBuf;
    struct tm* ptm;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    ptm = localtime_r(&(tv.tv_sec), &tmBuf);

    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%m-%d %H:%M:%S", ptm);

    int nm = snprintf(buf, LOG_BUF_SIZE, "%s.%03ld %s%s", timeBuf, tv.tv_usec / 1000, log_tag, log_level);
#else
    int nm = snprintf(buf, LOG_BUF_SIZE, "%s%s", log_tag, log_level);
#endif

    va_start(ap, fmt);
    vsnprintf(buf + nm, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);
    int slen = strlen(buf);
    buf[slen] = '\n';
    buf[slen + 1] = 0;
    printf(buf);
    return 0;
}

int sdk_log_print_without_format(const char *log_level, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];
    int nm = snprintf(buf, LOG_BUF_SIZE, "%s", log_level);
    va_start(ap, fmt);
    vsnprintf(buf + nm, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);
    int slen = strlen(buf);
    buf[slen] = '\n';
    buf[slen + 1] = 0;
    printf(buf);
    return 0;
}

void print_stacktrace()
{
    int size = 16;
    void * array[16];
    int stack_num = backtrace(array, size);
    char ** stacktrace = backtrace_symbols(array, stack_num);
    for (int i = 0; i < stack_num; ++i) {
        printf("%s\n", stacktrace[i]);
    }
    free(stacktrace);
}
