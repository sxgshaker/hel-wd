KERNELDIR := /home/tgg/桌面/linux/nxp_linux/alientek_linux_my-change-test/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
CURRENT_PATH := $(shell pwd)
obj-m := blockio.o

# 抑制交叉编译器对 #pragma region 的 "ignoring #pragma" 警告
# （region 仅用于 VS Code 折叠注释，不影响代码逻辑）
ccflags-y += -Wno-unknown-pragmas

# 顶层 Makefile 已写死 ARCH / CROSS_COMPILE，这里无需再写；
# 若换到没写死的树，可取消下面两行注释：
# ARCH ?= arm
# CROSS_COMPILE ?= arm-linux-gnueabihf-

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
