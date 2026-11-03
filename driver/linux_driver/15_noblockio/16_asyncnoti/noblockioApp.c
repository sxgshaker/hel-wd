#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
#include "linux/poll.h"
#include "sys/select.h"

/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2024. All rights reserved.
文件名 : imx6uirqApp.c
作者 : 左忠凯
版本 : V1.0
描述 : 定时器测试应用程序
其他 : 无
使用方法 ： ./imx6uirqApp /dev/imx6uirq 打开测试 App
论坛 : www.openedv.com
日志 初版 V1.0 2019/7/26 左忠凯创建
***************************************************************/

int main(int argc, char *argv[])
{
    struct pollfd fds;  // poll结构体
    fd_set readfds;     // 读文件描述符集合
    struct timeval timeout; // 超时时间结构体
    int ret = 0;
    char * filename = NULL;
    unsigned char data = 0;
    //入口参数判断及检查
    if(argc !=2){
        printf("Usage error!\r\n");
        return -1;
    }
    // 打开设备文件
    filename = argv[1]; // 设备文件名 对应第2个参数，即/dev/imx6uirq
    int fd = open(filename, O_RDWR|O_NONBLOCK);
    if(fd < 0){
        printf("open %s failed!\r\n", filename);
        return -1;
    }
    // 不断获取设备文件内信息
#if 0
    // 非阻塞IO Poll
    fds.fd = fd;
    fds.events = POLLIN;
    while(1){
        ret = poll(&fds,1,-1);
        if(ret){ // 有数据可读
            ret = read(fd,&data,sizeof(data));
            if(ret < 0){
                break;
            }
            else{
                if(data){
                    printf("key value = %#x\r\n",data);
                }
            }
        }else if(ret == 0){ // 超时，无数据可读
            printf("poll timeout!\r\n");
        }else{ // poll失败
            printf("poll failed!\r\n");
            break;
        }
    }

#endif
    while(1){
        FD_ZERO(&readfds); // 清空读文件描述符集合
        FD_SET(fd,&readfds); // 将fd添加到读文件描述符集合中
        timeout.tv_sec = 0;  // 超时时间 0秒
        timeout.tv_usec = 500000; // 超时时间 500ms
        ret = select(fd+1,&readfds,NULL,NULL,&timeout); // 等待读文件描述符集合中的fd就绪
        switch(ret){
            case -1:
                printf("select failed!\r\n");
                break;
            case 0:
                printf("select timeout!\r\n");
                break;
            default: // 有数据可读
                if(FD_ISSET(fd,&readfds)){
                    ret = read(fd,&data,sizeof(data));
                        if(ret < 0){
                            break;
                        }
                        else{
                            if(data){
                                printf("key value = %#x\r\n",data);
                            }
                        }
                break;
                }
        }
    }
    // 关闭设备文件
    close(fd);
    return ret;
}