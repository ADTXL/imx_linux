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
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>


#define LEDDEV_CNT 1
#define LEDDEV_NAME "platled"

#define LED_OFF 0   // close led
#define LED_ON 1    // open led

/* the virtual address after ioremap */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct leddev_dev{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
};

struct leddev_dev leddev;

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


static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .read = led_read,
    .write = led_write,

};

/*
 * @description  : platform驱动的probe函数，当驱动与设备匹配以后
 *                 此函数就会执行
 * @param  - dev : platform设备
 * @return       : 0, 成功；其他负值，失败
 */
static int led_probe(struct platform_device *dev)
{

    int i = 0;
    int res_size[5];
    u32 val = 0;
    struct resource *led_resource[5];

    printk("led driver and device has matched!\n");

    // 1. 获取资源
    for(i = 0; i < 5; i++) {
        led_resource[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
        if (!led_resource[i]) {
            dev_err(&dev->dev, "No Mem Resource for always on\n");
            return -ENXIO;
        }
        res_size[i] = resource_size(led_resource[i]);
    }

    // init the LED
    // 2、寄存器地址映射
    IMX6U_CCM_CCGR1 = ioremap(led_resource[0]->start, res_size[0]);
    SW_MUX_GPIO1_IO03 = ioremap(led_resource[1]->start, res_size[1]);
    SW_PAD_GPIO1_IO03 = ioremap(led_resource[2]->start, res_size[2]);
    GPIO1_DR = ioremap(led_resource[3]->start, res_size[3]);
    GPIO1_GDIR = ioremap(led_resource[4]->start, res_size[4]);
    
    // 3、使能GPIO时钟
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);  // 清楚以前的设置
    val |= (3 << 26);   // 设置新值
    writel(val, IMX6U_CCM_CCGR1);

    // 4、设置GPIO_IO03的复用功能，将其复用为GPIO1_IO03,最后设置IO属性
    writel(5, SW_PAD_GPIO1_IO03);

    // 寄存器SW_PAD_GPIO1_IO03设置IO属性
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    // 5、设置GPIO1_IO03为输出功能
    val = readl(GPIO1_DR);
    val &= ~(1 << 3);   // 清楚以前的设置
    val |= (1 << 3);    // 设置为输出
    writel(val, GPIO1_GDIR);

    // 6、默认关闭LED
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (leddev.major) {
         leddev.devid = MKDEV(leddev.major, 0); 
         register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);
     } else {
         alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
        leddev.major = MAJOR(leddev.devid);
         leddev.minor = MINOR(leddev.devid);
     }   
     printk("leddev major: %d, minor: %d\n", leddev.major, leddev.minor);
 
     // 2. 初始化cdev
     leddev.cdev.owner = THIS_MODULE;
     cdev_init(&leddev.cdev, &led_fops);
 
     // 3. 添加一个cdev
     cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);
 
     // 4. 创建类
     leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
     if (IS_ERR(leddev.class)) {
         return PTR_ERR(leddev.class);
     }   
 
     // 5. 创建设备
     leddev.device = device_create(leddev.class, NULL, leddev.devid,
                                      NULL, LEDDEV_NAME);
     if (IS_ERR(leddev.device)) {
         return PTR_ERR(leddev.device);
     }

    return 0;   

}


/*
 * @description  : 移除platform驱动的时候此函数会执行
 * @param  - dev : platform设备
 * @return       : 0, 成功；其他负值，失败
 */
static int led_remove(struct platform_device *dev)
{
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    // 字符设备的注销
    cdev_del(&leddev.cdev);
    unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
    device_destroy(leddev.class, leddev.devid);
    class_destroy(leddev.class);
    return 0;

}

static struct platform_driver led_driver = {
    .driver = {
        .name = "imx6ul-led",
    },
    .probe  = led_probe,
    .remove = led_remove,
};

static int __init led_driver_init(void)
{
    return platform_driver_register(&led_driver);
}

static void __exit led_driver_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(led_driver_init);
module_exit(led_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

