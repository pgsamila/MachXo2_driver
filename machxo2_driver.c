/*
 * machxo2_driver.c: Driver for debugging/programming of Machxo2
 *
 * (C) Copyright 2006 - 2017 apertus° community
 * Author: Amila Sampath Pelaketigamage <pgasampath@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3
 * of the License.
 */
#include <linux/fs.h>
//#include <linux/init.h>
//#include <linux/miscdevice.h>
#include <linux/module.h>
//#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
//#include <linux/errno.h>

#include <linux/i2c-dev.h>

#define	DEVICENAME	"MachXo2"
#define	TEXTLENGTH	100

MODULE_LICENSE("FOSS/OH");

/* funcitons */
int machxo2_open(struct inode *inode, struct file *filp);
int machxo2_release(struct inode *inode, struct file *filp);
static ssize_t machxo2_read(struct file *file, char *buf, 
				size_t count, loff_t *ppos);
static ssize_t machxo2_write(struct file *file, const char *buf, 
				size_t count, loff_t *ppos);


/* global variables */
ssize_t machxo2_size;
char machxo2_str[TEXTLENGTH];
int result;
int major;
int minor = 0;
int numofdevice = 1;
dev_t dev;
struct cdev cdev;


/* file operations structure */
static const struct file_operations machxo2_fops = {
	.owner	 	= THIS_MODULE,
	.open	 	= machxo2_open,
	.release	= machxo2_release,
	.read	 	= machxo2_read,
	.write	 	= machxo2_write
};

/* The I2C driver structure */
static struct i2c_device_id machxo2_idtable[] = {
	{"i2c-", 0},
	{},
}

MODULE_DEVICE_TABLE(i2c, machxo2_idtable);

static struct i2c_driver machxo2_i2c_driver = {
	.driver = {
		.name	= DEVICENAME,
		.pm	= &machxo2_pm_ops,	/* optional */
	},

	.id_table	= machxo2_idtable,
	.probe		= machxo2_probe,
	.remove		= machxo2_remove,*/
	/* if device autodetection is needed: */
	.class		= I2C_CLASS_SOMETHING,
	.detect		= machxo2_detect,
	.address_list	= normal_i2c,

	.shutdown	= machxo2_shutdown,	/* optional */
	.command	= machxo2_command,/* optional, deprecated */
}

/*
 * machxo2_init - driver initializing at insmode
 * @func: function to initialize the driver
 */
static int machxo2_init(void)
{

	/* register a range of char device numbers */
	result = alloc_chrdev_region(&dev, minor, numofdevice, DEVICENAME); 
	if(result < 0){
		printk(KERN_ALERT "MachXo2: did not registered\n");
		return result;
	}
	/* get major */
	major = MAJOR(dev);
	printk(KERN_ALERT "MachXo2: registered to major : %d\n",major);

	/* initialize a cdev structure */
	cdev_init(&cdev, &machxo2_fops);

	/* add a char device to the system */
	result = cdev_add(&cdev, dev, numofdevice);
	if(result < 0){
		printk(KERN_ALERT "MachXo2: failed to add cdev\n");
		return result;
	}
	printk(KERN_ALERT "MachXo2: initialization complete\n");
	return 0;
}

/*
 * machxo2_exit - driver removing at rmmode
 * @func: function to remove the driver
 */
static void machxo2_exit(void)
{
	/* remove cdev structure */
	cdev_del(&cdev);

	/* unregister a range of char device numbers */
	unregister_chrdev_region(dev, numofdevice);

	printk(KERN_ALERT "MachXo2: exit called\n");
}

/*
 * machxo2_open - driver opening at runtime
 * @func: function to open the driver
 */
int machxo2_open(struct inode *inode, struct file *filp)
{

        printk(KERN_ALERT "MachXo2: device opened\n");
        return 0;
}

/*
 * machxo2_release - driver releasing at runtime
 * @func: function to release the driver
 */
int machxo2_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "MachXo2: device released\n");
	return 0;
}

/*
 * machxo2_read - driver reading at runtime
 * @func: function to read data from the driver
 * @buf: buffer to fill data
 * @count: user reading data lenght
 * @ppos: position of reading data
 */
static ssize_t machxo2_read(struct file *file, char __user *buf, 
			size_t count, loff_t *ppos)
{
	printk(KERN_ALERT "MachXo2: call for read\n");
	
	/* Get length of saved data */
	machxo2_size = sizeof(machxo2_str);

	/* Check the end position of data */
	if(*ppos >= machxo2_size)
		return 0;

	/* If user try to read more than available data */
	if(*ppos + count > machxo2_size)
		count = machxo2_size - *ppos;

	/* Send data to user space */
	if(copy_to_user(buf, machxo2_str + *ppos, count) != 0)
		return -EFAULT;

	*ppos += count;
	return count;
}

/*
 * machxo2_write - driver writing at runtime
 * @func: function to write data to the driver
 * @buf: buffer to fill data
 * @count: user writing data lenght
 * @ppos: position of writing data
 */
static ssize_t machxo2_write(struct file *file, const char __user *buf, 
			size_t count, loff_t *ppos)
{
	printk(KERN_ALERT "MachXo2: call for write\n");

	/* Check for correct user data */
	if(buf != NULL && count != 0 && count < TEXTLENGTH){

		/* clear current data on driver */
		memset(machxo2_str, 0, sizeof machxo2_str);

		/* send data from user */
	        if(copy_from_user(machxo2_str, buf, count) != 0)
                	return -EINVAL;

		*ppos = count;
		printk(KERN_ALERT "MachXo2: Value written\n");
	 	return count;
	}  
	printk(KERN_ALERT "MachXo2: Value Not written\n");
	return 1;
}

module_init(machxo2_init);
module_exit(machxo2_exit);
