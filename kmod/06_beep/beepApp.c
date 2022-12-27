#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>



/*
 * 测试beep驱动，
 * 使用方法：./beepApp /dev/beep 0 // 关闭beep
 *           ./beepApp /dev/beep 1 // 打开beep
 * */

#define BEEP_OFF 0
#define BEEP_ON  1


int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;
    unsigned char databuf[1];

    if (argc != 3) {
        printf("input the wrong arguments!\n");
        return  -1;
    }

    /* 打开beep驱动  */
    filename = argv[1];
    fd = open(filename, O_RDWR);
    if (fd < 0)
        printf("open the file %s error!\n", filename);

    /* 向驱动写入数据 */
    databuf[0] = atoi(argv[2]);
    ret = write(fd, databuf, sizeof(databuf));
    if (ret < 0) {
        printf("write file %s failed!\n", filename);
    } else {
        printf("write data: %s\r\n", databuf);
    }

    /* close the device */
    ret = close(fd);
    if (ret < 0) {
        printf("close the %s failed!\n", filename);
        return -1;
    };

    
    return 0;
}


