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
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

// 
#define LEDDEV_CNT 			1 /* 设备号长度 */
#define LEDDEV_NAME 	"platled" /* 设备名字 */
#define LEDOFF 				0
#define LEDON 				1
// led设备结构体
struct leddev_dev
{
    dev_t devid;
    struct cdev cdev;		/* cdev		*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备		*/
	int major;				/* 主设备号	*/	
};

struct leddev_dev leddev;   /*LED设备实例*/

// 寄存器名虚拟地址指针
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

void led0_switch(u8 sta)
{
	if(sta ==LEDON)
	{
		// 打开LED灯
		writel(readl(GPIO1_DR) & ~(1<<3), GPIO1_DR);
		// gpiod_set_value(gpioled, 0); // 设置GPIO1_IO03输出低电平，点亮LED
	}
	else if(sta == LEDOFF)
	{
		// 关闭LED灯
		writel(readl(GPIO1_DR) | (1<<3), GPIO1_DR);
	}
}

/**
 * led_open - LED设备的 open 文件操作实现
 * @inode: 指向内核中代表文件 inode 结构的指针，包含文件的基本信息
 * @filp:  指向内核中代表打开文件实例的 file 结构指针
 *
 * 当用户空间应用程序调用 open() 系统调用打开 LED 设备文件（如 /dev/platled）时，
 * 内核会回调此函数。该函数负责将设备结构体实例挂载到 file 结构体的 private_data 字段，
 * 以便后续的 read/write/release 等操作都能通过 filp->private_data 访问到同一个设备实例，
 * 实现多操作间的数据共享。
 *
 * 返回值: 0 表示成功打开设备；负值（如 -EINVAL、-ENODEV）表示打开失败。
 */
static int led_open(struct inode *inode, struct file *filp)
{
	/*
	 * 将全局设备实例 leddev 的地址保存到 filp->private_data。
	 * 这是 Linux 驱动开发中的经典模式：利用 private_data 作为各文件操作间
	 * 传递设备上下文的"桥梁"。这样，在 led_read/led_write/led_release
	 * 中，都可以通过 ((struct leddev_dev *)filp->private_data) 获取到设备。
	 */
	filp->private_data = &leddev;
	return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	// struct leddev_dev *dev = filp->private_data;

	// 从用户空间复制数据到内核空间
	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0) {
		return -EFAULT; // 如果复制失败，返回错误码
	}

	ledstat = databuf[0]; // 获取状态值

	if (ledstat == LEDON) {
		led0_switch(LEDON); // 打开LED灯
	} else if (ledstat == LEDOFF) {
		led0_switch(LEDOFF); // 关闭LED灯
	}
	return 0; // 返回成功
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
};

static int led_probe(struct platform_device *dev)
{
	int i = 0;
	struct resource *ledsource[5];
	// 获取资源
	for(i=0;i<5;i++){
		ledsource[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
		if(ledsource[i] == NULL){
			dev_err(&dev->dev, "Failed to get resource %d\n", i);
			return -ENXIO;
		}

	}
	return 0;
}
