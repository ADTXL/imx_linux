/*
 * input驱动实验
 * */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>


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

#define KEYINPUT_CNT    1  // 设备号个数
#define KEYINPUT_NAME   "keyinput"

// 定义按键值
#define KEY0VALUE    0x01   // 按键值
#define INVALIDKEY   0xFF   // 无效的按键值
#define KEY_NUM      1      // 按键数量

// 中断IO描述结构体
struct irq_keydesc {
    int gpio;
    int irq_num;
    unsigned char value;    // 按键对应的键值
    char name[10];
    irqreturn_t (*handler)(int, void *);    // 中断服务函数
};

struct keyinput_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;
    atomic_t keyvalue;  // 按键值
    atomic_t releasekey;    // 标记是否完成一次完成的按键
    struct timer_list timer;
    struct irq_keydesc irqkeydesc[KEY_NUM]; // 按键描述数组
    unsigned char current_key_num;  // 当前的按键号
    struct input_dev *inputdev;

};

struct keyinput_dev keyinputdev;

/*
 * @description    : 中断服务函数，开启定时器，延时10ms
 *                  定时器用于按键消抖
 * @param - irq    : 中断号
 * @param - arg    : 设备结构
 * @return         : 中断执行结果
 * */
static irqreturn_t key0_handler(int irq, void *dev_id)
{
    struct keyinput_dev *dev = (struct keyinput_dev *)dev_id;
    
    dev->current_key_num = 0;
    dev->timer.data = (volatile long)dev_id;
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
    return IRQ_RETVAL(IRQ_HANDLED);
}


/*
 * @description    : 定时器服务函数，用于按键消抖，定时器到了以后
 *                  再次读取按键值，如果按键还是处于按下状态就表示按键有效
 * @param - arg  : 设备结构变量
 * @return         : 无
 * */
void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct keyinput_dev *dev = (struct keyinput_dev *)arg;

    num = dev->current_key_num;
    keydesc = &dev->irqkeydesc[num];
    value = gpio_get_value(keydesc->gpio);
    if (value == 0){    // 按下按键
	// 上报按键值
	input_report_key(dev->inputdev, keydesc->value, 1); // 按下
	input_sync(dev->inputdev);
    } else {        // 按键松开
	input_report_key(dev->inputdev, keydesc->value, 0);
	input_sync(dev->inputdev);
    }
}

/*
 * @description    : 按键IO初始化
 * @param - inode  : 无
 * @return         : 0 成功；其他值，失败
 * */
static int keyio_init(void)
{
    unsigned char i = 0;
    int ret = 0;

    keyinputdev.device_node = of_find_node_by_path("/key");
    if (keyinputdev.device_node == NULL) {
	printk("key node not find!\n");
        return -EINVAL;
    }

    // 提取GPIO
    for (i = 0; i < KEY_NUM; i++) {
        keyinputdev.irqkeydesc[i].gpio = of_get_named_gpio(keyinputdev.device_node, "key-gpios", 0);
        if (keyinputdev.irqkeydesc[i].gpio < 0) {
            printk("can't get key-gpios\n");
            return -EINVAL;
        }
    }

    // 初始化key所使用的IO,并设置成中断模式
    for (i= 0; i < KEY_NUM; i++) {
        memset(keyinputdev.irqkeydesc[i].name, 0, sizeof(keyinputdev.irqkeydesc[i].name));
        sprintf(keyinputdev.irqkeydesc[i].name, "KEY%d", i);
        gpio_request(keyinputdev.irqkeydesc[i].gpio, keyinputdev.irqkeydesc[i].name);
        gpio_direction_input(keyinputdev.irqkeydesc[i].gpio);  // 设置为输入
        keyinputdev.irqkeydesc[i].irq_num = irq_of_parse_and_map(keyinputdev.device_node, i);
#if 0
        keyinputdev.irqkeydesc[i].irq_num = gpio_to_irq(keyinputdev.irqkeydesc[i].gpio);
#endif
        printk("key%d:gpio=%d, irqnum=%d\n", i, keyinputdev.irqkeydesc[i].gpio, keyinputdev.irqkeydesc[i].irq_num);

    }

    // 申请中断
    keyinputdev.irqkeydesc[0].handler = key0_handler;
    keyinputdev.irqkeydesc[0].value = KEY0VALUE;

    for (i = 0; i < KEY_NUM; i++) {
        ret = request_irq(keyinputdev.irqkeydesc[i].irq_num,
                          keyinputdev.irqkeydesc[i].handler,
                          IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                          keyinputdev.irqkeydesc[i].name,
                          &keyinputdev);
        if (ret <0) {
            printk("irq %d request failed!\n", keyinputdev.irqkeydesc[i].irq_num);
            return -EFAULT;
        }
    }

    // 创建定时器
    init_timer(&keyinputdev.timer);
    keyinputdev.timer.function = timer_function;

    // 申请input_dev
    keyinputdev.inputdev = input_allocate_device();
    keyinputdev.inputdev->name = KEYINPUT_NAME;

    keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_0);

    // 注册输入设备
    ret = input_register_device(keyinputdev.inputdev);
    if (ret) {
    	printk("register input device failed!\n");
    }
    return 0;    
}

static int __init keyinput_init(void)
{
	keyio_init();
	return 0;
}

static void __exit keyinput_exit(void)
{
    unsigned int i = 0;

    // 删除定时器
    del_timer_sync(&keyinputdev.timer);

    // 释放中断
    for (i = 0; i < KEY_NUM; i++) {
        free_irq(keyinputdev.irqkeydesc[i].irq_num, &keyinputdev);
    }
    
    // 释放IO
    for (i = 0; i < KEY_NUM; i++){
    	gpio_free(keyinputdev.irqkeydesc[i].gpio);
    }

    // 释放input_dev
    input_unregister_device(keyinputdev.inputdev);
    input_free_device(keyinputdev.inputdev);
}

module_init(keyinput_init);
module_exit(keyinput_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

