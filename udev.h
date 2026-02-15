#ifndef UDEV_H
#define UDEV_H

#include "global.h"
#include "fops.h"


static int gaomon_probe(struct usb_interface *interface, const struct usb_device_id *device_id){
	struct gaomon_data *data;
	printk(KERN_INFO "usb device (%04X:%04X) plugged\n", device_id->idVendor, device_id->idProduct);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if(!data){
		//always check for null ptr
		return -ENOMEM;
	}

	kref_init(&(data->kref));

    int error_code;
    
    data->udev = usb_get_dev(interface_to_usbdev(interface));
    data->uintf = usb_get_intf(interface);

    error_code = usb_register_dev(interface, &gaomon_class);

    if(error_code) {
        printk(KERN_ALERT "Unable to register this driver. Not able to get a minor for this device. Error code %d\n", error_code);
        return error_code;
    }
    
    return error_code;
}  

static void gaomon_disconnect(struct usb_interface *interface){
    usb_deregister_dev(interface, &gaomon_class);
}

#endif
