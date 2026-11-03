#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

// 参数定义
#define KEY_NUM 1 /*按键数量*/
#define KEY0_VALUE 0x01
#define IMX6UIRQ_NAME "blockio"
#define IMX6UIRQ_CNT 1
#define INVAKEY 0XFF /* 无效的按键值 */

// 按键结构体
struct irq_keydesc
{
    int gpio;                            /* gpio */
    int irqnum;                          /* 中断号     */
    unsigned char value;                 /* 按键对应的键值 */
    char name[10];                       /* 名字 */
    irqreturn_t (*handler)(int, void *); /* 中断服务函数 */
};
/**
 * imx6uirq设备结构体*/
struct imx6uirq_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    // 业务数据
    struct device_node *nd; // 设备树节点
    int key_gpio;           // 按键gpio号
    int irq;                // 中断号
    struct timer_list timer;
    struct irq_keydesc keydesc[KEY_NUM]; // 按键描述数组
    atomic_t key_value;                  // 按键值
    unsigned char curkeynum;             // 当前按键号
    atomic_t release_key;                // 释放按键标志位

    wait_queue_head_t r_wait;           //等待队列头
};
struct imx6uirq_dev imx6uirq;

// 打开设备
static int imx6uirq_open(struct inode *inode, struct file *file)
{
    file->private_data = &imx6uirq;
    return 0;
}

// 读设备数据到用户空间
static int imx6uirq_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    struct imx6uirq_dev *dev = file->private_data;
    // 从设备结构体中获取按键值
    unsigned char key_value = atomic_read(&dev->key_value);
    // 从设备结构体中获取释放按键标志位
    unsigned char release_key = atomic_read(&dev->release_key);
    // 清空释放按键标志位

#if 0
    // 没按键就睡觉，等wake_up来叫醒
    ret = wait_event_interruptible(dev->r_wait,atomic_read(&dev->release_key));
    if(ret){
        goto wait_error;
    }
#endif
    DECLARE_WAITQUEUE(wait, current);
    if (atomic_read(&dev->release_key) == 0)    //按键没有按下
    {
        add_wait_queue(&dev->r_wait, &wait);    //没按键按下，就把当前等待队列加到等待队列头
        __set_current_state(TASK_INTERRUPTIBLE);/* 设为可中断睡眠 */
        schedule();                             //  主动放弃CPU，睡眠

        /* 进行一次任务切换 */
		if(signal_pending(current))	{			/* 判断是否为信号引起的唤醒 */
			ret = -ERESTARTSYS;
			goto wait_error;
		}
		__set_current_state(TASK_RUNNING);      /* 将当前任务设置为运行状态 */
	    remove_wait_queue(&dev->r_wait, &wait);    /* 将对应的队列项从等待队列头删除 */
    }
    /*唤醒后读取按键值和释放标记*/
    key_value = atomic_read(&dev->key_value);    // 从设备结构体中获取按键值
    release_key = atomic_read(&dev->release_key);    // 从设备结构体中获取释放按键标志位

    if (release_key) // 按键完成一次按下
    {
        if (key_value & 0x80)
        {
            // 清空按键标记
            key_value &= ~0x80;
            // 获取键值到用户空间
            ret = copy_to_user(buf, &key_value, sizeof(key_value));
        }
        else
        { // 按键按下但是没有松开
            goto data_error;
        }
        atomic_set(&dev->release_key, 0);
    }
    else{
        goto data_error;
    }
    return 0;

wait_error:
    // 唤醒后要进入运行态
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&dev->r_wait,&wait);
    return ret;
data_error:
    return -EINVAL;
}

static struct file_operations imx6uirq_fops =
    {
        .owner = THIS_MODULE,
        .open = imx6uirq_open,
        //.release = imx6uirq_release,
        .read = imx6uirq_read,
};

/* 前向声明 */
static irqreturn_t key0_handler(int irq, void *dev_id);
static void timer_function(unsigned long data);

