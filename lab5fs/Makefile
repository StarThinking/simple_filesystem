ifneq ($(KERNELRELEASE),) 
obj-m := lab5fs.o
lab5fs-objs := inode.o dir.o file.o
else 

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 

PWD := $(shell pwd)

default: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 
endif 
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
