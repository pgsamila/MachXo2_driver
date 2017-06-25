obj-m += machxo2_driver.o

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx M=$(shell pwd) modules

clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx M=$(shell pwd) clean
