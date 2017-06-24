#!/bin/sh

#start transfer
echo "Goint to File Transfer mode..."

sleep 1

echo "Going to umount the opt partition!"

sleep 1

echo "Are you sure you want to unmount this partiton?(y/n)"
read RESPONDING

if [ $RESPONDING == "y" || $RESPONDING == "Y" ] ; then
	echo "setting up the paths"
	rm -r /opt/TUNNEL_EXCHANGE
	sleep 1
	mkdir /opt/TUNNEL_EXCHANGE
	sleep 1
	echo "unmounting opt"
	sleep 1
	umount /dev/mmcblk0p3
	sleep 1
	echo "Waiting for files to be coppied"
	sleep 1
	echo "Press Enter after files are coppied"
	read pressEnter
	sleep 1
	echo "opt partition is mounting back"
	sleep 1
	mount /dev/mmcblk0p3 /opt/
	sleep 5
	echo "Partition is mounted!"
	echo "ready to copy the files for your location"
	sleep 5
	echo "deleting present files"
	rm -r /home/TUNNEL_EXCHANGE
	sleep 5
	echo "Files are deleted!"
	echo "copping files"
	sleep 1
	cp -r /opt/TUNNEL_EXCHANGE /home
	sleep 2
	echo "files are coppied!"
	echo "Operation completed!"
else 
	echo "Operation did not pressed!"
fi
