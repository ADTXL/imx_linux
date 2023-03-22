#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>



/*
 * 测试timer驱动，
 * 使用方法：./timerApp /dev/timer
 * */

#define CLOSE_CMD   (_IO(0xFF, 0x1))
#define OPEN_CMD   (_IO(0xFF, 0x2))
#define SETPERIOD_CMD   (_IO(0xFF, 0x3))

int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;
    unsigned long cmd;
    unsigned int arg;
    unsigned char str[100];

    if (argc != 2) {
        printf("input the wrong arguments!\n");
        return  -1;
    }

    /* 打开led驱动  */
    filename = argv[1];
    fd = open(filename, O_RDWR);
    if (fd < 0)
        printf("open the file %s error!\n", filename);

    while (1) {
        printf("Input CMD:");
        ret = scanf("%d", &cmd);
        if (ret != 1) {
            gets(str);  // 参数输入错误，防止卡死
        }

        if ( cmd == 1)
            cmd = CLOSE_CMD;
        else if (cmd == 2)
            cmd = OPEN_CMD;
        else if (cmd == 3) {
            cmd = SETPERIOD_CMD;
            printf("Input Timer Period:");
            ret = scanf("%d", &arg);
            if (ret != 1) {
                gets(str);
            }
        }
        ioctl(fd, cmd, arg);
    }


    /* close the device */
    ret = close(fd);
    if (ret < 0) {
        printf("close the %s failed!\n", filename);
        return -1;
    };

    
    return 0;
}


