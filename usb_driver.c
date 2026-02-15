#include "usb_driver.h"

static int __init usb_driver_init(void){
	printk(KERN_INFO "Hello!");

	return 0;
}
static void __exit usb_driver_exit(void){
	printk(KERN_INFO "Goodbye!");
}

module_init(usb_driver_init);
module_exit(usb_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("Test USB driver for my XBox controller");
