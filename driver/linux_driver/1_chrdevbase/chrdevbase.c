#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define CHRDEVBASE_MAJOR 200 /* 主设备号 */
#define CHRDEVBASE_NAME "chrdevbase"

static char readbuf[100];
static char writebuf[100]; /* 写缓冲区 */
static char kerneldata[] = {"kernel data!"};

/*
 * @description : 打开设备
 * @param – inode : 传递给驱动的 inode
 * @param - filp : 设备文件， file 结构体有个叫做 private_data 的成员变量
 * 一般在 open 的时候将 private_data 指向设备结构体。
 *  * @return : 0 成功;其他 失败
 */

/* 打开设备 */
static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    /* 用户实现具体功能 */
    return 0;
}

static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    retvalue = copy_to_user(buf, readbuf, cnt); // 通过函数 copy_to_user 将readbuf 中的数据复制到参数 buf 中
    if (retvalue == 0)
    {
        printk("copy_to_user success\n");
    }
    else
    {
        printk("copy_to_user failed\n");
    }

    /* 用户实现具体功能 */
    return 0;
}

static ssize_t chrdevbase_write(struct file *filp,
                                const char __user *buf,
                                size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    /* 接收用户空间传递给内核的数据并且打印出来 */
    retvalue = copy_from_user(writebuf, buf, cnt);
    if (retvalue == 0)
    {
        printk("kernel recevdata:%s\r\n", writebuf);
    }
    else
    {
        printk("kernel recevdata failed!\r\n");
    }

    // printk("chrdevbase write!\r\n");
    return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{
    // printk("chrdevbase release！ \r\n");
    return 0;
}

/* 设备操作函数结构体 */

static struct file_operations chrdevbase_fops = {
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .read = chrdevbase_read,
    .write = chrdevbase_write,
    .release = chrdevbase_release,
};

static int __init chrdevbase_init(void)
{
    int retvalue = 0;
    /* 注册设备 */
    retvalue = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if (retvalue < 0)
    {
        printk("register_chrdev failed, retvalue = %d\n", retvalue);
        return retvalue;
    }
    printk("register_chrdev success, retvalue = %d\n", retvalue);
    return 0;
}

static void __exit chrdevbase_exit(void)
{
    /* 注销设备 */
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
    printk("chrdevbase_exit()\r\n");
}

/* 将上面两个函数指定为驱动的入口和出口函数 */
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxx");
