#!/bin/sh

insmod machxo2_complete_driver.ko

echo "Enter major number: "

read major

mknod /dev/MachXo2 c $major 0

echo "/dev/MachXo2 is created!"



