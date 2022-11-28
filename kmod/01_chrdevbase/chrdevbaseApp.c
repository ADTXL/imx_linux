#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>



/*
 * 测试chrdevbase驱动，
 * 使用方法：./chrdevbaseApp /dev/chrdevbase <1>/<2>
 * argv[2] 1: 读文件
 * argv[2] 2: 写文件
 * */


static char userdata[] = {"user data!"};
char readbuf[1024];
char  writebuf[1024];


int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;

    if (argc != 3) {
        printf("input the wrong arguments!\n");
        return  -1;
    }

    /* 打开驱动文件  */
    filename = argv[1];
    fd = open(filename, O_RDWR);
    if (fd < 0)
        printf("open the file %s error!\n", filename);

    /* 读驱动文件  */
    if (atoi(argv[2]) == 1)
    {
        ret = read(fd, readbuf, 100);
        if (ret < 0) {
            printf("read file %s failed!\n", filename);
        } else {
            printf("read data: %s\n\r", readbuf);
        }
    }

    /* 写驱动文件 */
    if (atoi(argv[2]) == 2)
    {
        memcpy(writebuf, userdata, sizeof(userdata));
        ret = write(fd, writebuf, 100);
        if (ret < 0) {
            printf("write file %s failed!\n", filename);
        } else {
            printf("write data: %s\n\r", writebuf);
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