// 写设备树解析和GPIO初始化
static int keyio_init(void)
{
    unsigned char i;
    int ret = 0;
    // 获取设备树节点
    imx6uirq.nd = of_find_node_by_path("/key");
    if (!imx6uirq.nd)
    {
        printk("imx6uirq: of_find_node_by_path() failed!\n");
        return -EINVAL;
    }
    // 获取多个按键gpio号
    for (i = 0; i < KEY_NUM; i++)
    {
        imx6uirq.keydesc[i].gpio = of_get_named_gpio(imx6uirq.nd, "key-gpio", i); // 获取按键gpio号
        if (imx6uirq.keydesc[i].gpio < 0)
        {
            printk("can't get key%d\r\n", i);
        }
    }
    // 初始化按键GPIO
    for (i = 0; i < KEY_NUM; i++)
    {
        memset(imx6uirq.keydesc[i].name, 0, sizeof(imx6uirq.keydesc[i].name)); // 按键名字清空
        sprintf(imx6uirq.keydesc[i].name, "key%d", i);                         // 按键名字赋值key"i",比如key0,key1...
        gpio_request(imx6uirq.keydesc[i].gpio, imx6uirq.keydesc[i].name);      // 请求按键gpio
        gpio_direction_input(imx6uirq.keydesc[i].gpio);                        // 设置为输入方向

        imx6uirq.keydesc[i].irqnum = irq_of_parse_and_map(imx6uirq.nd, 0); // 获取按键中断号
    }

    // key0中断服务函数 先设置handler，再申请中断！
    imx6uirq.keydesc[0].handler = key0_handler;
    imx6uirq.keydesc[0].value = KEY0_VALUE;

    // 申请key按键中断
    for (i = 0; i < KEY_NUM; i++)
    {
        ret = request_irq(imx6uirq.keydesc[i].irqnum,
                          imx6uirq.keydesc[i].handler,
                          IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                          imx6uirq.keydesc[i].name, &imx6uirq);
        if (ret < 0)
        {
            printk("irq %d request failed!\n", imx6uirq.keydesc[i].irqnum);
            return -EFAULT;
        }
    }

    // 初始化定时器
    init_timer(&imx6uirq.timer);
    imx6uirq.timer.function = timer_function;

    // 初始化等待队列头
    init_waitqueue_head(&imx6uirq.r_wait);

    return 0;
}
// key0中断服务函数 中断里不能做耗时操作，只能做简单的处理
//  这里只是启动定时器，定时器中断服务函数里再处理
static irqreturn_t key0_handler(int irq, void *dev_id)
{
    // 按键中断里启动定时器
    struct imx6uirq_dev *dev = dev_id;
    dev->curkeynum = 0; // ← 你漏了这行！
    // 存设备指针到定时器数据
    dev->timer.data = (unsigned long)dev_id;
    // 启动定时器
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10)); // 10ms后触发定时器中断
    // 返回处理结果
    return IRQ_HANDLED;
}

// 定时器回调函数（按键消抖）
static void timer_function(unsigned long data)
{
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)data;
    // 获取当前按键号
    unsigned char num = dev->curkeynum;
    // 获取按键gpio值
    int key_value = gpio_get_value(dev->keydesc[num].gpio);
    // 按下按键时，key_value为0，松开按键时，key_value为1
    if (key_value == 0)
    {
        // 设置设备的按键组num键值
        atomic_set(&dev->key_value, dev->keydesc[num].value);
    }
    else
    { // 按键松开时
        // 设置设备的按键组num键值为0x80|dev->keydesc[num].value
        atomic_set(&dev->key_value, 0x80 | dev->keydesc[num].value);
        atomic_set(&dev->release_key, 1);
    }

    if(atomic_read(&dev->release_key)) // 按键完成一次完整的按下释放，即完成按键过程
    {
        wake_up_interruptible(&dev->r_wait);
    }
}

static int __init imx6uirq_init(void)
{
    /*1.构建设备号*/
    if (imx6uirq.major)
    {
        imx6uirq.devid = MKDEV(imx6uirq.major, 0);                           // 构建设设备号
        register_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT, IMX6UIRQ_NAME); // 注册设备号
    }
    else
    {
        alloc_chrdev_region(&imx6uirq.devid, 0, IMX6UIRQ_CNT, IMX6UIRQ_NAME); // 分配设备号
        imx6uirq.major = MAJOR(imx6uirq.devid);                               // 获取主设备号
        imx6uirq.minor = MINOR(imx6uirq.devid);                               // 获取次设备号
    }

    /*2.注册字符设备*/
    cdev_init(&imx6uirq.cdev, &imx6uirq_fops);              // 初始化字符设备
    cdev_add(&imx6uirq.cdev, imx6uirq.devid, IMX6UIRQ_CNT); // 添加字符设备

    /*3.创建设备类*/
    imx6uirq.class = class_create(THIS_MODULE, IMX6UIRQ_NAME); // 创建类
    if (IS_ERR(imx6uirq.class))
    {
        return PTR_ERR(imx6uirq.class);
    }
    /*4.创建设备*/
    imx6uirq.device = device_create(imx6uirq.class, NULL, imx6uirq.devid, NULL, IMX6UIRQ_NAME); // 创建设备
    if (IS_ERR(imx6uirq.device))
    {
        return PTR_ERR(imx6uirq.device);
    }
    /*5.初始化按键GPIO*/
    atomic_set(&imx6uirq.key_value, 0);
    atomic_set(&imx6uirq.release_key, 0);
    keyio_init();

    return 0;
}

static void __exit imx6uirq_exit(void)
{
    unsigned int i = 0;
    // 删除定时器
    del_timer_sync(&imx6uirq.timer);
    // 释放按键中断
    for (i = 0; i < KEY_NUM; i++)
    {
        free_irq(imx6uirq.keydesc[i].irqnum, &imx6uirq);
        gpio_free(imx6uirq.keydesc[i].gpio);
    }
    // 注销字符设备
    cdev_del(&imx6uirq.cdev);
    unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT); // 注销设备号
    // 注销设备类
    class_destroy(imx6uirq.class);
    // 注销设备
    device_destroy(imx6uirq.class, imx6uirq.devid);
}

module_init(imx6uirq_init);
module_exit(imx6uirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
