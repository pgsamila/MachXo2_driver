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
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/i2c-dev.h>

#include "machxo2_i2c_driver.h"


#define	DEVICENAME	"MachXo2"
#define	TEXTLENGTH	100
/* The Pic Register */
#define MACHXO2_REG_CONF 	0x48
#define MACHXO2_SHUTDOWN 	0x01

MODULE_LICENSE("GPL");


/* funcitons */
static int __init machxo2_i2c_init(void);

static void __exit machxo2_i2c_cleanup(void);

static int machxo2_init(void);
static void machxo2_exit(void);
int machxo2_open(struct inode *inode, struct file *filp);
int machxo2_release(struct inode *inode, struct file *filp);
static ssize_t machxo2_read(struct file *file, char *buf, 
				size_t count, loff_t *ppos);
static ssize_t machxo2_write(struct file *file, const char *buf, 
				size_t count, loff_t *ppos);
static int machxo2_i2c_probe(struct i2c_client *client,
			     	const struct i2c_device_id *id);
static int machxo2_i2c_remove(struct i2c_client *client);
void i2c_set_clientdata(struct i2c_client *client, void *data);
void *i2c_get_clientdata(const struct i2c_client *client);
static int machxo2_i2c_read_value(struct i2c_client *client, u8 reg);
static int machxo2_i2c_write_value(struct i2c_client *client, 
				u8 reg, u16 value);
/* Addresses scaned */
static const unsigned short normal_i2c[] ={ 0x48, 0x49, 0x4a, 0x4b, 0x4c,
					0x4d, 0x4e, 0x4f, I2C_CLIENT_END };

//static struct machxo2_data *machxo2_update_device(struct device *dev);
static const u8 MACHXO2_REG_TEMP[3] ={
	0x00,		/* input */
	0x03,		/* max */
	0x02,		/* hyst */
};

/* Each client has this additional data */
struct machxo2_data {
	struct i2c_client	*client;
	struct device		*hwmon_dev;
	struct thermal_zone_device	*tz;
	struct mutex		update_lock;
	u8			orig_conf;
	u8			resolution;	/* In bits, between 9 and 12 */
	u8			resolution_limits;
	char			valid;		/* !=0 if registers are valid */
	unsigned long		last_updated;	/* In jiffies */
	unsigned long		sample_time;	/* In jiffies */
	s16			temp[3];	/* Register values,*/				  
};

/* The I2C driver structure */ 
static const struct i2c_device_id machxo2_idtable[] = {
	{ "adt75", adt75, },
	{ "ds1775", ds1775, },
	{ "ds75", ds75, },
	{ "ds7505", ds7505, },
	{ "g751", g751, },
	{ "lm75", lm75, },
	{ "lm75a", lm75a, },
	{ "max6625", max6625, },
	{ "max6626", max6626, },
	{ "mcp980x", mcp980x, },
	{ "stds75", stds75, },
	{ "tcn75", tcn75, },
	{ "tmp100", tmp100, },
	{ "tmp101", tmp101, },
	{ "tmp105", tmp105, },
	{ "tmp175", tmp175, },
	{ "tmp275", tmp275, },
	{ "tmp75", tmp75, },
	{ /* LIST END */ }
};

/* I2C driver sturct */
static struct i2c_driver machxo2_i2c_driver = {
	.driver = {
		.name	= "machxo2",
		//.pm	= &machxo2_pm_ops,	
	},

	.id_table	= machxo2_idtable,
	.probe		= machxo2_i2c_probe,
	.remove		= machxo2_i2c_remove,
/*
	.class		= I2C_CLASS_SOMETHING,
	.detect		= machxo2_detect,
	.address_list	= normal_i2c,

	.shutdown	= machxo2_shutdown,	
	.command	= machxo2_command,*/
};

static inline long machxo2_reg_to_mc(s16 temp, u8 resolution)
{
	return ((temp >> (16 - resolution)) * 1000) >> (resolution - 8);
}

static int machxo2_i2c_read_value(struct i2c_client *client, u8 reg)
{
	if (reg == MACHXO2_REG_CONF)
		return i2c_smbus_read_byte_data(client, reg);
	else
		return i2c_smbus_read_word_swapped(client, reg);
}

static int machxo2_i2c_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if (reg == MACHXO2_REG_CONF)
		return i2c_smbus_write_byte_data(client, reg, value);
	else
		return i2c_smbus_write_word_swapped(client, reg, value);
}

