obj-m += machxo2_complete_driver.o
machxo2_complete_driver-objs := machxo2_driver.o machxo2_i2c_driver.o


all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx M=$(shell pwd) modules

clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C /home/daemon-x/Documents/GSoC_testing/GITHUB/linux-xlnx M=$(shell pwd) clean
