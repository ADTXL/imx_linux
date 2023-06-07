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
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#define BEEP_CNT    1  // 设备号个数
#define MISCBEEP_NAME   "miscbeep"
#define MISCBEEP_MINOR  144

#define BEEP_OFF 0   // close beep
#define BEEP_ON 1    // open beep


struct miscbeep_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;
    int beep_gpio;   /* beep所使用的GPIO编号 */

};

struct miscbeep_dev beep;

/*
 * @description i  : 打开设备
 * @param - inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，filp结构体有个叫做private_data的成员变量
 *                   一般在open的时候将private_data指向设备结构体
 * @return         : 0 成功；其他值，失败
 * */
static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &beep;
    printk("beep_open!\n");
    return 0;
}

static ssize_t beep_release(struct inode *inode, struct file *filp)
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
static ssize_t beep_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char beep_stat;
    struct miscbeep_dev *dev = filp->private_data;

    ret = copy_from_user(databuf, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed\n");
        return -EFAULT;
    }

    beep_stat = databuf[0];
    printk("the value of beep_stat is: %d\n", beep_stat);
    if (beep_stat == BEEP_ON) {
        gpio_set_value(dev->beep_gpio, 0);
    } else if (beep_stat == BEEP_OFF) {
        gpio_set_value(dev->beep_gpio, 1);
    }

    return 0;
}


static struct file_operations miscbeep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .release = beep_release,
    .read = beep_read,
    .write = beep_write,

};

static struct miscdevice beep_miscdev = {
    .minor = MISCBEEP_MINOR,
    .name = MISCBEEP_NAME,
    .fops = &miscbeep_fops,
};

static int miscbeep_probe(struct platform_device *dev)
{
    int ret = 0;
#if 0
// 设备树中的beep 设备树节点
        beep{
                compatible = "alientek,beep";
                pinctrl-names = "default";
                pinctrl-0 = <&pinctrl_beep>;
                beep-gpios = <&gpio5 1 GPIO_ACTIVE_HIGH>;
                status = "okay";
        };
#endif
    /* 设置beep所使用的GPIO  */
    // 1、获取设备节点：beep
    beep.device_node = of_find_compatible_node(NULL, NULL, "alientek,beep");
    if (!beep.device_node) {
        printk("find the device_node by compatible is fail!\n");
        return -EINVAL;
    } else {
        printk("find the device_node by compatible is success!\n");
    }
    // 2、获取设备树中的gpio属性，得到beep所使用的gpio编号
    beep.beep_gpio = of_get_named_gpio(beep.device_node, "beep-gpios", 0);
    if (beep.beep_gpio < 0) {
        printk("cannot get beep-gpios!\n");
        return -EINVAL;
    }

    printk("beep-gpio num = %d\n", beep.beep_gpio);

    // 3、设置GPIO5_IO01为输出，并且输出高电平，默认关闭beep
    ret = gpio_direction_output(beep.beep_gpio, 1);
    if (ret < 0) {
        printk("can't set gpio!\n");
    }
    /* 注册misc驱动 */
    ret = misc_register(&beep_miscdev);
    if (ret < 0) {
        printk("misc device register failed!\n");
        return -EFAULT;
    }

    return 0;
#if 0   
    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (beep.major) {
        beep.devid = MKDEV(beep.major, 0);
        register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
    } else {
        alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    printk("beep major: %d, minor: %d\n", beep.major, beep.minor);

    // 2. 初始化cdev
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev, &beep_fops);

    // 3. 添加一个cdev
    cdev_add(&beep.cdev, beep.devid, BEEP_CNT);

    // 4. 创建类
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if (IS_ERR(beep.class)) {
        return PTR_ERR(beep.class);
    }

    // 5. 创建设备
    beep.device = device_create(beep.class, NULL, beep.devid,
                                     NULL, BEEP_NAME);
    if (IS_ERR(beep.device)) {
        return PTR_ERR(beep.device);
    }
#endif
}

static int miscbeep_remove(struct platform_device *dev)
{
    // 注销设备的时候关闭蜂鸣器
    gpio_set_value(beep.beep_gpio, 1);
    gpio_free(beep.beep_gpio);

    misc_deregister(&beep_miscdev);
    return 0;
}


static const struct of_device_id beep_of_match[] = {
    {.compatible = "alientek,beep"},
    {}
};

static struct platform_driver beep_driver = {
    .driver = {
        .name = "imx6ul-beep",
        .of_match_table = beep_of_match,
    },
    .probe = miscbeep_probe,
    .remove = miscbeep_remove,
};

static int __init miscbeep_init(void)
{
    return platform_driver_register(&beep_driver);
}

static void __exit miscbeep_exit(void)
{
    // 字符设备的注销
    cdev_del(&beep.cdev);
    unregister_chrdev_region(beep.devid, BEEP_CNT);

    device_destroy(beep.class, beep.devid);
    class_destroy(beep.class);
    platform_driver_unregister(&beep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

