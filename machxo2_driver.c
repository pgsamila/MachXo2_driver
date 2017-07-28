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

#include <asm/segment.h>
#include <asm/uaccess.h>

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

/*#include "machxo2_i2c_driver.h"*/

/*comment next line to deactivate code debugging mode*/
//#define TURN_ON_DEBUG

#undef MDEBUG
#ifdef TURN_ON_DEBUG
#	define	MDEBUG(msg, args...)		printk(KERN_NOTICE "MachXo2: " msg, ## args)
#else
#	define	MDEBUG(msg, args...)		do { } while (0)
#endif

#define	MACHXO2_NAME			"machxo2"

#define	DEVICENAME			"machxo2"
#define	TEXTLENGTH			100

#define	MACHXO2_REG_CONF		0x46
#define	MACHXO2_SHUTDOWN		0x01
#define	MACHXO2_I2C_ADDRESS		0x48	/*Error on creating i2c*/
#define	MACHXO2_I2C_ID			0	/*dev/i2c-x ; enter x value*/
#define	MACHXO2_REG_CTRL_REG1		0x20
#define	MACHXO2_REG_STATUS_REG		0x46
#define	MACHXO_READ_WAIT_TIME_US	100
#define	MACHXO2_READ_TIMEOUT_US		3000000

#define	MAJOR_NUM			100

/*funcitons*/
static int machxo2_init(void);
static void machxo2_exit(void);
int machxo2_open(struct inode *inode, struct file *filp);
int machxo2_release(struct inode *inode, struct file *filp);
static ssize_t machxo2_read(struct file *file, char *buf, size_t count,
								loff_t *ppos);
static ssize_t machxo2_write(struct file *file, const char *buf, size_t count,
								loff_t *ppos);
static int machxo2_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id);
static int machxo2_i2c_remove(struct i2c_client *client);
int machxo2_set_register(char reg, char val);
int machxo2_get_register(char reg, char *val);
int machxo2_config(void);

/* Global Variables */
ssize_t machxo2_size;
char machxo2_str[TEXTLENGTH];
int result;
dev_t dev;

/*client's additional data*/
struct machxo2_data {
	struct i2c_client *client;
	int major;
	struct semaphore sem;
	struct cdev cdev;
} machxo2_dev;

/*machxo2 file operations structure*/
struct file_operations machxo2_fops = {
	.owner		= THIS_MODULE,
	.open		= machxo2_open,
	.release	= machxo2_release,
	.read		= machxo2_read,
	.write		= machxo2_write
};

/*The I2C driver structure*/
static const struct i2c_device_id machxo2_idtable[] = {
	{ MACHXO2_NAME, 0 },
	{ }
};

/*I2C driver sturct for the machxo2 I2C control*/
static struct i2c_driver machxo2_i2c_driver = {
	.driver = {
		.name	= MACHXO2_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= machxo2_i2c_probe,
	.remove		= machxo2_i2c_remove,
	.id_table	= machxo2_idtable,
};

/*I2C prob*/
static int machxo2_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	//TODO: the probbing bug need to fix
	int ret = -ENODEV;

	if (MACHXO2_I2C_ADDRESS == client->addr) {
		MDEBUG("Probe\n");
		ret = 0;
	}

	return ret;
}

/**
 *  machxo2_i2c_remove - machxo2 i2c remove function
 */
static int machxo2_i2c_remove(struct i2c_client *client)
{
	MDEBUG("i2c_remove\n");

	return 0;
}

/**
 *  machxo2_set_register() - Sets the given register located inside the
 *				machxo2
 *  @reg: register address
 *  @val: value to write
 *
 *  This function sets the given register located inside the machxo2.
 *  Setting is performed using I2C control line
 */
int machxo2_set_register(char reg, char val)
{
	int retval = 0;
	char reg_val_pair[] = {reg, val};

	retval = i2c_master_send(machxo2_dev.client, reg_val_pair, 2);

	if (retval < 0) {
		MDEBUG("Error setting register 0x%x\n", reg);
		return retval;
	}

	return 0;
}


/**
 *  machxo2_get_register() - gets the value of the given register located
 *				inside the machxo2
 *  @reg: register address
 *  @val: char pointer buffer used to hold the value returning
 *
 *  This function gets the curent value of the given register located inside
 *  the machxo2. Getting is performed using I2C control line
 */
int machxo2_get_register(char reg, char *val)
{
	int retval = 0;

	retval = i2c_master_send(machxo2_dev.client, &reg, 1);
	if (retval < 0) {
		MDEBUG("At machxo2_get_register 1st line\n");
		goto error;
	}
	retval = i2c_master_recv(machxo2_dev.client, val, 1);
	if (retval < 0) {
		MDEBUG("At machxo2_get_register 2nd line\n");
		goto error;
	}
	return 0;

error:
	MDEBUG("Error accessing register 0x%x\n", reg);
	return retval;
}

int machxo2_config()
{
	int retval = 0;
	char tmp;
	int i;
	char reg_default_val[13][2] = {
		{0x48, 0x00},
		{0x21, 0x09},
		{0x22, 0x01},
		{0x23, 0x80},
		{0x26, 0x00},
		{0X32, 0x16},
		{0X33, 0x00},
		{0X36, 0x10},
		{0X37, 0x00},
		{0x25, 0xFF},
		{0X30, 0x7F},
		{0X34, 0x2A},
		{0x24, 0x00}
	};

	for (i = 0; i < 13; i++) {
		if (i == 10) {/*FILTER_RESET*/
			retval = machxo2_get_register(reg_default_val[i][0], &tmp);
			MDEBUG("machxo2_get_reg try 10th %d \n", i);
		} else {
			retval = machxo2_set_register(reg_default_val[i][0],
							reg_default_val[i][1]);
			MDEBUG("machxo2_set_reg try else %d \n", i);
		}

		if (retval < 0)
			return retval;
	}

	return 0;
}

