#include "gaomon_driver.h"
#include <linux/usb.h>

//USB METHODS

void cleanup(struct usb_interface *intf, const struct usb_device_id *id, struct gaomon_data *data){
	//free in reverse order

	pr_info("%s - Freeing allocations\n", DRIVER_NAME);
	if(data->urb){
		usb_free_urb(data->urb);
	}
	if(data->buffer){
		kfree(data->buffer);
	}
	if(data->uintf){
		usb_put_intf(data->uintf);
	}
	if(data->udev){
		usb_put_dev(data->udev);
	}
	kfree(data);
}

static int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id){
	//second argument points to entry in my_id_table
	pr_info("%s - Probing device\n", DRIVER_NAME);

	struct gaomon_data *data;
	struct usb_endpoint_descriptor *input;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if(!data){
		pr_alert("%s - Error: could not allocate memory for global data.\n", DRIVER_NAME);
		return -ENOMEM;
	}

	data->udev = usb_get_dev(interface_to_usbdev(intf));
	data->uintf = usb_get_intf(intf);

	int error_code = 0;

	error_code = usb_find_common_endpoints(intf->cur_altsetting,
			NULL, NULL, &input, NULL);
	//TODO: figure out how to get the other endpoint

	//print endpoint information

	if(error_code){
		pr_alert("%s - Error: could not find input interrupt endpoint. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}
	else{
		unsigned int interval_length = usb_decode_interval(input, data->udev->speed);
		pr_info("%s - Found endpoint at address %d. Operates at an interval of %dms.\n", 
			DRIVER_NAME, input->bEndpointAddress, interval_length);
	}

	data->buffer_size = BUF_LEN;
	data->input_endpoint = input;
	data->buffer = kzalloc(data->buffer_size, GFP_KERNEL);
	if (!data->buffer) {
		cleanup(intf, id, data);
		pr_alert("%s - Error: could not allocate space for data buffer.\n", DRIVER_NAME);
		return -ENOMEM;
	}
	data->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!data->urb) {
		pr_alert("%s - Error: could not allocate space for urb.\n", DRIVER_NAME);
		cleanup(intf, id, data);
		return -ENOMEM;
	}
	data->buffer_usage = 0;

	/* save our data pointer in this interface device */
	usb_set_intfdata(intf, data);

	error_code = usb_register_dev(intf, &gaomon_class_driver);

	if(error_code) {
		pr_alert("Unable to register this driver. Not able to get a minor for this device. Error code %d\n", error_code);
		cleanup(intf, id, data);
		return error_code;
	}

	dev_info(&intf->dev,
			"USB device now attached to USB-%d", intf->minor);

	return error_code;
}

static void gaomon_disconnect(struct usb_interface *intf){
	pr_info("%s - usb disconnect function\n", DRIVER_NAME);

	usb_deregister_dev(intf, &gaomon_class_driver);

	struct gaomon_data *data;

	data = usb_get_intfdata(intf);

	usb_free_urb(data->urb);
	kfree(data->buffer);
	kfree(data);
}

//FOPS METHODS

static int gaomon_open(struct inode *inode, struct file *filp){
	pr_info("%s - Opening device file.\n", DRIVER_NAME);

	struct gaomon_data *data;
	struct usb_interface *intf;
	int subminor;

	subminor = iminor(inode);

	intf = usb_find_interface(&gaomon_driver, subminor);
	if (!intf) {
		pr_err ("%s - Error: can't find device for minor %d\n",
				DRIVER_NAME, subminor);
		return  -ENODEV;
	}

	data = usb_get_intfdata(intf);
	if (!data) {
		pr_err ("%s - Can't get data from interface.\n",
				DRIVER_NAME );
		return -ENODEV;
	}

	//int error_code = usb_autopm_get_interface(intf);
	// this function is used for autosuspend/resume so I should only use it after I've implemented those

	//if(error_code){
	//pr_err ("%s - Error getting interface. Error code %d.\n", DRIVER_NAME, error_code);
	//return error_code;
	//}

	/* save our object in the file's private structure */
	filp->private_data = data;

	return 0;
}

static int gaomon_release(struct inode *inode, struct file *filp){

	struct gaomon_data *data;

	data = filp->private_data;
	if(data == NULL){
		pr_err ("%s - Global data stored in file is null pointer.\n", DRIVER_NAME);
		return -ENODEV;
	}

	usb_autopm_put_interface(data->uintf);

	return 0;
}

static void gaomon_irq_callback(struct urb *urb){

	pr_info("%s - Running urb irq callback function.\n");

	struct gaomon_data *data;

	data = urb->context;

	if(urb->status){
		pr_alert("%s - error preparing urb read %d\n", DRIVER_NAME, urb->status);
	}

	data->buffer_usage = urb->actual_length;

	//I think I need to resubmit the urb here since im using interrupts
	//TODO: add concurrency 
}

static int gaomon_read_irq(struct gaomon_data *data, size_t count){
	int error_code = 0;

	if(!(data->urb && data->udev && data->buffer && data->buffer_size)){
		pr_err("%s - Error: a structure is unallocated.\n", DRIVER_NAME);
	}

	pr_info("%s - Reading from endpoint at 0x%x.\n", DRIVER_NAME, data->input_endpoint->bEndpointAddress);
	pr_info("%s - It has a max packet size of %d.\n", DRIVER_NAME, data->input_endpoint->wMaxPacketSize);

	unsigned int pipe = usb_rcvintpipe(data->udev, data->input_endpoint->bEndpointAddress);
	
	if(error_code = usb_pipe_type_check(data->udev, pipe))
		pr_err("%s - Error: pipe has invalid format. Error code %d.\n", DRIVER_NAME, error_code);

	usb_fill_int_urb(data->urb, 
			data->udev, 
			pipe,
			data->buffer, 
			min(data->buffer_size, count),
			gaomon_irq_callback, 
			data, data->input_endpoint->bInterval); //last argument is interval measured in microseconds

	data->buffer_usage = 0;

	if(error_code = usb_urb_ep_type_check(data->urb))
		pr_err("%s - Error: urb has invalid endpoint. Error code %d.\n", DRIVER_NAME, error_code);

	error_code = usb_submit_urb(data->urb, GFP_KERNEL);
	if(error_code < 0){
		pr_err("%s - Failed submitting read urb, error %d\n", DRIVER_NAME, error_code);
	}

	return error_code;

}
//prepares a urb and submits it

static ssize_t gaomon_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos){

	pr_info("%s - Reading %d bytes from device.\n", DRIVER_NAME, count);

	int error_code = 0;
	if(buffer == NULL){
		return 0;
	}

	if(count == 0){
		return 0;
	}

	struct gaomon_data *data;

	data = file->private_data;
	if(data == NULL){
		pr_alert( "%s - Error: global data not saved to device file.\n", DRIVER_NAME);
		return -EFAULT;
	}

	//TODO: add concurrency stuff here later
	//we need to make sure there aren't ongoing reads while this is happening 

	while(true){
		if(data->buffer_usage){
			size_t available_space = data->buffer_size - data->buffer_usage;
			size_t read_size = min(available_space, count); 
			if(!available_space){
				error_code = gaomon_read_irq(data, count);
				if(error_code < 0){
					pr_alert( "%s - Error: failed sending urb. Error code %d.\n", DRIVER_NAME, error_code);
					return error_code;
				}
				else{
					continue;
				}
				//else try again
			}

			if(read_size == 0){
				pr_alert( "%s - Error: buffer is full.\n", DRIVER_NAME);
				return -EFAULT;
			}

			if(copy_to_user(buffer, data->buffer + data->buffer_usage, read_size)){
				pr_alert("%s - Error: could not copy to user space.\n", DRIVER_NAME);
				return -EFAULT;
			}
			data->buffer_usage += read_size;

			if(available_space < count){
				gaomon_read_irq(data, count - read_size);
			}
		}
		else{
			error_code = gaomon_read_irq(data, count);
			if(error_code < 0){
				pr_alert( "%s - Error: failed sending urb. Error code %d.\n", DRIVER_NAME, error_code);
				return error_code;
			}
			else{
				continue;
			}
		}
	}

	return 0;
}

