#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: keyApp.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: chrdevbase驱测试APP。
其他	   	: 无
使用方法	 ：./keyApp /dev/key  0 关闭LED
			 ./keyApp /dev/key  1 打开LED
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/1/30 左忠凯创建
***************************************************************/
/* 定义按键值 */
#define KEY0VALUE 0XF0
#define INVAKEY 0X00

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	char *filename;
	int keyvalue;

	if (argc != 2)
	{
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1]; // 设备文件路径

	/* 打开key驱动 */
	fd = open(filename, O_RDWR); // 打开key设备,以读写模式打开,返回文件描述符
	if (fd < 0) // 打开失败
	{
		printf("file %s open failed!\r\n", argv[1]); 
		return -1;
	}
	// 读取按键值
	while (1)
	{	//read()函数的用法：int read(int fd, void *buf, size_t count);
		//参数：
		//fd：文件描述符
		//buf：指向用户空间的缓冲区，用于存储读取到的数据
		//count：要读取的字节数
		//返回值：
		//成功：读取到的字节数
		//失败：-1，表示读取失败
		read(fd, &keyvalue, sizeof(keyvalue)); // 从key设备读取
		if (keyvalue == KEY0VALUE) // 如果按下的是KEY0
		{
			printf("KEY0 Press, value = %#X\r\n", keyvalue);/* 按下 */
		}
	}

	// 关闭文件
	ret = close(fd);
	if (ret < 0)  //如果ret小于零，说明关闭文件失败，大于零说明关闭文件成功
	{
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}
