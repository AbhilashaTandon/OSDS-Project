#include "gaomon_driver.h"

int gaomon_probe(struct usb_interface *intf, const struct usb_device_id *id){
	struct gaomon_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);

	if(!data){
		return -ENOMEM;
	}

	data->udev = usb_get_dev(interface_to_usbdev(intf));
	data->uintf = usb_get_intf(intf);
	data->buffer_size = 4096; //temporary value, find out what value the system wants later
	data->buffer = kzalloc(data->buffer_size, GFP_KERNEL);

	usb_set_intfdata(intf, data);

	int retval;

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(intf, &gaomon_class_driver);
	if (retval) {
		/* something prevented us from registering this driver */
		printk(KERN_ALERT, 
				"%s - Not able to get a minor for this device.\n", DRIVER_NAME);
		usb_set_intfdata(intf, NULL);
		return retval;
	}

	printk(KERN_INFO, "%s - Successfully probed usb device.\n", DRIVER_NAME);

	return 0;
}

void gaomon_disconnect(struct usb_interface *intf){
	struct gaomon_data *data;
	data = usb_get_intfdata(intf);
	kfree(data);

	usb_deregister_dev(intf, &gaomon_class_driver);


	printk(KERN_INFO, "%s - Successfully disconnected from usb device.\n", DRIVER_NAME);
}
