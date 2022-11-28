#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>


#define CHRDEVBASE_MAJOR 200
#define CHRDEVBASE_NAME "chrdevbase"

#define CHRDEVBASE_BUFFER_SIZE 1024

static char readbuf[CHRDEVBASE_BUFFER_SIZE];
static char writebuf[CHRDEVBASE_BUFFER_SIZE];
static char kerneldata[] = {"kernel data!"};



static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    printk("chrdevbase_open!\n");
    return 0;
}

static ssize_t chrdevbase_release(struct inode *inode, struct file *filp)
{
    printk("chrdevbase_release!\n");
    return 0;
}

/*
 * @description   : 从设备读取数据
 * @param - filp  : 要打开的设备文件（文件描述符）
 * @param - buf   : 返回给用户空间的数据缓冲区
 * @param - cnt   : 要读取的数据长度
 * @param - offt  : 相对于文件首地址的偏移
 * @return        : 读取的字节数，如果为负值，表示读取失败
 * */
static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    /* 向用户空间发送数据 */
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    ret = copy_to_user(buf, readbuf, cnt);
    if (ret == 0) {
        printk("kernel send data ok!\n");
    } else {
        printk("kernel send data fail!\n");
    }
    return 0;
}


/*
 * @description   : 从设备写数据
 * @param - filp  : 要打开的设备文件（文件描述符）
 * @param - buf   : 要写给设备的数据
 * @param - cnt   : 要写入的数据长度
 * @param - offt  : 相对于文件首地址的偏移
 * @return        : 读取的字节数，如果为负值，表示读取失败
 * */
static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;

    /* 接收用户空间传递给内核的数据且打印出来 */
    ret = copy_from_user(writebuf, buf, cnt);
    if (ret == 0) {
        printk("kernel recevdata:%s\n", writebuf);
    } else {
        printk("kernel recevdata fail!\n");
    }
    return 0;
}


static struct file_operations chrdevbase_fops = {
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .release = chrdevbase_release,
    .read = chrdevbase_read,
    .write = chrdevbase_write,

};

static int __init chrdevbase_init(void)
{
    int ret = 0;
    // 字符设备的注册
    ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if (ret) {
        printk("the register_chrdev return fail\n");
    }
    printk("chrdevbase_init()\n");
    return 0;
}

static void __exit chrdevbase_exit(void)
{
    // 字符设备的注销
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
}

module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

