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
#include <signal.h>



/*
 * 异步通知驱动测试App，
 * 使用方法：./asyncnotiApp /dev/asyncnoti
 * */

static int fd = 0;

/*
 * SIGIO信号处理函数
 * @param - signum  :  信号值
 * @return          :  无
 *
 */
static void sigio_signal_func(int signum)
{
    int err = 0;
    unsigned int keyvalue = 0;

    err = read(fd, &keyvalue, sizeof(keyvalue));
    if(err < 0) {
        // 读取错误
    } else {
        printf("sigio signal! key value = %d\r\n", keyvalue);
    }

}


int main(int argc, char* argv[])
{
    char* filename = NULL;
    int flags = 0, ret = 0;

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

    // 设置信号SIGIO的处理函数
    signal(SIGIO, sigio_signal_func);

    fcntl(fd, F_SETOWN, getpid());      // 将当前进程的进程号告诉内核
    flags = fcntl(fd, F_GETFD);         // 获取当前的进程状态
    fcntl(fd, F_SETFL, flags | FASYNC); // 设置进程启用异步通知功能

    while(1) {
        sleep(2);
    }


    /* close the device */
    ret = close(fd);
    if (ret < 0) {
        printf("close the %s failed!\n", filename);
        return -1;
    };

    
    return 0;
}


