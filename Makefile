# Makefile

obj-m += machxo2_complete_driver.o

machxo2_complete_driver-objs := machxo2_driver.o 

# KDIR => Kernel directory path
KDIR := /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx
PWD := $(shell pwd)
# MAKE_COMPILER => available cross compiler
MAKE_COMPILER := make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-

all:
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE_COMPILER) -C $(KDIR) M=$(PWD) clean
