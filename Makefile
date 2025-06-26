TARGET := drv

PWD := $(shell pwd)

KERNEL := $(shell uname -r)

obj-m := $(TARGET).o

build:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) modules
	make -C ioctl_test build

info:
	@sudo modinfo $(TARGET).ko

dmsg:
	@sudo dmesg | tail

install:
	@sudo insmod $(TARGET).ko init_shared_vars=4,3,2,1

show:
	@sudo lsmod | grep $(TARGET) || true

uninstall:
	@sudo rmmod $(TARGET)

run_test:
	make -C ioctl_test run

clean:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) clean
	make -C ioctl_test clean

usage:
	@echo Try one of those
	@echo build
	@echo info
	@echo install
	@echo show
	@echo uninstall
	@echo clean
