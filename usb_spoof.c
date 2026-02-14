#include "usb_spoof.h"

static int __init usb_spoof_init(void){
	pr_info("Hello, world!\n");

	int ret;

	ret = alloc_chrdev_region(&USB_dev, 0, 1, DRIVER_NAME);

	if(ret){ return ret; }

	USB_major_no = MAJOR(USB_dev);
	USB_minor_no = MINOR(USB_dev);

	cdev_init(&USB_cdev, &fops);

	ret = cdev_add(&USB_cdev, USB_dev, 1);

	if(ret){
	       	printk(KERN_ALERT "error with adding char device to system: %d.\n", ret); 
		return ret;
       	}
	

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, USB_major_no, USB_minor_no);	
	printk(KERN_INFO "To talk to\n");
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DRIVER_NAME, USB_major_no);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");


	return 0;
}
static void __exit usb_spoof_exit(void){
	printk(KERN_INFO "Goodbye!");
	unregister_chrdev_region(USB_dev, 0);
}

static int USB_open(struct inode *inode, struct file *filp){
	if(USB_device_open){
		return -EBUSY;
	}

	USB_device_open = 1;

	sprintf(msg, "If you can see this message, it worked!");
	msg_ptr = msg;
	try_module_get(THIS_MODULE);
	
	return 0;
}

static int USB_release(struct inode *inode, struct file *filp){
	USB_device_open = 0;

	module_put(THIS_MODULE);

	return 0;
}	

static ssize_t USB_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;

	if(msg_ptr == NULL){
		return -1;
	}

	if(*msg_ptr == 0){
		return 0;
	}

	while(length && *msg_ptr){
		put_user(*(msg_ptr++), buffer++);

		length--;
		bytes_read++;
	}

	printk(KERN_INFO "Message received: ", msg);

	return bytes_read;
}
		
static ssize_t USB_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}

module_init(usb_spoof_init);
module_exit(usb_spoof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("A basic USB spoofer. Will print out bytes received from USB.\n");
