#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
#include "linux/poll.h" //#include "poll.h" 有什么区别
#include "sys/select.h"
#include "sys/time.h"
#include "linux/fcntl.h" // 异步通知头文件  新增
#include "signal.h"

/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2024. All rights reserved.
文件名 : asyncnotiApp.c
作者 : 左忠凯
版本 : V1.0
描述 : 定时器测试应用程序
其他 : 无
使用方法 ： ./asyncnotiApp /dev/asyncnoti 打开测试 App
论坛 : www.openedv.com
日志 初版 V1.0 2019/7/26 左忠凯创建
***************************************************************/
static int fd = 0;
// APP里注册信号处理函数
void signal_handler(int signum)
{
    // int fd = 0;
    int err = 0;
    unsigned char key_value = 0;

    err = read(fd, &key_value, sizeof(key_value));
    if (err < 0)
    {
        printf("read failed!\r\n");
    }
    else
    {
        printf("sigio signal! key value=%d\r\n", key_value);
    }
}
int main(int argc, char *argv[])
{
        struct pollfd fds;      // poll结构体
        fd_set readfds;         // 读文件描述符集合
        struct timeval timeout; // 超时时间结构体
        int ret = 0;
        char *filename = NULL;
        unsigned char data = 0;
        int flags = 0;
        // 入口参数判断及检查
        if (argc != 2)
        {
            printf("Usage error!\r\n");
            return -1;
        }
        // 打开设备文件
        filename = argv[1]; // 设备文件名 对应第2个参数，即/dev/imx6uirq
        fd = open(filename, O_RDWR | O_NONBLOCK);
        if (fd < 0)
        {
            printf("open %s failed!\r\n", filename);
            return -1;
        }
    
        signal(SIGIO,signal_handler);       // 注册信号处理函数，指定处理信号SIGIO的函数
        fcntl(fd,F_SETOWN,getpid());        // 告知进程号给内核
        flags = fcntl(fd,F_GETFL);          // 获取当前的进程状态
        fcntl(fd, F_SETFL, flags | FASYNC); /* 设置进程启用异步通知功能 */

        while(1) {
            sleep(2);
        }   

        // 关闭设备文件
        close(fd);
        return ret;
}