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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

#define TIMER_NAME "timer"
#define TIMER_CNT 1 // 定时器数量
#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	(_IO(0XEF, 0x3))	/* 设置定时器周期命令 */
#define LEDON 			1		/* 开灯 */
#define LEDOFF 			0		/* 关灯 */
// timer设备结构体
struct timer_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    int major;
    int minor;
    int led_gpio;
    int timeperiod;
    spinlock_t lock; /* 自旋锁 */
    struct timer_list timer;
};

// timer设备实例
struct timer_dev timerdev;

//LED初始化函数
static int led_init(void)
{
    int ret = 0;
    // 获取设备节点
    timerdev.nd = of_find_node_by_path("/gpioled"); // 获取设备树led节点
    if (timerdev.nd == NULL)
    {
        printk("gpioled node can not find!\r\n");
        return -EINVAL;
    }

    // 获取设备树中的 gpio 属性，得到 LED 所使用的 LED 编号
    timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
    if (timerdev.led_gpio < 0)
    {
        printk("can't get led-gpio\r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", timerdev.led_gpio);
    return 0;

    // 初始化led IO
    gpio_request(timerdev.led_gpio, "timer-led"); // 请求GPIO
    if (ret < 0)
    {
        printk("can't request gpio!\r\n");
        return ret;
    }
    ret = gpio_direction_output(timerdev.led_gpio, 1); // 设置 GPIO 输出，并默认关闭 LED
    if (ret < 0)
    {
        gpio_free(timerdev.led_gpio);
        return ret;
    }
}

// 设备操作函数
static int timer_open(struct inode *inode, struct file *file)
{//打开定时器设备程序后
    int ret = 0;
    file->private_data = &timerdev;// 将timerdev结构体的指针赋值给file->private_data

    timerdev.timeperiod = 1000;// 设置定时器时间周期为1000ms秒
    ret = led_init();// 初始化LED
    if (ret < 0)
    {
        return ret;
    }
    return 0;
}
/*函数 timer_unlocked_ioctl，对应应用程序的 ioctl 函数，应用程序调用 ioctl
函数向驱动发送控制信息，此函数响应并执行。此函数有三个参数： filp， cmd 和 arg，其中 fil
p 是对应的设备文件， cmd 是应用程序发送过来的命令信息， arg 是应用程序发送过来的参数，
在本章例程中 arg 参数表示定时周期。*/
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    // struct timer_dev *timer = filp->private_data;
    int timerperiod;
    unsigned long flags;
	
	switch (cmd) {
		case CLOSE_CMD:		/* 关闭定时器 */
			del_timer_sync(&dev->timer);
			break;
		case OPEN_CMD:		/* 打开定时器 */
			spin_lock_irqsave(&dev->lock, flags);
			timerperiod = dev->timeperiod;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
			break;
		case SETPERIOD_CMD: /* 设置定时器周期 */
			spin_lock_irqsave(&dev->lock, flags);
			dev->timeperiod = arg;
			spin_unlock_irqrestore(&dev->lock, flags);
			mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
			break;
		default:
			break;
	}
    return 0;
}

//设备操作函数
static struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.unlocked_ioctl = timer_unlocked_ioctl,
};

