KERNELDIR := /home/tgg/桌面/linux/nxp_linux/alientek_linux_my-change-test/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
CURRENT_PATH := $(shell pwd)
obj-m := chrdevbase.o

# 你当前 shell 没有 export ARCH / CROSS_COMPILE，这里显式指定，
# 否则内核顶层 Makefile 会默认按 x86 编译，报错。
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
