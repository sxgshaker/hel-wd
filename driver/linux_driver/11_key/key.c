/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: key.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: Linux按键输入驱动实验
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/7/18 左忠凯创建
***************************************************************/

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

#define KEY_CNT		1		/* 设备号个数 */
#define KEY_NAME	"key"		/* 名字 */
#define KEY0VALUE	0XF0		/* 按键值 */
#define INVAKEY		0X00		/* 无效按键值 */

/**
 * struct key_dev - 按键设备结构体，聚合本驱动所需的全部资源
 * @devid:    设备号（主+次）
 * @cdev:     cdev 结构体，用于注册字符设备
 * @class:    设备类，用于自动在 /sys/class 下创建设备类
 * @device:   设备节点，用于自动在 /dev 下创建设备文件
 * @major:    主设备号（动态申请时为0，申请后回填）
 * @minor:    次设备号
 * @nd:       设备树节点指针（"/key"）
 * @key_gpio: 按键所使用的 GPIO 编号
 * @keyvalue: 原子变量，保存最新按键值（原子操作避免并发读写竞争）
 */
struct key_dev {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *nd;
	int key_gpio;
	atomic_t keyvalue;
};

static struct key_dev keydev;		/* key设备 */

/**
 * keyio_init() - 初始化按键所使用的 GPIO 资源
 *
 * 返回值：0 成功；负值为错误码
 *
 * 流程：
 *   1. of_find_node_by_path("/key") 在设备树里找到按键节点
 *   2. of_get_named_gpio() 从节点取出 "key-gpio" 属性得到 GPIO 编号
 *   3. gpio_request() 向内核申请该 GPIO（防重复申请，必须配对 gpio_free）
 *   4. gpio_direction_input() 设为输入模式
 */
static int keyio_init(void)
{
	keydev.nd = of_find_node_by_path("/key");
	if (keydev.nd == NULL) {
		printk("key node not found!\r\n");
		return -EINVAL;
	}

	keydev.key_gpio = of_get_named_gpio(keydev.nd, "key-gpio", 0);
	if (keydev.key_gpio < 0) {
		printk("can't get key0\r\n");
		return -EINVAL;
	}
	printk("key-gpio num = %d\r\n", keydev.key_gpio);

	#pragma region 用法说明：gpio_request / gpio_direction_input
	/* int gpio_request(unsigned gpio, const char *name)
	 *   申请 GPIO 资源，name 用于标识（如 "key0"）。
	 *   成功返回 0，失败返回负错误码。
	 *   注意：用完后必须 gpio_free() 释放；引脚需在设备树里配置好。
	 * int gpio_direction_input(unsigned gpio)
	 *   把 GPIO 设为输入模式。成功返回 0，失败返回负错误码。
	 * 下面两行调用的参数含义见上方 keyio_init() 注释。 */
	#pragma endregion

	gpio_request(keydev.key_gpio, "key0");		/* 申请GPIO ,命名为key0 */
	gpio_direction_input(keydev.key_gpio);		/* 设置为输入模式 */
	return 0;
}

#pragma region key_open 调用链图解（需要时展开查看）
/* 应用层 open("/dev/key") 到本函数的完整调用路径：
 *
 * 用户空间                          内核空间
 * ─────────                      ─────────
 * keyApp.c
 * fd = open("/dev/key")
 *    │
 *    └─── sys_open 系统调用 ────→ VFS 层
 *                                   │
 *                                   ├─ 根据 /dev/key 找到 cdev
 *                                   ├─ 准备好 inode（含设备号）
 *                                   ├─ 新建一个 struct file（filp）
 *                                   │
 *                                   └─→ 调用 fops->open(inode, filp)
 *                                             │
 *                                             ↓
 *                                        你写的 key_open()   ← 我们在这里
 *                                        {
 *                                          filp->private_data = &keydev;
 *                                          keyio_init();
 *                                        }
 */
#pragma endregion
/**
 * key_open() - 应用层 open() 对应的驱动入口
 * @inode: VFS 传入的 inode（内含设备号）
 * @filp:  本次打开创建的文件对象，其 private_data 用来挂私有数据
 *
 * 返回：0 成功；负值为错误码
 */
