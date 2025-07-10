#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int main(int argc, char *argv[]) {
    int fd;
    struct input_event ev;
    ssize_t n;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 打开输入设备文件
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("打开设备文件失败");
        return EXIT_FAILURE;
    }

    printf("开始读取输入事件...\n");

    while (1) {
        // 读取输入事件
        n = read(fd, &ev, sizeof(ev));
        if (n == (ssize_t) -1) {
            perror("读取事件失败");
            close(fd);
            return EXIT_FAILURE;
        }

        // 打印事件信息
        printf("时间戳: %ld.%06ld, 类型: %d, 代码: %d, 值: %d\n",
               ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value);
    }

    close(fd);
    return EXIT_SUCCESS;
}
