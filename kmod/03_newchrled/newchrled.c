#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>

#define NEWCHRLED_CNT    1  // 设备号个数
#define NEWCHRLED_NAME   "newchrled"

#define LED_OFF 0   // close led
#define LED_ON 1    // open led

/* the phy address of reg */
#define CCM_CCGR1_BASE           (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE   (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE   (0x020E02F4)
#define GPIO1_DR_BASE            (0x0209C000)
#define GPIO1_GDIR_BASE          (0x0209C004)

/* the virtual address after ioremap */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct newchrled_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;

};

struct newchrled_dev newchrled;

void led_switch(u8 status)
{
    u32 val = 0;
    if (status == LED_ON)
    {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    } else if (status == LED_OFF) {
        val = readl(GPIO1_DR);
        val |= (1 << 3);
        writel(val, GPIO1_DR);
    }
}
/*
 * @description i  : 打开设备
 * @param - inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，filp结构体有个叫做private_data的成员变量
 *                   一般在open的时候将private_data指向设备结构体
 * @return         : 0 成功；其他值，失败
 * */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    printk("led_open!\n");
    return 0;
}

static ssize_t led_release(struct inode *inode, struct file *filp)
{
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
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char led_stat;

    ret = copy_from_user(databuf, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed\n");
        return -EFAULT;
    }

    led_stat = databuf[0];
    printk("the value of led_stat is: %d\n", led_stat);
    if (led_stat == LED_ON) {
        led_switch(LED_ON);
    } else if (led_stat == LED_OFF) {
        led_switch(LED_OFF);
    }

    return 0;
}


static struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .read = led_read,
    .write = led_write,

};

static int __init led_init(void)
{
    int ret = 0;
    u32 val = 0;

    // init the LED
    // 1、寄存器地址映射
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
    
    // 2、使能GPIO时钟
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);  // 清楚以前的设置
    val |= (3 << 26);   // 设置新值
    writel(val, IMX6U_CCM_CCGR1);

    // 3、设置GPIO_IO03的复用功能，将其复用为GPIO1_IO03,最后设置IO属性
    writel(5, SW_PAD_GPIO1_IO03);

    // 寄存器SW_PAD_GPIO1_IO03设置IO属性
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    // 4、设置GPIO1_IO03为输出功能
    val = readl(GPIO1_DR);
    val &= ~(1 << 3);   // 清楚以前的设置
    val |= (1 << 3);    // 设置为输出
    writel(val, GPIO1_GDIR);

    // 5、默认关闭LED
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (newchrled.major) {
        newchrled.devid = MKDEV(newchrled.major, 0);
        register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
    } else {
        alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    printk("newchrled major: %d, minor: %d\n", newchrled.major, newchrled.minor);

    // 2. 初始化cdev
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &newchrled_fops);

    // 3. 添加一个cdev
    cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);

    // 4. 创建类
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.class)) {
        return PTR_ERR(newchrled.class);
    }

    // 5. 创建设备
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid,
                                     NULL, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.device)) {
        return PTR_ERR(newchrled.device);
    }
    
    return 0;
}

static void __exit led_exit(void)
{
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    // 字符设备的注销
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);

    device_destroy(newchrled.class, newchrled.devid);
    class_destroy(newchrled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

