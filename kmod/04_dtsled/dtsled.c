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

#if 0
// 设备树中的led 设备树节点
        alphaled {
                #address-cells = <1>;
                #size-cells = <1>;
                compatible = "alientek,alphaled";
                status = "okay";
                reg = < 0X020C406C 0x04 /* CCM_CCGR1_BASE */
                                0X020E0068 0X04 /* SW_MUX_GPIO1_IO03_BASE */
                                0X020E02F4 0X04 /* SW_PAD_GPIO1_IO03_BASE */
                                0X0209C000 0X04 /* GPIO1_DR_BASE */
                                0X0209C004 0X04>; /* GPIO1_GDIR_BASE */
        };  
#endif

#define NEWCHRLED_CNT    1  // 设备号个数
#define NEWCHRLED_NAME   "dtsled"

#define LED_OFF 0   // close led
#define LED_ON 1    // open led

/* the phy addredss of reg */
// #define CCM_CCGR1_BASE           (0x020C406C)
// #define SW_MUX_GPIO1_IO03_BASE   (0x020E0068)
// #define SW_PAD_GPIO1_IO03_BASE   (0x020E02F4)
// #define GPIO1_DR_BASE            (0x0209C000)
// #define GPIO1_GDIR_BASE          (0x0209C004)

/* the virtual address after ioremap */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct dtsled_dev {
    dev_t devid;    // device id
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *device_node;
    int major;
    int minor;

};

struct dtsled_dev dtsled;

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
    filp->private_data = &dtsled;
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


static struct file_operations dtsled_fops = {
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
    u32 reg_data[14];
#if 0
// 设备树中的led 设备树节点
        alphaled {
                #address-cells = <1>;
                #size-cells = <1>;
                compatible = "alientek,alphaled";
                status = "okay";
                reg = < 0X020C406C 0x04 /* CCM_CCGR1_BASE */
                                0X020E0068 0X04 /* SW_MUX_GPIO1_IO03_BASE */
                                0X020E02F4 0X04 /* SW_PAD_GPIO1_IO03_BASE */
                                0X0209C000 0X04 /* GPIO1_DR_BASE */
                                0X0209C004 0X04>; /* GPIO1_GDIR_BASE */
        };  
#endif
    // 0、从设备树中获取寄存器地址
    // 根据device_type和compatible这两个属性查找指定的节点
    dtsled.device_node = of_find_compatible_node(NULL, NULL, "alientek,alphaled");
    if (!dtsled.device_node) {
        printk("find the device_node by compatible is fail!\n");
        return -EINVAL;
    } else {
        printk("find the device_node by compatible is success!\n");
    }
    // 获取reg属性中的地址，以数组的形式保存
    ret = of_property_read_u32_array(dtsled.device_node, "reg", reg_data, 10);
    if (ret < 0) {
        printk("reg property read failed!\n");
    } else {
        u8 i = 0;
        printk("reg data:\n");
        for (i = 0; i < 10; i++)
            printk("%#X ", reg_data[i]);
        printk("\n");
    }
    
    // init the LED
    // 1、寄存器地址映射
#if 0
    IMX6U_CCM_CCGR1 = ioremap(reg_data[0], reg_data[1]);
    SW_MUX_GPIO1_IO03 = ioremap(reg_data[2], reg_data[3]);
    SW_PAD_GPIO1_IO03 = ioremap(reg_data[4], reg_data[5]);
    GPIO1_DR = ioremap(reg_data[6], reg_data[7]);
    GPIO1_GDIR = ioremap(reg_data[8], reg_data[9]);
#else
    IMX6U_CCM_CCGR1 = of_iomap(dtsled.device_node, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.device_node, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.device_node, 2);
    GPIO1_DR = of_iomap(dtsled.device_node, 3);
    GPIO1_GDIR = of_iomap(dtsled.device_node, 4);
#endif

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
    if (dtsled.major) {
        dtsled.devid = MKDEV(dtsled.major, 0);
        register_chrdev_region(dtsled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
    } else {
        alloc_chrdev_region(&dtsled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    printk("dtsled major: %d, minor: %d\n", dtsled.major, dtsled.minor);

    // 2. 初始化cdev
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

    // 3. 添加一个cdev
    cdev_add(&dtsled.cdev, dtsled.devid, NEWCHRLED_CNT);

    // 4. 创建类
    dtsled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(dtsled.class)) {
        return PTR_ERR(dtsled.class);
    }

    // 5. 创建设备
    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid,
                                     NULL, NEWCHRLED_NAME);
    if (IS_ERR(dtsled.device)) {
        return PTR_ERR(dtsled.device);
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
    cdev_del(&dtsled.cdev);
    unregister_chrdev_region(dtsled.devid, NEWCHRLED_CNT);

    device_destroy(dtsled.class, dtsled.devid);
    class_destroy(dtsled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("adtxl");

