TARGET := drv

PWD := $(shell pwd)

KERNEL := $(shell uname -r)

obj-m := $(TARGET).o

build:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) modules

install:
	sudo insmod $(TARGET).ko

show:
	sudo lsmod | grep $(TARGET) || true

uninstall:
	sudo rmmod $(TARGET)

clean:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) clean
