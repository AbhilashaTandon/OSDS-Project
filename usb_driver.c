#include "usb_driver.h"
#include <linux/usb.h>

#define VENDOR_ID  0x045e
#define PRODUCT_ID 0x028e

static struct usb_device_id my_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};
MODULE_DEVICE_TABLE(usb, my_id_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id){
//second argument points to entry in my_id_table
	printk(KERN_INFO "usb probe function\n");
	return 0;
}

static void my_usb_disconnect(struct usb_interface *intf){
	printk(KERN_INFO "usb disconnect function\n");
}

static struct usb_driver my_usb_driver = {
	.name = "my_usb_device_driver",
	.id_table = my_id_table,
	.probe = my_usb_probe,
	.disconnect = my_usb_disconnect,
};

static int __init usb_driver_init(void){
	printk(KERN_INFO "Hello!");

	int retval;

	retval = usb_register(&my_usb_driver);

	if(retval){
		printk(KERN_ALERT "my usb driver had a error during register. Error code %d.\n", retval);
		return -retval;
	}

	return 0;
}


static void __exit usb_driver_exit(void){
	printk(KERN_INFO "Goodbye!");

	usb_deregister(&my_usb_driver);
}

module_init(usb_driver_init);
module_exit(usb_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("Test USB driver for my XBox controller");