/* device probe and removal */
/* I2C prob */
static int machxo2_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct machxo2_data *data;
	int status;
	u8 set_mask, clr_mask;
	int new;
	enum lm75_type kind = id->driver_data;
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA))
	return -EIO;

	/* GPL LICESNS ISSUE */
	data = devm_kzalloc(dev, sizeof(struct machxo2_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	/* Set to Machxo2 resolution (9 bits, 1/2 degree C) and range.
	 * Then tweak to be more precise when appropriate.
	 */
	set_mask = 0;
	clr_mask = MACHXO2_SHUTDOWN;

	/* Switch Case set */
	switch (kind) {
	case adt75:
		clr_mask |= 1 << 5;		/* not one-shot mode */
		data->resolution = 12;
		data->sample_time = HZ / 8;
		break;
	case ds1775:
	case ds75:
	case stds75:
		clr_mask |= 3 << 5;
		set_mask |= 2 << 5;		/* 11-bit mode */
		data->resolution = 11;
		data->sample_time = HZ;
		break;
	case ds7505:
		set_mask |= 3 << 5;		/* 12-bit mode */
		data->resolution = 12;
		data->sample_time = HZ / 4;
		break;
	case g751:
	case lm75:
	case lm75a:
		data->resolution = 9;
		data->sample_time = HZ / 2;
		break;
	case max6625:
		data->resolution = 9;
		data->sample_time = HZ / 4;
		break;
	case max6626:
		data->resolution = 12;
		data->resolution_limits = 9;
		data->sample_time = HZ / 4;
		break;
	case tcn75:
		data->resolution = 9;
		data->sample_time = HZ / 8;
		break;
	case mcp980x:
		data->resolution_limits = 9;
		/* fall through */
	case tmp100:
	case tmp101:
		set_mask |= 3 << 5;		/* 12-bit mode */
		data->resolution = 12;
		data->sample_time = HZ;
		clr_mask |= 1 << 7;		/* not one-shot mode */
		break;
	case tmp105:
	case tmp175:
	case tmp275:
	case tmp75:
		set_mask |= 3 << 5;		/* 12-bit mode */
		clr_mask |= 1 << 7;		/* not one-shot mode */
		data->resolution = 12;
		data->sample_time = HZ / 2;
		break;
	}

	status = machxo2_i2c_read_value(client, MACHXO2_REG_CONF);
	if (status < 0) {
		dev_dbg(dev, "Can't read config? %d\n", status);
		return status;
	}
	data->orig_conf = status;
	new = status & ~clr_mask;
	new |= set_mask;
	if (status != new)
		machxo2_i2c_write_value(client, MACHXO2_REG_CONF, new);
	dev_dbg(dev, "Config %02x\n", new);

	printk(KERN_ALERT "MachXo2: Prob Call\n");
	return 0;
}


/* I2C prob remove */
static int machxo2_i2c_remove(struct i2c_client *client)
{
	//i2c_del_driver(&machxo2_i2c_driver);
	printk(KERN_ALERT "MachXo2: Prob Removed Call\n");
	return 0;
}

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

static int __init machxo2_i2c_init(void)
{
	return i2c_add_driver(&machxo2_i2c_driver);
	printk(KERN_ALERT "MachXo2: I2C registered\n");
}


static void __exit machxo2_i2c_cleanup(void)
{
	i2c_del_driver(&machxo2_i2c_driver);
	printk(KERN_ALERT "MachXo2: I2C uregistered\n");
}





/*
 * machxo2_init - driver initializing at insmode
 * @func: function to initialize the driver
 */
static int machxo2_init(void)
{

	if(i2c_add_driver(&machxo2_i2c_driver)!=0){
		printk(KERN_ALERT "MachXo2: I2C Not registered\n");
	}else{
		printk(KERN_ALERT "MachXo2: I2C registered\n");
	}
	/* register a range of char device numbers */
	result = alloc_chrdev_region(&dev, minor, numofdevice,
		DEVICENAME); 
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
	//machxo2_i2c_read_value(struct i2c_client *client, u8 reg);
	
	return 0;
}

/*
 * machxo2_exit - driver removing at rmmode
 * @func: function to remove the driver
 */
static void machxo2_exit(void)
{
	i2c_del_driver(&machxo2_i2c_driver);

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
	//i2c_add_driver(&machxo2_driver);
        printk(KERN_ALERT "MachXo2: device opened\n");
        return 0;
}

/*
 * machxo2_release - driver releasing at runtime
 * @func: function to release the driver
 */
int machxo2_release(struct inode *inode, struct file *filp)
{
	//i2c_del_driver(&machxo2_driver);
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
	
	//*i2c_get_clientdata(const struct i2c_client *client);
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
/* I2C macro */
module_init(machxo2_i2c_init);
module_exit(machxo2_i2c_cleanup);

MODULE_DEVICE_TABLE(i2c, machxo2_idtable);
//module_init(machxo2_init);
//module_i2c_driver(machxo2_i2c_driver);
//module_exit(machxo2_exit);

