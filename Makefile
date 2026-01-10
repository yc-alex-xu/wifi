.PHONY: driver client test clean
# 内核源码路径（自动匹配当前系统内核）
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# 当前工作目录
PWD := $(shell pwd)
# 要编译的模块名（和.c文件名一致）
obj-m += wifi_driver.o

# 编译指令
driver:
	@echo "Compiling WiFi driver..."
	make -C $(KERNELDIR) M=$(PWD) modules
	@echo "Compile success! Generated: wifi_driver.ko"
client:
	@echo "Compiling WiFi client application..."
	gcc -o wifi_client wifi_client.c
	@echo "Compile success! Generated: wifi_client"
test:
	@echo "Testing WiFi driver..."
	sudo insmod wifi_driver.ko
	dmesg | tail -n 10
	sudo rmmod wifi_driver
	@echo "Test completed!"
# 清理编译产物
clean:
	@echo "Cleaning compile files..."
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions modules.order Module.symvers wifi_client
	@echo "Clean success!"


