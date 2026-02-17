#include "usb_driver.h"
#include <linux/usb.h>

#define VENDOR_ID  0x256c
#define PRODUCT_ID 0x006e
#define MINOR_BASE 192

// gaomon: vendorid=256c productid=006e

static unsigned int my_device;
static struct cdev my_char_device;

static struct usb_device_id my_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};

static struct file_operations my_usb_fops = {
	.read = my_usb_read,
	.owner = THIS_MODULE,
};

static struct usb_class_driver my_usb_class = {
	.name =		"my_usb%d",
	.fops =		&my_usb_fops,
	.minor_base =	MINOR_BASE,
};

MODULE_DEVICE_TABLE(usb, my_id_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id){
	//second argument points to entry in my_id_table
	printk(KERN_INFO "%s - usb probe function\n", DRIVER_NAME);

	int error_code;

	error_code = usb_register_dev(intf, &my_usb_class);

	if(error_code) {
		printk(KERN_ALERT "Unable to register this driver. Not able to get a minor for this device. Error code %d\n", error_code);
		return error_code;
	}

	dev_info(&intf->dev,
			"USB device now attached to USB-%d",
			intf->minor);

	return error_code;

	return 0;
}

static void my_usb_disconnect(struct usb_interface *intf){
	printk(KERN_INFO "%s - usb disconnect function\n", DRIVER_NAME);
	usb_deregister_dev(intf, &my_usb_class);
}

static struct usb_driver my_usb_driver = {
	.name = "%s",
	.id_table = my_id_table,
	.probe = my_usb_probe,
	.disconnect = my_usb_disconnect,
};

static ssize_t my_usb_read(struct file *file, char *buffer, size_t count, loff_t *ppos){
	if(buffer != NULL){
		for(int i = 0; i < count; i++){
			printk(KERN_INFO "%02x", buffer + i);
		}
	}
	return 0;
}


static int __init usb_driver_init(void){
	printk(KERN_INFO "%s - Hello!", DRIVER_NAME);

	int retval;
	if ((retval = alloc_chrdev_region(&my_device, MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
		printk(KERN_ALERT "%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, retval);
		return retval;
	}

	int	gaomon_major_no = MAJOR(my_device);
	int gaomon_minor_no = MINOR(my_device);

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	printk(KERN_INFO "This line is a test. To talk to\n");
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d %d'.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no); printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n"); printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	retval = usb_register(&my_usb_driver);

	if(retval){
		printk(KERN_ALERT "%s - Error during register. Error code %d.\n", DRIVER_NAME, retval);
		return -retval;
	}

	printk(KERN_INFO "%s - USB driver registered successfully!.\n", DRIVER_NAME); 
	return 0;
}


static void __exit usb_driver_exit(void){
	printk(KERN_INFO "%s - Goodbye!", DRIVER_NAME);

	usb_deregister(&my_usb_driver);
	unregister_chrdev_region(my_device, 1);
}

module_init(usb_driver_init);
module_exit(usb_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("Test USB driver for my Gaomon graphics tablet");
