TARGET := drv

PWD := $(shell pwd)

KERNEL := $(shell uname -r)

obj-m := $(TARGET).o

build:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) modules

info:
	@sudo modinfo $(TARGET).ko

install:
	@sudo insmod $(TARGET).ko

show:
	@sudo lsmod | grep $(TARGET) || true

msg:
	@sudo dmesg

uninstall:
	@sudo rmmod $(TARGET)

clean:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) clean

usage:
	@echo Try one of those
	@echo build
	@echo info
	@echo install
	@echo show
	@echo uninstall
	@echo clean
