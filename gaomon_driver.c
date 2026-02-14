#include "global.h"
#include "fops.h"

static int __init gaomon_driver_init(void){
	pr_info("Hello, world!\n");

	int ret;

	ret = alloc_chrdev_region(&gaomon_dev, 0, 1, DRIVER_NAME);

	if(ret){ return ret; }

	gaomon_major_no = MAJOR(gaomon_dev);
	gaomon_minor_no = MINOR(gaomon_dev);

	cdev_init(&gaomon_cdev, &fops);

	ret = cdev_add(&gaomon_cdev, gaomon_dev, 1);

	if(ret){
	       	printk(KERN_ALERT "error with adding char device to system: %d.\n", ret); 
		return ret;
       	}
	

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	printk(KERN_INFO "This line is a test. To talk to\n");
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DRIVER_NAME, gaomon_major_no); printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n"); printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");


	return 0;
}
static void __exit gaomon_driver_exit(void){
	printk(KERN_INFO "Goodbye!");
	unregister_chrdev_region(gaomon_dev, 0);
}


module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("A linux device driver for the gaomon PD1560 tablet.\n");
