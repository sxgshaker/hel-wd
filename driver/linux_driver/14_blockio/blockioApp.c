#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

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
    int fd = open(filename, O_RDWR);
    if(fd < 0){
        printf("open %s failed!\r\n", filename);
        return -1;
    }
    // 不断获取设备文件内信息
    
    while(1){
        ret =read(fd,&data,sizeof(data));
        if(ret < 0){
            continue;
        }
        else{
            if(data){
                printf("key value = %#x\r\n",data);
            }
        }
    }
    // 关闭设备文件
    close(fd);
    return ret;
}