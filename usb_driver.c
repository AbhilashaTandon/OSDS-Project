#include "usb_spoof.h"

static int __init usb_spoof_init(void){
	printk(KERN_INFO "Hello!");

	return 0;
}
static void __exit usb_spoof_exit(void){
	printk(KERN_INFO "Goodbye!");
}


module_init(usb_spoof_init);
module_exit(usb_spoof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("Test USB driver for my XBox controller");
