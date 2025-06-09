TARGET := drv

PWD := $(shell pwd)

KERNEL := $(shell uname -r)

obj-m := $(TARGET).o

build:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) modules

info:
	@sudo modinfo $(TARGET).ko

dmsg:
	@sudo dmesg | tail

install:
	@sudo insmod $(TARGET).ko

show:
	@sudo lsmod | grep $(TARGET) || true

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
