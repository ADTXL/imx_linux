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

#if 0
// 设备树中的key设备树节点
        key{
                compatible = "alientek,key";
                pinctrl-names = "default";
                pinctrl-0 = <&pinctrl_key>;
                key-gpios = <&gpio1 18 GPIO_ACTIVE_LOW>;    /* KEY0  */
                status = "okay";
                interrupt-parent = <&gpio1>;
                interrupts = <18 IRQ_TYPE_EDGE_BOTH>;
        };

#endif

#define KEY_CNT    1  // 设备号个数
#define KEY_NAME   "key"

// 定义按键值
#define KEY0VALUE    0xF0   // 按键值
#define INVALIDKEY   0x00   // 无效的按键值

struct key_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;
    int key_gpio;   /* key所使用的GPIO编号 */
    atomic_t keyvalue;  // 按键值

};

struct key_dev keydev;

/*
 * @description    : open的时候初始化按键所使用的GPIO引脚
 * @param - inode  : 无
 * @return         : 0 成功；其他值，失败
 * */
static int mykey_gpio_init(void)
{
    keydev.device_node = of_find_node_by_path("/key");
    if (keydev.device_node == NULL) {
        return -EINVAL;
    }

    keydev.key_gpio = of_get_named_gpio(keydev.device_node, "key-gpios", 0);
    if (keydev.key_gpio < 0) {
        printk("can't get key-gpios\n");
        return -EINVAL;
    }
    printk("key_gpio=%d\n", keydev.key_gpio);

    // 初始化key所使用的IO
    gpio_request(keydev.key_gpio, "key0"); // 申请一个gpio,name为“key0”
    gpio_direction_input(keydev.key_gpio);  // 设置为输入
 
    return 0;    
}

/*
 * @description    : 打开设备
 * @param - inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，filp结构体有个叫做private_data的成员变量
 *                   一般在open的时候将private_data指向设备结构体
 * @return         : 0 成功；其他值，失败
 * */
static int mykey_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &keydev;
    ret = mykey_gpio_init();  // 初始化按键IO
    if (ret < 0) {
        return ret;
    }
    printk("mykey_open!\n");
    return 0;
}

static ssize_t mykey_release(struct inode *inode, struct file *filp)
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
static ssize_t mykey_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    unsigned char value;
    struct key_dev *dev = filp->private_data;

    if (gpio_get_value(dev->key_gpio) == 0) {   // 按键按下
        while(!gpio_get_value(dev->key_gpio));  // 等待释放
        atomic_set(&dev->keyvalue, KEY0VALUE);  // 释放后结束while循环，设置按键值
    } else {                // 无效的按键值
        atomic_set(&dev->keyvalue, INVALIDKEY);
    }

    value = atomic_read(&dev->keyvalue);    // 保存按键值
    ret = copy_to_user(buf, &value, sizeof(value));
    return ret;
}


/*
 * @description   : 从设备写数据
 * @param - filp  : 要打开的设备文件（文件描述符）
 * @param - buf   : 要写给设备的数据
 * @param - cnt   : 要写入的数据长度
 * @param - offt  : 相对于文件首地址的偏移
 * @return        : 读取的字节数，如果为负值，表示读取失败
 * */
static ssize_t mykey_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}


static struct file_operations mykey_fops = {
    .owner = THIS_MODULE,
    .open = mykey_open,
    .release = mykey_release,
    .read = mykey_read,
    .write = mykey_write,

};

static int __init mykey_init(void)
{
    // 初始化原子变量
    atomic_set(&keydev.keyvalue, INVALIDKEY); 

    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (keydev.major) {
        keydev.devid = MKDEV(keydev.major, 0);
        register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
    } else {
        alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
        keydev.major = MAJOR(keydev.devid);
        keydev.minor = MINOR(keydev.devid);
    }
    printk("keydev major: %d, minor: %d\n", keydev.major, keydev.minor);

    // 2. 初始化cdev
    keydev.cdev.owner = THIS_MODULE;
    cdev_init(&keydev.cdev, &mykey_fops);

    // 3. 添加一个cdev
    cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

    // 4. 创建类
    keydev.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(keydev.class)) {
        return PTR_ERR(keydev.class);
    }

    // 5. 创建设备
    keydev.device = device_create(keydev.class, NULL, keydev.devid,
                                     NULL, KEY_NAME);
    if (IS_ERR(keydev.device)) {
        return PTR_ERR(keydev.device);
    }
    
    return 0;
}

static void __exit mykey_exit(void)
{
    // 字符设备的注销
    cdev_del(&keydev.cdev);
    unregister_chrdev_region(keydev.devid, KEY_CNT);

    device_destroy(keydev.class, keydev.devid);
    class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