/**
 *  machxo2_init() - machxo2 init function
 */
static int machxo2_init(void)
{

	int retval = 0;
	dev_t DEV = 0;
	struct i2c_board_info info;
	struct i2c_adapter *adapter;

	MDEBUG("Starting MachXo2 driver init...\n");
	retval = i2c_add_driver(&machxo2_i2c_driver);
	if (retval) {
		MDEBUG("I2C driver registration failed\n");
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = MACHXO2_I2C_ADDRESS;
	strlcpy(info.type, MACHXO2_NAME, I2C_NAME_SIZE);
	adapter = i2c_get_adapter(MACHXO2_I2C_ID);

	if (NULL == adapter) {
		MDEBUG("Failed to get the adapter.\n");
	}

	machxo2_dev.client = i2c_new_device(adapter, &info);	/*pinctrl DT*/

	i2c_put_adapter(adapter);

		
	if (NULL == machxo2_dev.client) {
		MDEBUG("Failed to creat new i2c device.\n");
	}
	MDEBUG("Created the new i2c device.\n");

	if (MAJOR_NUM) {
		DEV = MKDEV (MAJOR_NUM, 0) ;
		retval = register_chrdev_region (DEV, 1, "MachXo2");
	}

	if (retval < 0) {
		MDEBUG("Can't get major number %d\n", MAJOR_NUM);
		return retval;
	}

	cdev_init(&machxo2_dev.cdev, &machxo2_fops);
	machxo2_dev.cdev.owner = THIS_MODULE;
	machxo2_dev.cdev.ops = &machxo2_fops;
	retval = cdev_add(&machxo2_dev.cdev, DEV, 1);

	if (retval < 0) {
		MDEBUG("Cdev registration failed.\n");
		goto fail;
	}

	MDEBUG("Major number = %d \n", MAJOR_NUM);
	sema_init(&machxo2_dev.sem, 1);
	MDEBUG("initialization complete\n");
	return 0;

fail:
	MDEBUG("init failed.\n");
	return retval;
}

/*
 * machxo2_exit - driver removing at rmmode
 * @func: function to remove the driver
 */
static void machxo2_exit(void)
{
	dev_t DEVNO = MKDEV(MAJOR_NUM, 0);

	cdev_del(&machxo2_dev.cdev);
	unregister_chrdev_region(DEVNO, 1);

	i2c_unregister_device(machxo2_dev.client);
	i2c_del_driver(&machxo2_i2c_driver);

	MDEBUG("Exit\n");
}

/*
 * machxo2_open - driver opening at runtime
 * @func: function to open the driver
 */
int machxo2_open(struct inode *inode, struct file *filp)
{
	MDEBUG("Device opened\n");
	return 0;
}

/*
 * machxo2_release - driver releasing at runtime
 * @func: function to release the driver
 */
int machxo2_release(struct inode *inode, struct file *filp)
{
	MDEBUG("Device released\n");
	return 0;
}

int machxo2_read_values(int mach_data)
{
	int retval = 0;
	int i =0;
	char val_reg_stat = 0;
	char tmp;
	int val[6];

	mach_data = 0;
	MDEBUG("machxo2_read_values calles\n");
	if (down_interruptible(&machxo2_dev.sem))
		return -ERESTARTSYS;

	do {
		MDEBUG("Went in to do while loop\n");
		retval  = machxo2_get_register(0x48, &val_reg_stat);
		if (retval < 0) {
			MDEBUG("machxo2_read_values set 1\n");
			goto exit_function;
		}
		udelay(MACHXO_READ_WAIT_TIME_US);
		i += MACHXO_READ_WAIT_TIME_US;

		if (i > MACHXO2_READ_TIMEOUT_US) {
			retval  = -ETIMEDOUT;
			goto exit_function;
		}

	} while (!(val_reg_stat & 0x48));

	for (i = 0; i < 6; i++) {
		//TODO: define 0x48
		retval = machxo2_get_register(0x48 + i, &tmp);
		if (retval < 0) 
			goto exit_function;

		val[i] = tmp;
	}

		//TODO: RET_CTRL_REG define this
	retval = machxo2_get_register(0x48, &tmp);
	i = ((tmp>>3) & 0x3);
	if (i == 3)
		i = 2;

	mach_data = ((s16)((val[1]<<8) | val[0])) * 61;
		//TODO: define this 61 bit number

exit_function:
	up(&machxo2_dev.sem);
	return retval;
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
	int retval = 0;
	int i;
	int mach_data = 0;

	for (i = 0; i < 10; i++) {
		retval = machxo2_read_values(mach_data);
		MDEBUG("I2C data: %d : 0x%x   %d\n", i, mach_data, retval);
	}

	MDEBUG("Call for read\n");
	return retval;

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
	MDEBUG("Call for write\n");

	/* Check for correct user data */
	if (buf != NULL && count != 0 && count < TEXTLENGTH) {

		/* clear current data on driver */
		memset(machxo2_str, 0, sizeof machxo2_str);

		/* send data from user */
		if (copy_from_user(machxo2_str, buf, count) != 0)
			return -EINVAL;

		*ppos = count;
		MDEBUG("Value written\n");
		return count;
	}
	MDEBUG("Value Not written\n");
	return 1;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("P.G.Amila Sampath Pelaketigamage <pgasampath@gmail.com>");
MODULE_DESCRIPTION("MACHXO2 Programming/Debugging Driver");
MODULE_DEVICE_TABLE(i2c, machxo2_idtable);
module_init(machxo2_init);
module_exit(machxo2_exit);
