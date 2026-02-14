#include <linux/module.h> // needed by all modules
#include <linux/printk.h> // needed for pr_info()
#include <linux/init.h> // needed for macros
#include <linux/cdev.h> 
#define DRIVER_NAME "usb_spoofer"

static struct cdev USB_cdev;
static int USB_major_no;
static int USB_minor_no;


static int __init usb_spoof_init(void){
	pr_info("Hello, world!\n");

	dev_t dev;
	int ret;

	ret = alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME);

	if(ret){ return ret; }

	USB_major_no = MAJOR(dev);
	USB_minor_no = MINOR(dev);

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, USB_major_no, USB_minor_no);	

	unregister_chrdev_region(dev, 0);

	return 0;
}
static void __exit usb_spoof_exit(void){
}

module_init(usb_spoof_init);
module_exit(usb_spoof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("A basic USB spoofer. Will print out bytes received from USB.\n");
