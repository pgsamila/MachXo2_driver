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
#include <linux/delay.h>
#include <linux/slab.h> 
#include <linux/types.h> 
#include <linux/ioctl.h>
#include <linux/fcntl.h>  

#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>  

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/i2c-dev.h>

#include "machxo2_i2c_driver.h"

#define MACHXO2_NAME 	"machxo2"

#define	DEVICENAME	"machxo2"
#define	TEXTLENGTH	100
/* The Pic Register */
#define MACHXO2_REG_CONF 	0x48
#define MACHXO2_SHUTDOWN 	0x01

#define MACHXO2_I2C_ADDRESS	0x48
#define MACHXO2_OMAP_I2C_ID	1 
#define MACHXO2_REG_CTRL_REG1		0x20
#define MACHXO2_REG_STATUS_REG	0x48
#define MACHXO_READ_WAIT_TIME_US 100
#define MACHXO2_READ_TIMEOUT_US	3000000

MODULE_LICENSE("GPL");

/* global variables */
ssize_t machxo2_size;
char machxo2_str[TEXTLENGTH];
int result;
int major;
int minor = 0;
int numofdevice = 1;
dev_t dev;
//struct cdev cdev;

/* client's additional data */
struct machxo2_data {
	struct i2c_client	*client;
	int major;
	struct semaphore sem;
	struct cdev cdev;	  
}machxo2_dev;

int MAJOR_NUM = 100;

/* funcitons */
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
int machxo2_set_register(char reg, char val);
int machxo2_get_register(char reg, char *val);


/* machxo2 file operations structure */
static const struct file_operations machxo2_fops = {
	.owner	 	= THIS_MODULE,
	.open	 	= machxo2_open,
	.release	= machxo2_release,
	.read	 	= machxo2_read,
	.write	 	= machxo2_write
};

/* The I2C driver structure */ 
static const struct i2c_device_id machxo2_idtable[] = {
	{ MACHXO2_NAME, 0 },
	{ /* LIST END */ }
};

/* I2C driver sturct for the machxo2 I2C control*/
static struct i2c_driver machxo2_i2c_driver = {
	.driver = {
		.name	= MACHXO2_NAME,
		.owner	= THIS_MODULE,
		//.pm	= &machxo2_pm_ops,	
	},

	.probe		= machxo2_i2c_probe,
	.remove		= machxo2_i2c_remove,
	.id_table	= machxo2_idtable,
};



/* device probe and removal */
/* I2C prob */
static int machxo2_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	
	if(MACHXO2_I2C_ADDRESS == client->addr){
		// TODO: do any init here (put device sleep)
		ret = 0;
	}
	
	return ret;
}


/** 
 *  machxo2_i2c_remove() - machxo2 i2c remove function
 */
static int machxo2_i2c_remove(struct i2c_client *client)
{
	//i2c_del_driver(&machxo2_i2c_driver);
	printk(KERN_ALERT "MachXo2: i2c_remove\n");
	return 0;
}


/** 
 *  machxo2_set_register() - Sets the given register located inside the 
 *                           machxo2
 *  @reg:	register address 
 *  @val:	value to write
 *
 *  This function sets the given register located inside the machxo2. 
 *  Setting is performed using I2C control line
 */
int machxo2_set_register(char reg, char val)
{
	int retval = 0;
	char reg_val_pair[] = {reg,val};
    
		
	retval = i2c_master_send(machxo2_dev.client,reg_val_pair,2);
	
	if (retval <0) 
	{
		printk( KERN_ALERT " Machxo2: Error setting register 0x%x\n",reg);
		return retval;
	}
	
	return 0;
}


/** 
 *  machxo2_get_register() - gets the value of the given register located 
 *                                 inside the machxo2
 *  @reg:	register address 
 *  @val:	char pointer buffer used to hold the value returning
 *
 *  This function gets the curent value of the given register located inside 
 *  the machxo2. Getting is performed using I2C control line
 */