static int key_open(struct inode *inode, struct file *filp)
{
	int ret;

	filp->private_data = &keydev;		/* 设置私有数据 */
	ret = keyio_init();			/* 初始化按键所使用的GPIO */
	if (ret < 0)return ret;
    
	return 0;
}

/**
 * key_read() - 应用层 read() 对应的驱动实现（查询式）
 * @filp: 文件对象，private_data 里是 keydev
 * @buf:  用户态缓冲区，用来接收按键值
 * @cnt:  请求读取的字节数
 * @offt: 文件偏移（本驱动忽略）
 *
 * 返回：0 成功；负值表示 copy_to_user 失败
 *
 * @note 当前为查询（轮询）实现：按键松开后才返回一次当前按键值，
 *       应用层 read() 会一直等到按键被松开。中断/等待队列方式见后续实验。
 */
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int value;
	int ret;
	struct key_dev *dev = filp->private_data;

	if (gpio_get_value(dev->key_gpio) == 0) {
		/* 按键按下（低电平），等待松开，消除抖动 */
		while (!gpio_get_value(dev->key_gpio))
			;
		atomic_set(&dev->keyvalue, KEY0VALUE);
	} else {
		atomic_set(&dev->keyvalue, INVAKEY);
	}

	value = atomic_read(&dev->keyvalue);
	ret = copy_to_user(buf, &value, sizeof(value));
	return ret;
}

/**
 * key_write() - 应用层 write() 对应实现
 * @filp: 文件对象
 * @buf:  用户态缓冲区
 * @cnt:  写入字节数
 * @offt: 文件偏移
 *
 * 返回：0（按键为只读设备，不支持写，直接返回成功）
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/**
 * key_release() - 应用层 close() 对应实现
 * @inode: inode
 * @filp:  文件对象
 *
 * 返回：0
 */
static int key_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数集合，把上面几个函数挂到字符设备的文件操作上 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
};

/**
 * mykey_init() - 模块加载入口（insmod 时调用）
 *
 * 返回：0 成功；负值为错误码
 *
 * 框架：申请设备号 → 初始化并添加 cdev → 创建设备类与节点
 */
static int __init mykey_init(void)
{
	atomic_set(&keydev.keyvalue, INVAKEY);	/* 初始按键值为无效值 */

	if (keydev.major) {
		/* 已静态指定主设备号，直接注册 */
		keydev.devid = MKDEV(keydev.major, keydev.minor);
		register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
	} else {
		/* 动态申请设备号，申请后回填 major/minor。
		 * int alloc_chrdev_region(dev_t *dev, unsigned baseminor,
		 *                         unsigned count, const char *name)
		 *   dev:    返回的设备号；baseminor: 起始次设备号；
		 *   count:  数量；name: 设备名。成功返回 0，失败返回负错误码。 */
		alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
		keydev.major = MAJOR(keydev.devid);
		keydev.minor = MINOR(keydev.devid);
	}

	keydev.cdev.owner = THIS_MODULE;
	cdev_init(&keydev.cdev, &key_fops);

	#pragma region 用法说明：cdev_add
	/* int cdev_add(struct cdev *cdev, dev_t dev, unsigned int count)
	 *   把 cdev 加入内核字符设备管理体系；dev 为起始设备号，
	 *   count 为连续设备号个数。成功返回 0，失败返回负错误码。
	 *   调用后 /dev 下才能真正通过本设备号访问驱动。 */
	#pragma endregion
	cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

	keydev.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(keydev.class)) {
		printk("class_create failed\n");
		return PTR_ERR(keydev.class);
	}

	keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
	if (IS_ERR(keydev.device)) {
		printk("device_create failed\n");
		return PTR_ERR(keydev.device);
	}

	return 0;
}

/**
 * mykey_exit() - 模块卸载入口（rmmod 时调用）
 *
 * 注意：与 mykey_init() 的顺序“反着”做注销，避免引用已释放资源。
 */
static void __exit mykey_exit(void)
{
	gpio_free(keydev.key_gpio);
	cdev_del(&keydev.cdev);
	unregister_chrdev_region(keydev.devid, KEY_CNT);

	device_destroy(keydev.class, keydev.devid);
	class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
