#!/bin/sh

#start copy process

echo "Make loop"
sleep 1

sudo losetup /dev/loop0 /home/daemon-x/Documents/GSoC_testing/GITHUB/apertus_qemu_nonProxy/new_kernel_build_image/beta_20170109.dd

sleep 1

echo "make directory"

sleep 1

sudo mkdir -p /media/beta_opt

sleep 1

echo "mount loop0p3"

sleep 1

sudo mount /dev/loop0p3 /media/beta_opt

sleep 1

echo "clear files"

sleep 1

echo "Delete Files"

sleep 1

echo "Coppy files"

sleep 1

sudo cp /home/daemon-x/Desktop/TestCode/* /media/beta_opt/TUNNEL_EXCHANGE

sleep 2

sudo rm /media/beta_opt/TUNNEL_EXCHANGE/host_file_transfer.sh

sleep 2

cd /home/daemon-x/Desktop/TestCode

sleep 1

echo "Files Coppied!"

sleep 1

sudo umount /media/beta_opt

sleep 2

echo "umounted"

sleep 1

echo "delete mount folder"

sleep 1

sudo rm -r /media/beta_opt

sleep 1

echo "Remove Loop"

sleep 1

sudo losetup -d /dev/loop0

echo "Operation Finished!"
