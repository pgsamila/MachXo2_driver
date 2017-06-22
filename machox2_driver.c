#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/errno.h>

MODULE_LICENSE("FOSS/OH");

#define DEVICENAME "MachXo2"
#define TEXTLENGTH 100

char *machxo2_str[TEXTLENGTH];

int machxo2_open(struct inode *inode, struct file *filp);
int machxo2_release(struct inode *inode, struct file *filp);
int machxo2_read(struct file * file, char * buf, size_t count, loff_t *ppos);
int machxo2_write(struct file * file, char * buf, size_t count, loff_t *ppos);

int result;
int major;
int minor = 0;
int numofdevice = 1;
dev_t dev;
struct cdev cdev;

static const struct file_operations machxo2_fops = {
	.owner 	= THIS_MODULE,
	.open	= machxo2_open,
	.release= machxo2_release,
	.read	= machxo2_read,
	.write	= machxo2_write,
};

static int machxo2_init(void)
{
	result = alloc_chrdev_region(&dev, minor, numofdevice, DEVICENAME); 
	if(result < 0){
		printk(KERN_ALERT "MachXo2: did not registered\n");
		return result;
	}
	major = MAJOR(dev);
	printk(KERN_ALERT "MachXo2: registered to major : %d\n",major);
	cdev_init(&cdev, &machxo2_fops);
	result = cdev_add(&cdev, dev, numofdevice);
	if(result < 0){
		printk(KERN_ALERT "MachXo2: failed to add cdev\n");
		return result;
	}

	printk(KERN_ALERT "MachXo2: initialization complete\n");
	return 0;
}

static void machxo2_exit(void)
{
	cdev_del(&cdev);
	unregister_chrdev_region(dev, numofdevice);
	printk(KERN_ALERT "MachXo2: exit called\n");
}

int machxo2_open(struct inode *inode, struct file *filp)
{
        printk(KERN_ALERT "MachXo2: device opened\n");
        return 0;
}

int machxo2_release(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "MachXo2: device released\n");
	return 0;
}

int machxo2_read(struct file * file, char * buf, size_t count, loff_t *ppos)
{
	printk(KERN_ALERT "MachXo2: call for read\n");
	if(machxo2_str != NULL && count != 0 && count < TEXTLENGTH ){
        	if(copy_to_user(buf, machxo2_str, count))
                	return -EINVAL;	
		*ppos = count;
        	return count;
	}
}

int machxo2_write(struct file * file, char * buf, size_t count, loff_t *ppos)
{
	printk(KERN_ALERT "MachXo2: call for write\n");
	if(machxo2_str == NULL && buf != NULL && count != 0 && count < TEXTLENGTH){
	        if(copy_from_user(machxo2_str,buf, count))
                	return -EINVAL;
        	*ppos = count;
	 	return count;
	}  
}

module_init(machxo2_init);
module_exit(machxo2_exit);