static ssize_t gaomon_write(struct file *filp, const char __user *buff,size_t len, loff_t *off){
	pr_alert("Sorry, this operation is not supported.\n");
	return -EINVAL;
}

//INIT AND EXIT METHODS


static int __init gaomon_driver_init(void){
	pr_info( "%s - Hello!", DRIVER_NAME);

	int error_code;
	if ((error_code = alloc_chrdev_region(&gaomon_device, MINOR_BASE, 1,DRIVER_NAME)) < 0)	{
		pr_alert("%s - Error during allocating region. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	gaomon_major_no = MAJOR(gaomon_device);
	gaomon_minor_no = MINOR(gaomon_device);

	pr_info("I'm %s. I'm a kernel module with major number %d and minor number %d.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);	
	pr_info("This line is a test. To talk to\n");
	pr_cont("the driver, create a dev file with\n");
	pr_info("'mknod /dev/%s c %d %d'.\n", DRIVER_NAME, gaomon_major_no, gaomon_minor_no);
	pr_info("Try various minor numbers. Try to cat and echo to\n");
	pr_cont(" the device file.\n");
	pr_info("Remove the device file and module when done.\n");

	cdev_init(&gaomon_char_device, &gaomon_fops);
	error_code = cdev_add(&gaomon_char_device, gaomon_device, 1);

	if(error_code){
		pr_alert("%s - Error registering char driver. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	error_code = usb_register(&gaomon_driver);

	if(error_code){
		pr_alert("%s - Error during register. Error code %d.\n", DRIVER_NAME, error_code);
		return error_code;
	}

	pr_info("%s - USB driver registered successfully!.\n", DRIVER_NAME); 
	return 0;
}


static void __exit gaomon_driver_exit(void){
	pr_info("%s - Goodbye!", DRIVER_NAME);

	usb_deregister(&gaomon_driver);
	unregister_chrdev_region(gaomon_device, 1);
}

module_init(gaomon_driver_init);
module_exit(gaomon_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhilasha Tandon");
MODULE_DESCRIPTION("USB driver for my Gaomon graphics tablet");