// 定时器回调函数,超时了切换LED状态
void timer_function (unsigned long arg)
{
    static int sta = 1; // 定义一个静态变量sta，初始值为1，用于记录LED状态
    struct timer_dev *dev = (struct timer_dev *)arg;
    int timerperiod;
	unsigned long flags;
    sta = !sta; // 切换LED状态
    
    gpio_set_value(dev->led_gpio, sta); // 设置LED状态

    spin_lock_irqsave(&dev->lock, flags);
    timerperiod = dev->timeperiod;
    spin_unlock_irqrestore(&dev->lock, flags);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod)); // 重新设置定时器时间周期
}

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init timer_init(void)
{
	/* 初始化自旋锁 */
	spin_lock_init(&timerdev.lock);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (timerdev.major) {		/*  定义了设备号 */
		timerdev.devid = MKDEV(timerdev.major, 0);
		register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
	} else {						/* 没有定义设备号 */
		alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);	/* 申请设备号 */
		timerdev.major = MAJOR(timerdev.devid);	/* 获取分配号的主设备号 */
		timerdev.minor = MINOR(timerdev.devid);	/* 获取分配号的次设备号 */
	}
	
	/* 2、初始化cdev */
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timer_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);

	/* 4、创建类 */
	timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
	if (IS_ERR(timerdev.class)) {
		return PTR_ERR(timerdev.class);
	}

	/* 5、创建设备 */
	timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
	if (IS_ERR(timerdev.device)) {
		return PTR_ERR(timerdev.device);
	}
	
	/* 6、初始化timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	init_timer(&timerdev.timer);
	timerdev.timer.function = timer_function;
	timerdev.timer.data = (unsigned long)&timerdev;
	return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit timer_exit(void)
{
	
	gpio_set_value(timerdev.led_gpio, 1);	/* 卸载驱动的时候关闭LED */
	del_timer_sync(&timerdev.timer);		/* 删除timer */
#if 0
	del_timer(&timerdev.tiemr);
#endif

	/* 注销字符设备驱动 */
	gpio_free(timerdev.led_gpio);		
	cdev_del(&timerdev.cdev);/*  删除cdev */
	unregister_chrdev_region(timerdev.devid, TIMER_CNT); /* 注销设备号 */

	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");

































// #include <linux/module.h>
// #include <linux/kernel.h>
// #include <linux/init.h>
// #include <linux/timer.h>
// #include <linux/jiffies.h>
// #include <linux/gpio.h>
// #include <linux/of.h>
// #include <linux/of_gpio.h>

// /* Configure led-gpios on the root node, or pass led_gpio=<number>. */
// static int led_gpio = -1;
// module_param(led_gpio, int, 0444);
// MODULE_PARM_DESC(led_gpio, "GPIO number connected to the LED");

// static unsigned int interval_ms = 500;
// module_param(interval_ms, uint, 0644);
// MODULE_PARM_DESC(interval_ms, "LED toggle interval in milliseconds");

// static struct timer_list led_timer;
// static bool led_state;

// static void led_timer_func(struct timer_list *timer)
// {
// 	led_state = !led_state;
// 	gpio_set_value(led_gpio, led_state);
// 	mod_timer(timer, jiffies + msecs_to_jiffies(interval_ms));
// }

// static int __init led_timer_init(void)
// {
// 	struct device_node *root;
// 	int gpio, ret;

// 	root = of_find_node_by_path("/");
// 	if (root) {
// 		gpio = of_get_named_gpio(root, "led-gpios", 0);
// 		if (gpio_is_valid(gpio))
// 			led_gpio = gpio;
// 		of_node_put(root);
// 	}

// 	if (!gpio_is_valid(led_gpio)) {
// 		pr_err("led_timer: set led_gpio or provide root-node led-gpios\n");
// 		return -EINVAL;
// 	}
// 	if (!interval_ms)
// 		interval_ms = 500;

// 	ret = gpio_request(led_gpio, "timer-led");
// 	if (ret)
// 		return ret;
// 	ret = gpio_direction_output(led_gpio, 0);
// 	if (ret) {
// 		gpio_free(led_gpio);
// 		return ret;
// 	}

// 	led_state = false;
// 	timer_setup(&led_timer, led_timer_func, 0);
// 	mod_timer(&led_timer, jiffies + msecs_to_jiffies(interval_ms));
// 	pr_info("led_timer: GPIO %d, interval %u ms\n", led_gpio, interval_ms);
// 	return 0;
// }

// static void __exit led_timer_exit(void)
// {
// 	del_timer_sync(&led_timer);
// 	gpio_set_value(led_gpio, 0);
// 	gpio_free(led_gpio);
// 	pr_info("led_timer: stopped\n");
// }

// module_init(led_timer_init);
// module_exit(led_timer_exit);

// MODULE_LICENSE("GPL");
// MODULE_DESCRIPTION("Timer-driven GPIO LED blink driver");
