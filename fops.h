#ifndef FOPS_H
#define FOPS_H

#include "global.h"
#include "udev.h"

static int gaomon_open(struct inode *inode, struct file *filp){
	int error_code = 0;

	int subminor = iminor(inode);

	gaomon_uinterface = usb_find_interface(&gaomon_udriver, subminor);
	if(!gaomon_uinterface){
		printk(KERN_ALERT "%s. Error, can't find device for minor %d.\n", __func__, subminor);
		return -ENODEV;
	}

	gaomon_udev = usb_get_intfdata(gaomon_uinterface);
	if(!gaomon_udev){
		return -ENODEV;
	}

	return error_code;
}

static int gaomon_release(struct inode *inode, struct file *filp){
	return 0;
}	

static ssize_t gaomon_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	return 0;
}
		
static ssize_t gaomon_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}

#endif
