#include "gaomon_driver.h"
#include <linux/usb.h>

//USB METHODS

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id){
	//second argument points to entry in my_id_table
	printk(KERN_INFO "%s - usb probe function\n", DRIVER_NAME);

	struct gaomon_data *data;
	struct usb_endpoint_descriptor *input;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if(!data){
		printk(KERN_ALERT "%s - Error: could not allocate memory for global data.\n", DRIVER_NAME);
	}

	data->udev = usb_get_dev(interface_to_usbdev(intf));
	data->uintf = usb_get_intf(intf);

	int error_code;

	error_code = usb_find_common_endpoints(intf->cur_altsetting,
			NULL, NULL, &input, NULL);

	if(error_code){
		printk(KERN_ALERT "%s - Error: could not find input interrupt endpoint.\n", DRIVER_NAME);
	}

	data->buffer_size = usb_endpoint_maxp(input);
	data->input_endpoint = input->bEndpointAddress;
	data->buffer = kmalloc(data->buffer_size, GFP_KERNEL);
	if (!data->buffer) {
		return -ENOMEM;
	}
	data->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!data->urb) {
		return -ENOMEM;
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(intf, data);

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

	struct gaomon_data *data;

	data = usb_get_intfdata(intf);

	usb_free_urb(data->urb);
	kfree(data->buffer);
	kfree(data);

}

//FOPS METHODS

static int gaomon_open(struct inode *inode, struct file *filp){

	struct gaomon_data *data;
	struct usb_interface *intf;
	int subminor;

	subminor = iminor(inode);

	intf = usb_find_interface(&gaomon_driver, subminor);
	if (!intf) {
		pr_err("%s - Error: can't find device for minor %d\n",
				DRIVER_NAME, subminor);
		return  -ENODEV;
	}

	data = usb_get_intfdata(intf);
	if (!data) {
		return -ENODEV;
	}

	int error_code = usb_autopm_get_interface(intf);

	if(!error_code){
		return error_code;
	}

	//TODO: add ref counting like usb_skeleton.c

	/* save our object in the file's private structure */
	filp->private_data = data;

	return 0;
}

static int gaomon_release(struct inode *inode, struct file *filp){

	struct gaomon_data *data;

	data = filp->private_data;
	if(data == NULL){
		return -ENODEV;
	}

	usb_autopm_put_interface(data->uintf);
	//TODO: add ref counting like usb_skeleton.c

	return 0;
}

static ssize_t gaomon_read(struct file *file, char *buffer, size_t count, loff_t *ppos){
	if(buffer == NULL){
		return 0;
	}

	if(count == 0){
		return 0;
	}

	struct gaomon_data *data;

	data = file->private_data;
	if(data == NULL){
		printk(KERN_ALERT "%s - Error: global data not saved to device file.\n", DRIVER_NAME);
		return -EFAULT;
	}

	//TODO: add concurrency stuff here later
	//we need to make sure there aren't ongoing reads while this is happening 

	size_t available_space = data->buffer_size - data->buffer_usage;
	size_t read_size = available_space < count ? available_space : count; //min
	if(read_size == 0){
		printk(KERN_ALERT "%s - Error: buffer is full.\n", DRIVER_NAME);
		return -EFAULT;
	}

	if(copy_to_user(buffer, data->buffer + data->buffer_usage, read_size)){
		printk(KERN_ALERT "%s - Error: could not copy to user space.\n", DRIVER_NAME);
		return -EFAULT;
	}
	data->buffer_usage += read_size;

	return read_size;
}

static ssize_t gaomon_write(struct file *filp, const char __user *buff,size_t len, loff_t *off){
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

//INIT AND EXIT METHODS


static int __init gaomon_driver_init(void){
	printk(KERN_INFO "%s - Hello!", DRIVER_NAME);

	int error_code;
	if ((error_code = alloc_chrdev_region(&gaomon_device, MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
		printk(KERN_ALERT "%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	gaomon_major_no = MAJOR(gaomon_device);
	gaomon_minor_no = MINOR(gaomon_device);

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	printk(KERN_INFO "This line is a test. To talk to\n");
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d %d'.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no); printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n"); printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	cdev_init(&gaomon_char_device, &gaomon_fops);
	error_code = cdev_add(&gaomon_char_device, gaomon_device, 1);

	if(error_code){
		printk(KERN_ALERT "Error registering char driver.\n");
		return error_code;
	}

	error_code = usb_register(&gaomon_driver);

	if(error_code){
		printk(KERN_ALERT "%s - Error during register. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
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
