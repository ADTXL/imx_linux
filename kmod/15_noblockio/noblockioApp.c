#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/ioctl.h>



/*
 * 非阻塞访问驱动测试App，
 * 使用方法：./noblockApp /dev/noblockio
 * */



int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;
    unsigned char data;
    struct pollfd fds;
    fd_set readfds;
    struct timeval timeout;

    if (argc != 2) {
        printf("input the wrong arguments!\n");
        return  -1;
    }

    /* 打开key驱动  */
    filename = argv[1];
    fd = open(filename, O_RDWR | O_NONBLOCK);   // 非阻塞式访问
    if (fd < 0) {
        printf("open the file %s error!\n", filename);
        return -1;
    };
#if 0
    fds.fd = fd;
    fds.events = POLLIN;

    while (1) {
        ret = poll(&fds, 1, 500);
        if (ret) {  // 数据有效
            ret = read(fd, &data, sizeof(data));
            if (ret < 0) {
                printf("读取错误\n");
            } else {
                if (data)
                    printf("key value = %d \n", data);
            }
        } else if (ret == 0) {
            printf("用户自定义超时处理\n");
        } else if (ret < 0) {
            printf("用户自定义错误处理\n");
        }
    }
#endif

    /* 循环读取数据 */
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        // 构造超时时间
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;   // 500ms
        ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
        switch (ret) {
            case 0: // 超时
                printf("用户自定义超时处理\n");
                break;
            case 1:
                printf("用户自定义错误处理\n");
                break;
            default:
                if (FD_ISSET(fd, &readfds)) {
                    ret = read(fd, &data, sizeof(data));
                    if (ret < 0) {
                        printf("数据读取错误或者无效\n");
                    } else {
                        if (data)
                            printf("the key0 value is %d\n", data);
                    }
                }

                break;
        }
    }


    /* close the device */
    ret = close(fd);
    if (ret < 0) {
        printf("close the %s failed!\n", filename);
        return -1;
    };

    
    return 0;
}


