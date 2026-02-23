#include "gaomon_driver.h"

int gaomon_open(struct inode *inode, struct file *filp){
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

	filp->private_data = data;

	return error_code;
}

int gaomon_release(struct inode *inode, struct file *filp){
	return 0;
}
ssize_t gaomon_read(struct file *filp, char __user *ext_buffer, size_t count, loff_t *f_pos){
	struct gaomon_data *data = filp->private_data;		

	//TODO: add mutex lock around this section
	size_t leftover_space = data->buffer_size - data->buffer_usage;
	if(leftover_space < count){
		return 0;
	}

	if (copy_to_user(ext_buffer,
				data->buffer + data->buffer_usage,
				count)){
		return -EFAULT;
	}
	else{
		return count;
	}
}

ssize_t gaomon_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos){
	printk(KERN_ALERT "%s - Sorry! This function isn't implemented yet.", DRIVER_NAME);
	return 0;
}
