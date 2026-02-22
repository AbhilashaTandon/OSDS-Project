#include "gaomon_driver.h"

static int gaomon_open(struct inode *inode, struct file *file){
	struct gaomon_data *data;
	struct usb_interface *uintf;

	int error_code = 0;

	int subminor = iminor(inode);

	uintf = usb_find_interface(&gaomon_driver, subminor);
	if(!uintf){
		printk(KERN_ALERT, "%s - Error, can't find device for minor %d.\n", DRIVER_NAME, subminor);
		return -ENODEV;
	}

	data = usb_get_intfdata(uintf);
	if(!data){
		return -ENODEV;
	}

	error_code = usb_autopm_get_interface(uintf);
	if(error_code){
		return error_code;
	}

	file->private_data = data;

	return error_code;
}

static int gaomon_release(struct inode *, struct file *);
static ssize_t gaomon_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gaomon_write(struct file *, const char __user *, size_t, loff_t *);
