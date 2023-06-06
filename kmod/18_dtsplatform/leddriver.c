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
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#if 0
// 设备树中的gpioled 设备树节点
        gpioled{
                compatible = "alientek,gpioled";
                pinctrl-names = "default";
                pinctrl-0 = <&pinctrl_gpioled>;
                led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
                status = "okay";
        };
#endif

#define NEWCHRLED_CNT    1  // 设备号个数
#define NEWCHRLED_NAME   "dtsplatled"

#define LED_OFF 0   // close led
#define LED_ON 1    // open led

struct gpioled_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;
    int led_gpio;   /* led所使用的GPIO编号 */

};

struct gpioled_dev gpioled;

/*
 * @description i  : 打开设备
 * @param - inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，filp结构体有个叫做private_data的成员变量
 *                   一般在open的时候将private_data指向设备结构体
 * @return         : 0 成功；其他值，失败
 * */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &gpioled;
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
    struct gpioled_dev *dev = filp->private_data;

    ret = copy_from_user(databuf, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed\n");
        return -EFAULT;
    }

    led_stat = databuf[0];
    printk("the value of led_stat is: %d\n", led_stat);
    if (led_stat == LED_ON) {
        gpio_set_value(dev->led_gpio, 0);
    } else if (led_stat == LED_OFF) {
        gpio_set_value(dev->led_gpio, 1);
    }

    return 0;
}


static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .read = led_read,
    .write = led_write,

};

static int led_probe(struct platform_device *dev)
{
    int ret = 0;
#if 0
// 设备树中的gpioled 设备树节点
        gpioled{
                compatible = "alientek,gpioled";
                pinctrl-names = "default";
                pinctrl-0 = <&pinctrl_gpioled>;
                led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
                status = "okay";
        };
#endif
    /* 设置LED所使用的GPIO  */
    // 1、获取设备节点：gpioled
    gpioled.device_node = of_find_compatible_node(NULL, NULL, "alientek,gpioled");
    if (!gpioled.device_node) {
        printk("find the device_node by compatible is fail!\n");
        return -EINVAL;
    } else {
        printk("find the device_node by compatible is success!\n");
    }
    // 2、获取设备树中的gpio属性，得到LED所使用的gpio编号
    gpioled.led_gpio = of_get_named_gpio(gpioled.device_node, "led-gpios", 0);
    if (gpioled.led_gpio < 0) {
        printk("cannot get led-gpio!\n");
        return -EINVAL;
    }

    printk("led-gpio num = %d\n", gpioled.led_gpio);

    // 3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if (ret < 0) {
        printk("can't set gpio!\n");
    }
    
    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (gpioled.major) {
        gpioled.devid = MKDEV(gpioled.major, 0);
        register_chrdev_region(gpioled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
    } else {
        alloc_chrdev_region(&gpioled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("gpioled major: %d, minor: %d\n", gpioled.major, gpioled.minor);

    // 2. 初始化cdev
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);

    // 3. 添加一个cdev
    cdev_add(&gpioled.cdev, gpioled.devid, NEWCHRLED_CNT);

    // 4. 创建类
    gpioled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(gpioled.class)) {
        return PTR_ERR(gpioled.class);
    }

    // 5. 创建设备
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid,
                                     NULL, NEWCHRLED_NAME);
    if (IS_ERR(gpioled.device)) {
        return PTR_ERR(gpioled.device);
    }
}

static int led_remove(struct platform_device *dev)
{
    // 字符设备的注销
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, NEWCHRLED_CNT);

    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
    {.compatible = "alientek,gpioled"},
    {}
};

/* platform驱动结构体 */
static struct platform_driver led_driver = {
    .driver = {
        .name = "imx6ul-led",
        .of_match_table = led_of_match,
    },
    .probe = led_probe,
    .remove = led_remove,
};


static int __init leddriver_init(void)
{
    return platform_driver_register(&led_driver);
}

static void __exit leddriver_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

