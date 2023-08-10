#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>


/*
 * 测试按键输入驱动--irq，
 * 使用方法：./keyinputApp /dev/input/event1
 */


/* 定义一个input_event变量，存放输入事件信息 */
static struct input_event inputevent;

int main(int argc, char* argv[])
{
    char* filename = NULL;
    int fd = 0, ret = 0;

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
        ret = read(fd, &inputevent, sizeof(inputevent));
        if (ret > 0) {
            printf("数据读取success\n");
	    switch (inputevent.type) {
	    	case EV_KEY:
			if (inputevent.code < BTN_MISC) { /* 键盘键值 */
				printf("key %d %s \n", inputevent.code,
					inputevent.value ? "press" : "release");
			} else {
				printf("button %d %s\n", inputevent.code,
					inputevent.value ? "press" : "release");
			}
			break;
		case EV_REL:
			break;
		case EV_ABS:
			break;
		case EV_MSC:
			break;
		case EV_SW:
			break;
	    }
        } else {
            printf("数据读取错误或者无效\n");
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


