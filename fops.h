#ifndef FOPS_H
#define FOPS_H

#include "global.h"
#include "udev.h"

//mostly copied from /drivers/usb/usb-skeleton.c in the linux kernel

static void gaomon_free(struct kref *kref){
	struct gaomon_data *data = get_gaomon_data_from_ref(kref);

	usb_free_urb(data->input_urb);
	usb_put_intf(data->uintf);
	usb_put_dev(data->udev);
	kfree(data->input_buffer);
	kfree(data);
}

static int gaomon_open(struct inode *inode, struct file *filp){
	struct gaomon_data *data;
	struct usb_interface *uintf;

	int error_code = 0;

	int subminor = iminor(inode);

	uintf = usb_find_interface(&gaomon_udriver, subminor);
	if(!uintf){
		printk(KERN_ALERT "%s. Error, can't find device for minor %d.\n", __func__, subminor);
		return -ENODEV;
	}

	data = usb_get_intfdata(uintf);
	if(!data){
		printk(KERN_ALERT "Error, can't find data from interface.\n");
		return -ENODEV;
	}

	error_code = usb_autopm_get_interface(uintf);
	if(error_code){
		return error_code;
	}

	kref_get(&data->kref); //add to ref count

	filp->private_data = data; //save data in file

	return error_code;
}

static int gaomon_release(struct inode *inode, struct file *filp){
	struct gaomon_data *data;
	if((data = filp->private_data) == NULL){
		return -ENODEV;
	}

	int error_code = usb_autopm_get_interface(data->uintf);

	kref_put(&(data->kref), gaomon_free);

	return error_code;
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
