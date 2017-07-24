
obj-m += machxo2_complete_driver.o

machxo2_complete_driver-objs := machxo2_driver.o 


KDIR := /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx
PWD := $(shell pwd)
MAKE_COMPILER := make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-


all:
	#make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C $(KDIR) M=$(shell pwd) modules
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) modules
clean:
	#make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C $(KDIR) M=$(shell pwd) clean
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) clean
