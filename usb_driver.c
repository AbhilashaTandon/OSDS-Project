#include "usb_driver.h"
#include <linux/usb.h>

#define VENDOR_ID  0x256c
#define PRODUCT_ID 0x006e

static struct usb_device_id my_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};
MODULE_DEVICE_TABLE(usb, my_id_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id){
//second argument points to entry in my_id_table
	printk(KERN_INFO "%s - usb probe function\n", DRIVER_NAME);
	return 0;
}

static void my_usb_disconnect(struct usb_interface *intf){
	printk(KERN_INFO "%s - usb disconnect function\n", DRIVER_NAME);
}

static struct usb_driver my_usb_driver = {
	.name = "%s",
	.id_table = my_id_table,
	.probe = my_usb_probe,
	.disconnect = my_usb_disconnect,
};

static int __init usb_driver_init(void){
	printk(KERN_INFO "%s - Hello!", DRIVER_NAME);

	int retval;

	retval = usb_register(&my_usb_driver);

	if(retval){
		printk(KERN_ALERT "%s - Error during register. Error code %d.\n", DRIVER_NAME, retval);
		return -retval;
	}

	return 0;
}


static void __exit usb_driver_exit(void){
	printk(KERN_INFO "%s - Goodbye!", DRIVER_NAME);

	usb_deregister(&my_usb_driver);
}

module_init(usb_driver_init);
module_exit(usb_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("Test USB driver for my Gaomon graphics tablet");