int machxo2_get_register(char reg, char *val)
{
	
	int retval = 0;
	
	retval = i2c_master_send(machxo2_dev.client, &reg, 1);	
	if (retval <0) { 
		printk(KERN_ALERT "Machxo2 : line 177\n");
		goto error;
	}
	retval = i2c_master_recv(machxo2_dev.client, val,1);	
	if (retval <0) {
		printk(KERN_ALERT "Machxo2 : line 182\n");
		goto error;
	}
	return 0;
	
error:
	printk(KERN_ALERT "Machxo2 : Error accessing register 0x%x\n",reg);
	return retval;
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
	
	retval = i2c_add_driver(&machxo2_i2c_driver);
	if(retval){
		printk(KERN_ALERT "MachXo2: I2C driver registration failed\n");
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = MACHXO2_I2C_ADDRESS;
	strlcpy(info.type, MACHXO2_NAME, I2C_NAME_SIZE);

	adapter = i2c_get_adapter(MACHXO2_OMAP_I2C_ID);
	if ( NULL == adapter )
	{
		printk (KERN_NOTICE "MachXo2: Failed to get the adapter.\n");
	}

	machxo2_dev.client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if(NULL == machxo2_dev.client){
		printk(KERN_NOTICE "MachXo2: Failed to cread the new i2c device.\n");
	}


	if(MAJOR_NUM){
		DEV = MKDEV ( MAJOR_NUM , 0 ) ;
		retval = register_chrdev_region ( DEV , 1 , "MachXo2" ) ;       
	}

	if(retval < 0){
		printk ( KERN_WARNING "MachXo2 : Can't get major number %d\n" , MAJOR_NUM);
		return retval;
	}

	cdev_init ( &machxo2_dev.cdev ,  &machxo2_fops ) ;
	machxo2_dev.cdev.owner = THIS_MODULE;
	machxo2_dev.cdev.ops = &machxo2_fops;
	retval = cdev_add(&machxo2_dev.cdev, DEV, 1);

	if ( retval <0 )
	{
		printk ( KERN_NOTICE "MachXo2: Cdev registration failed. \n");
		goto fail;
	}

	printk ( KERN_ALERT "MachXo2 : Major number = %d \n" , MAJOR_NUM ) ;

	printk(KERN_ALERT "MachXo2: initialization complete\n");
	//machxo2_i2c_read_value(struct i2c_client *client, u8 reg);
	return 0;

 fail: 
	printk (KERN_NOTICE "MachXo2 : init failed.\n");
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
	unregister_chrdev_region(DEVNO,1);

	i2c_unregister_device(machxo2_dev.client);
	i2c_del_driver(&machxo2_i2c_driver);

	printk(KERN_ALERT "MachXo2: exit\n");
}

/*
 * machxo2_open - driver opening at runtime
 * @func: function to open the driver
 */
int machxo2_open(struct inode *inode, struct file *filp)
{
	//i2c_add_driver(&machxo2_driver);
        printk(KERN_ALERT "MachXo2: device opened\n");
        return 0;//machxo2_enable();
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


//TODO: fix read value function to work read function 
int machxo2_read_values(int mach_data)
{
	int retval = 0;
	int i =0;
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
	//int j =0;
	char val_reg_stat = 0;
	char tmp;
	int val[6];

	mach_data = 0;
	printk(KERN_ALERT "MachXo2: machxo2_ read_ values calles\n");
/*Fail*/ //down(&machxo2_dev.sem);
	
do{
		printk(KERN_ALERT "MachXo2: went in to do while loop\n");
		retval  = machxo2_get_register(MACHXO2_REG_STATUS_REG, &val_reg_stat);
		if (retval<0){
			printk(KERN_ALERT "MachXo2: line 311\n");
			goto exit_function;
		}
		//TODO: check for udelay
		udelay(MACHXO_READ_WAIT_TIME_US);
		i+=MACHXO_READ_WAIT_TIME_US;
		//for(j=0;j<3000000;j++){
			//TODO: this is to keep a delay (make a better one)
		//}
		//i+=100;
		/* time out read */
		if (i>MACHXO2_READ_TIMEOUT_US){
			retval  = -ETIMEDOUT;
			goto exit_function;
		}
		
	}while (!(val_reg_stat & 0x10));
	
	for (i=0; i<6; i++){
		//TODO: define 0x28
		//retval = machxo2_get_register(0x28 + i,&tmp);
		if (retval<0) goto exit_function;
		
		val[i] = tmp;
	}
	
		//TODO: RET_CTRL_REG define this 
	//retval = machxo2_get_register(0x23, &tmp);
	i = ((tmp>>3) & 0x3 );
	if ( i ==3) i =2;	
	
	mach_data = ((s16)((val[1]<<8) | val[0])) * 61;	
		//TODO: define this 61 bit number 

exit_function:
	//up(&machxo2_dev.sem);
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

	for (i=0;i<10;i++){
		retval = machxo2_read_values(mach_data);
		printk( "MachXo2: I2C data: %d : 0x%x   %d\n", i, mach_data, retval);
	}

	//*i2c_get_clientdata(const struct i2c_client *client);
	printk(KERN_ALERT "MachXo2: call for read\n");
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
MODULE_DEVICE_TABLE(i2c, machxo2_idtable);
module_init(machxo2_init);
//module_i2c_driver(machxo2_i2c_driver);
module_exit(machxo2_exit);

