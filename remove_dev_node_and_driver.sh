#!/bin/sh

rm -f /dev/MachXo2 

echo "Dev node removed!"

rmmod machxo2_complete_driver.ko

echo "Driver removed!"
