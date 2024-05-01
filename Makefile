#KERNELDIR_LKP ?= /lib/modules/$(shell uname -r)/build
KERNELDIR_LKP ?= ~/Documents/PNL/VM/linux-6.5.7

obj-m += read_and_write.o

all:
	make -C $(KERNELDIR_LKP) M=$$PWD modules

clean:
	make -C $(KERNELDIR_LKP) M=$$PWD clean