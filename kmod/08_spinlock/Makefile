KERNEL_DIR := $(shell cd ../..;pwd)

CURRENT_PATH := $(shell pwd)

obj-m := spinlock.o

build: kernel_modules

kernel_modules:
	$(MAKE) CFLAGS_MODULE=-fno-pic -C $(KERNEL_DIR) M=$(CURRENT_PATH) modules
test:
	arm-linux-gnueabihf-gcc spinlockApp.c -o spinlockApp
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_PATH) clean

