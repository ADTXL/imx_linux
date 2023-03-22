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

#define TIMER_CNT    1  // 设备号个数
#define TIMER_NAME   "timer"
#define CLOSE_CMD      (_IO(0xFF, 0x1))  // 关闭定时器
#define OPEN_CMD       (_IO(0xFF, 0x2))  // 打开定时器
#define SETPERIOD_CMD  (_IO(0xFF, 0x3))  // 设置定时器周期命令

#define LED_OFF 0   // close led
#define LED_ON 1    // open led

struct timer_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;
    int led_gpio;   /* led所使用的GPIO编号 */
    int timerperiod;    /* 定时周期，单位为ms  */
    struct timer_list timer;    // 定义一个定时器
    spinlock_t lock;

};

struct timer_dev timerdev;


/*
 * @description    : 初始化LED IO，open函数打开驱动的时候初始化LED灯所使用的的GPIO引脚
 * @param - param  : 无
 * @return         : 0 成功；其他值，失败
 * */
static int led_init(void)
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
      // 1、获取设备节点：timerdev
      timerdev.device_node = of_find_compatible_node(NULL, NULL, "alientek,gpioled");
      if (!timerdev.device_node) {
          printk("find the device_node by compatible is fail!\n");
          return -EINVAL;
      } else {
          printk("find the device_node by compatible is success!\n");
      }   
      // 2、获取设备树中的gpio属性，得到LED所使用的gpio编号
      timerdev.led_gpio = of_get_named_gpio(timerdev.device_node, "led-gpios", 0); 
      if (timerdev.led_gpio < 0) {
          printk("cannot get led-gpio!\n");
          return -EINVAL;
      }   
  
      printk("led-gpio num = %d\n", timerdev.led_gpio);
  
      // 3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭LED灯
      ret = gpio_direction_output(timerdev.led_gpio, 1);
      if (ret < 0) {
          printk("can't set gpio!\n");
      }

    return ret;
}

/*
 * @description    : 打开设备
 * @param - inode  : 传递给驱动的inode
 * @param - filp   : 设备文件，filp结构体有个叫做private_data的成员变量
 *                   一般在open的时候将private_data指向设备结构体
 * @return         : 0 成功；其他值，失败
 * */
static int timer_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &timerdev;

    timerdev.timerperiod = 1000;   // 默认周期为1s
    ret = led_init();
    if (ret < 0)
        return ret;
    printk("led_open!\n");
    return 0;
}

static ssize_t timer_release(struct inode *inode, struct file *filp)
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
static ssize_t timer_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t timer_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description   : ioctl函数
 * @param - filp  : 要打开的设备文件（文件描述符）
 * @param - cmd   : 应用程序发送过来的命令
 * @param - arg   : 参数
 * @param - offt  : 相对于文件首地址的偏移
 * @return        : 0 成功;其它，失败
 * */
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    int timerperiod;
    unsigned long flags;

    switch (cmd) {
    case CLOSE_CMD:
        del_timer_sync(&dev->timer);
        break;
    case OPEN_CMD:
        spin_lock_irqsave(&dev->lock, flags);
        timerperiod = dev->timerperiod;
        spin_unlock_irqrestore(&dev->lock, flags);
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
        break;
    case SETPERIOD_CMD:
        spin_lock_irqsave(&dev->lock, flags);
        dev->timerperiod = arg;
        spin_unlock_irqrestore(&dev->lock, flags);
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
        break;
    default:
        break;
    }
    return 0;
}

static struct file_operations timerdev_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .release = timer_release,
    .read = timer_read,
    .write = timer_write,
    .unlocked_ioctl = timer_unlocked_ioctl,

};

/* 定时器回调函数 */
void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int status = 1;
    int timerperiod;
    unsigned long flags;

    status = !status;
    gpio_set_value(dev->led_gpio, status);

    // 重启定时器
    spin_lock_irqsave(&dev->lock, flags);
    timerperiod = dev->timerperiod;
    spin_unlock_irqrestore(&dev->lock, flags);
    mod_timer(&dev->timer, jiffies +msecs_to_jiffies(dev->timerperiod));
}

static int __init timer_init(void)
{
    // 初始化spinlock
    spin_lock_init(&timerdev.lock);

    /* 注册字符设备驱动 */
    // 1. 创建设备号
    if (timerdev.major) {
        timerdev.devid = MKDEV(timerdev.major, 0);
        register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
    } else {
        alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    printk("timerdev major: %d, minor: %d\n", timerdev.major, timerdev.minor);

    // 2. 初始化cdev
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timerdev_fops);

    // 3. 添加一个cdev
    cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);

    // 4. 创建类
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if (IS_ERR(timerdev.class)) {
        return PTR_ERR(timerdev.class);
    }

    // 5. 创建设备
    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid,
                                     NULL, TIMER_NAME);
    if (IS_ERR(timerdev.device)) {
        return PTR_ERR(timerdev.device);
    }

    // 6. 初始化timer，设置定时器处理函数，还未设置周期，所以不会激活定时器
    init_timer(&timerdev.timer);
    timerdev.timer.function = timer_function;
    timerdev.timer.data = (unsigned long)&timerdev;
    
    return 0;
}

static void __exit timer_exit(void)
{
    // 卸载驱动时关闭LED
    gpio_set_value(timerdev.led_gpio, 1);
    // 删除timer
    del_timer_sync(&timerdev.timer);
#if 0
    del_timer(&timerdev.timer);
#endif
    // 字符设备的注销
    cdev_del(&timerdev.cdev);
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);

    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

