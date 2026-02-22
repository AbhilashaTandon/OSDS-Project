#include "gaomon_driver.h"
#include <linux/usb.h>

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

static void cleanup(void){
	if(driver_registered){
		usb_deregister(&gaomon_driver);
	}
	if(cdev_added){
		cdev_del(&gaomon_cdev);	
	}
	if(gaomon_device_no != 0){
		unregister_chrdev_region(gaomon_device_no, 1);
	}
}

static int __init gaomon_driver_init(void){
	printk(KERN_INFO "%s - Hello, world!\n", DRIVER_NAME);

	int error_code;

	error_code = alloc_chrdev_region(&gaomon_device_no, MINOR_BASE, 1, DRIVER_NAME);
	if(error_code != 0){
		printk(KERN_ALERT "%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	gaomon_major_no = MAJOR(gaomon_device_no);
	gaomon_minor_no = MINOR(gaomon_device_no);

	pr_info("%s - I'm a kernel module with major number %d and minor number %d!\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	

	cdev_init(&gaomon_cdev, &gaomon_fops);
	gaomon_cdev.owner = THIS_MODULE;
	gaomon_cdev.ops = &gaomon_fops;
	error_code = cdev_add(&gaomon_cdev, gaomon_device_no, 1);
	if(error_code != 0){
		printk(KERN_ALERT "%s - Error registering char driver.\n");
		cleanup();
		return error_code;
	}

	cdev_added = 1;

	error_code = usb_register(&gaomon_driver);

	if(error_code){
		printk(KERN_ALERT "%s - Error during usb driver register. Error code %d.\n", DRIVER_NAME, error_code);
		cleanup();
		return error_code;
	}

	driver_registered = 1;

	return 0;
}


static void __exit gaomon_driver_exit(void){
	printk(KERN_INFO "%s - Goodbye, cruel world!", DRIVER_NAME);

	cleanup();
}

module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("USB driver for my Gaomon graphics tablet");
