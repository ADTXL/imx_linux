KERNEL_DIR := $(shell cd ../..;pwd)
CURRENT_PATH := $(shell pwd)

obj-m := led.o

build: kernel_modules

kernel_modules:
	$(MAKE) CFLAGS_MODULE=-fno-pic -C $(KERNEL_DIR) M=$(CURRENT_PATH) modules
ledApp:
	arm-linux-gnueabihf-gcc ledApp.c -o ledApp
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_PATH) clean




