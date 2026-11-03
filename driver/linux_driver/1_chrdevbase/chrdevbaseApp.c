/* chrdevbase 测试程序：
 *   ./chrdevbaseApp /dev/chrdevbase 1   读设备
 *   ./chrdevbaseApp /dev/chrdevbase 2   写设备
 */
// #include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sys/types.h"
#include "sys/stat.h"

#define BUFFER_SIZE 100





static char usrdata[] = {"usr data!"}; /* 用户空间数据 */

/*
 * @description : main 主程序
 * @param - argc : argv 数组元素个数
 * @param - argv : 具体参数
 * @return : 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    int fd,retvalue;
    char *filename;
    char readbuf[100],writebuf[100];
    //CHECK argc 是不是 3 main 函数的 argc 参数表示参数数量， argv[]保存着具体的参数，如果参数不为 3 个的话就表示测试 APP 用法错误 
    if (argc != 3) {                    
        printf("error usage!\r\n");
        return -1;
    }

    filename = argv[1];

    /* 打开驱动文件 */
    fd = open(filename, O_RDWR); // 读写模式打开filename文件
    if (fd < 0) {
        printf("Can't open file %s\r\n", filename);
        return -1;
    }
    // 借助 atoi 函数将字符串格式的数字转换为真实的数字类型
    if(atoi(argv[2]) == 1){
        /* 从驱动读取数据 */
        retvalue = read(fd, readbuf, 50); // 从驱动中读取50个字节到readbuf中
        if (retvalue < 0) {
            printf("read file %s failed!\r\n", filename);
        } else {
            /* 读取成功，打印出读取成功的数据 */
            printf("readbuf = %s\r\n", readbuf);
        }
    }
    if(atoi(argv[2]) == 2){
        /* 写入数据到驱动 */
       memcpy(writebuf, usrdata, sizeof(usrdata));
        retvalue = write(fd, writebuf, sizeof(writebuf)); // 将writebuf中的数据写入驱动
        if (retvalue < 0) {
            printf("write file %s failed!\r\n", filename);
        }
    }

    /* 关闭驱动文件 */
    retvalue = close(fd);
    if (retvalue < 0) {
        printf("Can't close file %s\r\n", filename);
        return -1;
    }
  
    return 0;
}














































/* static void usage(const char *program)
{
	printf("用法: %s <设备文件> <操作>\n", program);
	printf("  操作 1: 读设备\n");
	printf("  操作 2: 写设备\n");
}

int main(int argc, char *argv[])
{
	int fd;
	int operation;
	ssize_t count;
	char buffer[BUFFER_SIZE];
	const char *write_data = "chrdevbase test data";

	if (argc != 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	operation = atoi(argv[2]);
	if (operation != 1 && operation != 2) {
		fprintf(stderr, "错误：操作必须为 1（读）或 2（写）\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "打开设备 %s 失败: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	if (operation == 1) {
		memset(buffer, 0, sizeof(buffer));
		count = read(fd, buffer, sizeof(buffer) - 1);
		if (count < 0) {
			fprintf(stderr, "读取设备失败: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
		buffer[count] = '\0';
		printf("读取到 %zd 字节: %s\n", count, buffer);
	} else {
		count = write(fd, write_data, strlen(write_data));
		if (count < 0) {
			fprintf(stderr, "写入设备失败: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
		printf("写入 %zd 字节\n", count);
	}

	close(fd);
	return EXIT_SUCCESS;
}
 */