#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>



/*
 * 测试按键输入驱动，
 * 使用方法：./mykeyApp /dev/led
 * */

#define KEY0VALUE     0xF0
#define INVALIDKEY    0x00


int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;
    unsigned char keyvalue;

    if (argc != 2) {
        printf("input the wrong arguments!\n");
        return  -1;
    }

    /* 打开key驱动  */
    filename = argv[1];
    fd = open(filename, O_RDWR);
    if (fd < 0)
        printf("open the file %s error!\n", filename);

    /* 循环读取数据 */
    while(1) {
        read(fd, &keyvalue, sizeof(keyvalue));
        if (keyvalue == KEY0VALUE)
            printf("the key0 value is %d\n", keyvalue);
    }


    /* close the device */
    ret = close(fd);
    if (ret < 0) {
        printf("close the %s failed!\n", filename);
        return -1;
    };

    
    return 0;
}


