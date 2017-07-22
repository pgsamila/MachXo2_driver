
obj-m += machxo2_complete_driver.o

machxo2_complete_driver-objs := machxo2_driver.o 

#path to linux-xlnx kernel
KDIR := ../linux-xlnx

PWD := $(shell pwd)

#define the cross compiler
MAKE_COMPILER := make ARCH=arm CROSS_COMPILE=arm-linux-gnu-


all:
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) clean
