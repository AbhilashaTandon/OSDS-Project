#include "usb_driver.h"
#include <linux/usb.h>

#define VENDOR_ID  0x256c
#define PRODUCT_ID 0x006e
#define MINOR_BASE 192

// gaomon: vendorid=256c productid=006e

static struct usb_device_id gaomon_id_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};

static struct file_operations gaomon_fops = {
	.read = gaomon_read,
	.write = gaomon_write,
	.owner = THIS_MODULE,
};

static struct usb_class_driver gaomon_class_driver = {
	.name =		"gaomon%d",
	.fops =		&gaomon_fops,
	.minor_base =	MINOR_BASE,
};

MODULE_DEVICE_TABLE(usb, gaomon_id_table);

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id){
	//second argument points to entry in my_id_table
	printk(KERN_INFO "%s - usb probe function\n", DRIVER_NAME);

	int error_code;

	error_code = usb_register_dev(intf, &gaomon_class_driver);

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

static void gaomon_disconnect(struct usb_interface *intf){
	printk(KERN_INFO "%s - usb disconnect function\n", DRIVER_NAME);
	usb_deregister_dev(intf, &gaomon_class_driver);
}

static struct usb_driver gaomon_driver = {
	.name = "%s",
	.id_table = gaomon_id_table,
	.probe = gaomon_probe,
	.disconnect = gaomon_disconnect,
};

static ssize_t gaomon_read(struct file *file, char *buffer, size_t count, loff_t *ppos){
	if(buffer != NULL){
		for(int i = 0; i < count; i++){
			printk(KERN_INFO "%02x", buffer + i);
		}
	}
	return 0;
}

static ssize_t gaomon_write(struct file *filp, const char __user *buff,size_t len, loff_t *off){
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/chardev"
 */
static int device_open(struct inode *inode, struct file *file)
{
	static int counter = 0;

	if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
		return -EBUSY;

	sprintf(msg, "I already told you %d times Hello world!\n", counter++);

	return 0;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
	/* We're now ready for our next caller */
	atomic_set(&already_open, CDEV_NOT_USED);

	return 0;
}

/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp, /* see include/linux/fs.h */
		char __user *buffer, /* buffer to fill with data */
		size_t length, /* length of the buffer */
		loff_t *offset)
{
	/* Number of bytes actually written to the buffer */
	int bytes_read = 0;
	const char *msg_ptr = msg;

	if (!*(msg_ptr + *offset)) { /* we are at the end of message */
		*offset = 0; /* reset the offset */
		return 0; /* signify end of file */
	}

	msg_ptr += *offset;

	/* Actually put the data into the buffer */
	while (length && *msg_ptr) {
		/* The buffer is in the user data segment, not the kernel
		 * segment so "*" assignment won't work. We have to use
		 * put_user which copies data from the kernel data segment to
		 * the user data segment.
		 */
		put_user(*(msg_ptr++), buffer++);
		length--;
		bytes_read++;
	}

	*offset += bytes_read;

	/* Most read functions return the number of bytes put into the buffer. */
	return bytes_read;
}

static int __init gaomon_driver_init(void){
	printk(KERN_INFO "%s - Hello!", DRIVER_NAME);

	int retval;
	if ((retval = alloc_chrdev_region(&gaomon_device, MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
		printk(KERN_ALERT "%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, retval);
		return retval;
	}

	gaomon_major_no = MAJOR(gaomon_device);
	gaomon_minor_no = MINOR(gaomon_device);

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	printk(KERN_INFO "This line is a test. To talk to\n");
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d %d'.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no); printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n"); printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	cdev_init(&gaomon_char_device, &gaomon_fops);
	retval = cdev_add(&gaomon_char_device, gaomon_device, 1);

	if(retval){
		printk(KERN_ALERT "Error registering char driver.\n");
		return retval;
	}

	retval = usb_register(&gaomon_driver);

	if(retval){
		printk(KERN_ALERT "%s - Error during register. Error code %d.\n", DRIVER_NAME, retval);
		return retval;
	}

	printk(KERN_INFO "%s - USB driver registered successfully!.\n", DRIVER_NAME); 
	return 0;
}


static void __exit gaomon_driver_exit(void){
	printk(KERN_INFO "%s - Goodbye!", DRIVER_NAME);

	usb_deregister(&gaomon_driver);
	unregister_chrdev_region(gaomon_device, 1);
}

module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("USB driver for my Gaomon graphics tablet");
