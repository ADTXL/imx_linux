KERNEL_DIR := /home/user/txl/project/zdyz/imx_linux

CURRENT_PATH := $(shell pwd)

obj-m := chrdevbase.o

build: kernel_modules

kernel_modules:
	$(MAKE) CFLAGS_MODULE=-fno-pic -C $(KERNEL_DIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_PATH) clean




