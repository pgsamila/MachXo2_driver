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

static int machxo2_i2c_probe(struct i2c_client *client,
			     	const struct i2c_device_id *id);
static int machxo2_i2c_remove(struct i2c_client *client);

/* Addresses scaned */
static const unsigned short normal_i2c[] ={ 0x48, 0x49, 0x4a, 0x4b, 0x4c,
					0x4d, 0x4e, 0x4f, I2C_CLIENT_END };
/* store the value */
void i2c_set_clientdata(struct i2c_client *client, void *data);

/* retrieve the value */
void *i2c_get_clientdata(const struct i2c_client *client);
static int machxo2_i2c_read_value(struct i2c_client *client, u8 reg);
static int machxo2_i2c_write_value(struct i2c_client *client, 
				u8 reg, u16 value);
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
	s16			temp[3];	/* Register values,
						   0 = input
						   1 = max
						   2 = hyst */	
};

/* I2C driver sturct */
static struct i2c_driver machxo2_i2c_driver = {
	.driver = {
		.name	= "machxo2",
		//.pm	= &machxo2_pm_ops,	
	},

	.id_table	= machxo2_idtable,
	.probe		= machxo2_i2c_probe,
	.remove		= machxo2_i2c_remove,//machxo2_remove,
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


/* sysfs attributes for hwmon */
/*
static int machxo2_read_temp(void *dev, long *temp)
{
	struct machxo2_data *data = machxo2_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	*temp = machxo2_reg_to_mc(data->temp[0], data->resolution);

	return 0;
}*/

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

	/* THIS LINE CONTAION GPL LICESNS ISSUE */
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

	//data->hwmon_dev = hwmon_device_register_with_groups(dev, client->name,
	//						    data, lm75_groups);
	//if (IS_ERR(data->hwmon_dev))
	//	return PTR_ERR(data->hwmon_dev);

	printk(KERN_ALERT "MachXo2: Prob Call\n");
	return 0;
}


/* I2C prob remove */
static int machxo2_i2c_remove(struct i2c_client *client)
{
	i2c_del_driver(&machxo2_i2c_driver);
	printk(KERN_ALERT "MachXo2: Prob Removed Call\n");
	return 0;
}

/*
static struct machxo2_data *machxo2_update_device(struct device *dev)
{
	struct machxo2_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct machxo2_data *ret = data;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + data->sample_time)
	    || !data->valid) {
		int i;
		dev_dbg(&client->dev, "Starting machxo2 update\n");

		for (i = 0; i < ARRAY_SIZE(data->temp); i++) {
			int status;

			status = machxo2_i2c_read_value(client, MACHXO2_REG_TEMP[i]);
			if (unlikely(status < 0)) {
				dev_dbg(dev,
					"MachXo2: Failed to read value: reg %d, error %d\n",
					MACHXO2_REG_TEMP[i], status);
				ret = ERR_PTR(status);
				data->valid = 0;
				goto abort;
			}
			data->temp[i] = status;
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}

abort:
	mutex_unlock(&data->update_lock);
	return ret;
}
*/

module_i2c_driver(machxo2_i2c_driver);
//module_exit(machxo2_exit);

